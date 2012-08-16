// -*- mode: c++; tab-width: 8; c-basic-offset: 4;  -*-

/* Copyright (c) 2007-2012, Eddie Kohler
 * Copyright (c) 2007, Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Tamer LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Tamer LICENSE file; the license in that file is
 * legally binding.
 */
#include "config.h"
#include <tamer/tamer.hh>
#include <tamer/util.hh>
#if HAVE_LIBEV
#include <ev.h>
#endif

namespace tamer {
#if HAVE_LIBEV
namespace {

class driver_libev: public driver
{
public:

    driver_libev();
    ~driver_libev();

    virtual void store_fd(int fd, int action, tamerpriv::simple_event *se,
                          int *slot);
    virtual void store_time(const timeval &expiry, tamerpriv::simple_event *se);
    virtual void kill_fd(int fd);

    virtual bool empty();
    virtual void once();
    virtual void loop();

    struct eev : public tamerutil::dlist_element {
	::event libevent;
	tamerpriv::simple_event *se;
	int *slot;
	driver_libev *driver;
    };

    struct eev_group : public tamerutil::slist_element {
	eev e[1];
    };

    struct ev_loop *_eloop;

    eevent *_etimer;
    eevent *_efd;

    tamer::dlist<eev> _efree;
    tamer::slist<eev_group> _egroups;

    size_t _ecap;
    eev *_esignal;

private:
    eev *get_eev();
    void expand_eevs ();

};


extern "C" {
void libevent_trigger(int, short, void *arg)
{
    driver_libevent::eevent *e = static_cast<driver_libevent::eevent *>(arg);
    if (*e->se && e->slot)
	*e->slot = 0;
    e->se->simple_trigger(true);
    *e->pprev = e->next;
    if (e->next)
	e->next->pprev = e->pprev;
    e->next = e->driver->_efree;
    e->driver->_efree = e;
}

void libevent_sigtrigger(int, short, void *arg)
{
    driver_libevent::eevent *e = static_cast<driver_libevent::eevent *>(arg);
    e->driver->dispatch_signals();
}
}


driver_libev::driver_libev()
    : _eloop (ev_default_loop(0))
{
    set_now();
    at_signal(0, event<>());	// create signal_fd pipe
    _esignal = get_eev()
    ::event_set(&_esignal->libevent, sig_pipe[0], EV_READ | EV_PERSIST,
		libevent_sigtrigger, 0);
    ::event_priority_set(&_esignal->libevent, 0);
    ::event_add(&_esignal->libevent, 0);
}

driver_libevent::~driver_libevent()
{
    // discard all active events
    while (_etimer) {
	_etimer->se->simple_trigger(false);
	::event_del(&_etimer->libevent);
	_etimer = _etimer->next;
    }
    while (_efd) {
	_efd->se->simple_trigger(false);
	if (_efd->libevent.ev_events)
	    ::event_del(&_efd->libevent);
	_efd = _efd->next;
    }
    ::event_del(&_esignal->libevent);

    // free event groups
    while (!_egroups.empty()) {
	delete[] reinterpret_cast<unsigned char *>(_egroups.pop_front());
    }
}

eev *
driver_libev::get_eev()
{
    eev *ret = _efree.pop_front();
    if (!ret) {
	expand_eevs();
    }
    ret = _efree.pop_front();
    return ret;
}

void driver_libev::expand_eevs()
{
    size_t ncap = (_ecap ? _ecap * 2 : 16);

    // Allocate space ncap of them
    size_t sz = sizeof(eev_group) + sizeof(eev) * (ncap - 1);

    eev_group *ngroup = reinterpret_cast<eev_group *>(new unsigned char[sz]);
    _egroups.push_front (ngroup);

    // Use placement new to call ncap constructors....
    new (ngroup->e) eev[ncap];

    for (size_t i = 0; i < ncap; i++) {
	eev *e = &ngroup->e[i];
	e->driver = this;
	_efree.push_front (e);
    }
}

void driver_libevent::store_fd(int fd, int action,
			       tamerpriv::simple_event *se, int *slot)
{
    assert(fd >= 0);
    if (!_efree)
	expand_events();
    if (se) {
	eevent *e = _efree;
	_efree = e->next;
	event_set(&e->libevent, fd, (action == fdwrite ? EV_WRITE : EV_READ),
		  libevent_trigger, e);
	event_add(&e->libevent, 0);

	e->se = se;
	e->slot = slot;
	e->next = _efd;
	e->pprev = &_efd;
	if (_efd)
	    _efd->pprev = &e->next;
	_efd = e;
    }
}

void driver_libevent::kill_fd(int fd)
{
    eevent **ep = &_efd;
    for (eevent *e = *ep; e; e = *ep)
	if (e->libevent.ev_fd == fd) {
	    event_del(&e->libevent);
	    if (*e->se && e->slot)
		*e->slot = -ECANCELED;
	    e->se->simple_trigger(true);
	    *ep = e->next;
	    if (*ep)
		(*ep)->pprev = ep;
	    e->next = _efree;
	    _efree = e;
	} else
	    ep = &e->next;
}

void driver_libevent::store_time(const timeval &expiry,
				 tamerpriv::simple_event *se)
{
    if (!_efree)
	expand_events();
    if (se) {
	eevent *e = _efree;
	_efree = e->next;

	evtimer_set(&e->libevent, libevent_trigger, e);
	timeval timeout = expiry;
	timersub(&timeout, &now, &timeout);
	evtimer_add(&e->libevent, &timeout);

	e->se = se;
	e->slot = 0;
	e->next = _etimer;
	e->pprev = &_etimer;
	if (_etimer)
	    _etimer->pprev = &e->next;
	_etimer = e;
    }
}

bool driver_libevent::empty()
{
    // remove dead events
    while (_etimer && !*_etimer->se) {
	eevent *e = _etimer;
	::event_del(&e->libevent);
	tamerpriv::simple_event::unuse_clean(_etimer->se);
	if ((_etimer = e->next))
	    _etimer->pprev = &_etimer;
	e->next = _efree;
	_efree = e;
    }
    while (_efd && !*_efd->se) {
	eevent *e = _efd;
	if (e->libevent.ev_events)
	    ::event_del(&e->libevent);
	tamerpriv::simple_event::unuse_clean(_etimer->se);
	if ((_efd = e->next))
	    _efd->pprev = &_efd;
	e->next = _efree;
	_efree = e;
    }

    if (_etimer || _efd
	|| sig_any_active || tamerpriv::abstract_rendezvous::has_unblocked())
	return false;
    return true;
}

void driver_libevent::once()
{
    if (tamerpriv::abstract_rendezvous::has_unblocked())
	::event_loop(EVLOOP_ONCE | EVLOOP_NONBLOCK);
    else
	::event_loop(EVLOOP_ONCE);
    set_now();
    while (tamerpriv::abstract_rendezvous *r = tamerpriv::abstract_rendezvous::pop_unblocked())
	r->run();
}

void driver_libevent::loop()
{
    while (1)
	once();
}

}

driver *driver::make_libevent()
{
    return new driver_libevent;
}

#else

driver *driver::make_libevent()
{
    return 0;
}

#endif
}
