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
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

namespace tamer {
namespace {

class driver_tamer : public driver { public:

    driver_tamer();
    ~driver_tamer();

    virtual void store_fd(int fd, int action, tamerpriv::simple_event *se,
			  int *slot);
    virtual void store_time(const timeval &expiry, tamerpriv::simple_event *se);
    virtual void store_asap(tamerpriv::simple_event *se);
    virtual void kill_fd(int fd);

    virtual bool empty();
    virtual void once();
    virtual void loop();

  private:

    struct ttimer {
	timeval expiry_;
	unsigned order_;
	tamerpriv::simple_event *trigger_;
	ttimer(const timeval &expiry, unsigned order,
	       tamerpriv::simple_event *trigger)
	    : expiry_(expiry), order_(order), trigger_(trigger) {
	}
	bool operator>(const ttimer &x) const {
	    if (expiry_.tv_sec != x.expiry_.tv_sec)
		return expiry_.tv_sec > x.expiry_.tv_sec;
	    if (expiry_.tv_usec != x.expiry_.tv_usec)
		return expiry_.tv_usec > x.expiry_.tv_usec;
	    return (int) (order_ - x.order_) > 0;
	}
    };

    struct tfd {
	int fd : 30;
	unsigned action : 2;
	tamerpriv::simple_event *se;
	int *slot;
	tfd *next;
    };

    struct tfd_group {
	tfd_group *next;
	tfd t[1];
    };

    union xfd_set {
	fd_set fds;
	char s[1];
    };

    ttimer *t_;
    int nt_;
    int tcap_;
    unsigned torder_;

    tfd *_fd;
    int _nfds;
    xfd_set *_fdset[4];
    int _fdset_cap;

    tamerpriv::simple_event **asap_;
    unsigned asap_head_;
    unsigned asap_tail_;
    unsigned asap_capmask_;

    int _fdcap;
    tfd_group *_fdgroup;
    tfd *_fdfree;
    rendezvous<> _fdcancelr;

    void expand_timers();
    void check_timers() const;
    void timer_reheapify_from(int pos);
    void expand_fds();
    void cull_timers();
    void expand_asap();

};


driver_tamer::driver_tamer()
    : t_(0), nt_(0), tcap_(0), torder_(0),
      _fd(0), _nfds(0), _fdset_cap(sizeof(xfd_set) * 8),
      asap_(0), asap_head_(0), asap_tail_(0), asap_capmask_(-1U),
      _fdcap(0), _fdgroup(0), _fdfree(0)
{
    assert(FD_SETSIZE <= _fdset_cap);
    expand_timers();
    for (int i = 0; i < 4; ++i)
	_fdset[i] = reinterpret_cast<xfd_set *>(new char[_fdset_cap / 8]);
    FD_ZERO(&_fdset[fdread]->fds);
    FD_ZERO(&_fdset[fdwrite]->fds);
    set_now();
}

driver_tamer::~driver_tamer()
{
    // destroy all active timers
    for (int i = 0; i < nt_; i++)
	tamerpriv::simple_event::unuse(t_[i].trigger_);
    delete[] reinterpret_cast<char *>(t_);

    // destroy all active asaps
    while (asap_head_ != asap_tail_) {
	tamerpriv::simple_event::unuse(asap_[asap_head_ & asap_capmask_]);
	++asap_head_;
    }
    delete[] asap_;

    // destroy all active file descriptors
    while (_fd) {
	tamerpriv::simple_event::unuse(_fd->se);
	_fd = _fd->next;
    }

    // free file descriptor groups
    while (_fdgroup) {
	tfd_group *next = _fdgroup->next;
	delete[] reinterpret_cast<unsigned char *>(_fdgroup);
	_fdgroup = next;
    }

    // free fd_sets
    for (int i = 0; i < 4; ++i)
	delete[] reinterpret_cast<char *>(_fdset[i]);
}

void driver_tamer::expand_timers()
{
    int ntcap = (tcap_ ? ((tcap_ + 1) * 2 - 1) : 511);
    ttimer *nt = reinterpret_cast<ttimer *>(new char[sizeof(ttimer) * ntcap]);
    if (nt_ != 0)
	// take advantage of fact that memcpy() works on event<>
	memcpy(nt, t_, sizeof(ttimer) * nt_);
    delete[] reinterpret_cast<char *>(t_);
    t_ = nt;
    tcap_ = ntcap;
}

void driver_tamer::expand_asap()
{
    unsigned ncapmask =
	(asap_capmask_ + 1 ? ((asap_capmask_ + 1) * 4 - 1) : 31);
    tamerpriv::simple_event **na = new tamerpriv::simple_event *[ncapmask + 1];
    unsigned i = 0;
    for (unsigned x = asap_head_; x != asap_tail_; ++x, ++i)
	na[i] = asap_[x & asap_capmask_];
    delete[] asap_;
    asap_ = na;
    asap_capmask_ = ncapmask;
    asap_head_ = 0;
    asap_tail_ = i;
}

void driver_tamer::timer_reheapify_from(int pos)
{
    int npos;
    while (pos > 0
	   && (npos = (pos - 1) >> 1, t_[npos] > t_[pos])) {
	std::swap(t_[pos], t_[npos]);
	pos = npos;
    }

    while (1) {
	int smallest = pos;
	npos = 2*pos + 1;
	if (npos < nt_ && t_[smallest] > t_[npos])
	    smallest = npos;
	if (npos + 1 < nt_ && t_[smallest] > t_[npos + 1])
	    smallest = npos + 1, ++npos;
	if (smallest == pos)
	    break;
	std::swap(t_[pos], t_[smallest]);
	pos = smallest;
    }
#if 0
    if (_t + 1 < tend || !will_delete)
	_timer_expiry = tbegin[0]->expiry;
    else
	_timer_expiry = Timestamp();
#endif
}

#if 0
void driver_tamer::check_timers() const
{
    fprintf(stderr, "---");
    for (int k = 0; k < _nt; k++)
	fprintf(stderr, " %p/%d.%06d", _t[k], _t[k]->expiry.tv_sec, _t[k]->expiry.tv_usec);
    fprintf(stderr, "\n");

    for (int i = 0; i < _nt / 2; i++)
	for (int j = 2*i + 1; j < 2*i + 3; j++)
	    if (j < _nt && *_t[i] > *_t[j]) {
		fprintf(stderr, "***");
		for (int k = 0; k < _nt; k++)
		    fprintf(stderr, (k == i || k == j ? " **%d.%06d**" : " %d.%06d"), _t[k]->expiry.tv_sec, _t[k]->expiry.tv_usec);
		fprintf(stderr, "\n");
		assert(0);
	    }
}
#endif

void driver_tamer::expand_fds()
{
    int ncap = (_fdcap ? _fdcap * 2 : 16);

    tfd_group *ngroup = reinterpret_cast<tfd_group *>(new unsigned char[sizeof(tfd_group) + sizeof(tfd) * (ncap - 1)]);
    ngroup->next = _fdgroup;
    _fdgroup = ngroup;
    for (int i = 0; i < ncap; i++) {
	ngroup->t[i].next = _fdfree;
	_fdfree = &ngroup->t[i];
    }
}

void driver_tamer::store_fd(int fd, int action, tamerpriv::simple_event *se,
			    int *slot)
{
    if (!_fdfree)
	expand_fds();
    if (se && *se) {
	tfd *t = _fdfree;
	_fdfree = t->next;
	t->next = _fd;
	_fd = t;

	t->fd = fd;
	t->action = action;
	t->se = se;
	t->slot = slot;

	if (fd >= FD_SETSIZE) {
	    int ncap = _fdset_cap * 2;
	    while (ncap < fd)
		ncap *= 2;
	    for (int acti = 0; acti < 4; ++acti) {
		xfd_set *x = reinterpret_cast<xfd_set *>(new char[ncap / 8]);
		if (acti < 2) {
		    memcpy(x, _fdset[acti], _fdset_cap / 8);
		    memset(x->s + _fdset_cap / 8, 0, (ncap - _fdset_cap) / 8);
		}
		delete[] reinterpret_cast<char *>(_fdset[acti]);
		_fdset[acti] = x;
	    }
	    _fdset_cap = ncap;
	}

	FD_SET(fd, &_fdset[action]->fds);
	if (fd >= _nfds)
	    _nfds = fd + 1;
	if (action <= fdwrite)
	    tamerpriv::simple_event::at_trigger(se, make_event(_fdcancelr).__take_simple());
    }
}

void driver_tamer::kill_fd(int fd)
{
    assert(fd >= 0);
    FD_CLR(fd, &_fdset[fdread]->fds);
    FD_CLR(fd, &_fdset[fdwrite]->fds);
    tfd **pprev = &_fd, *t;
    while ((t = *pprev))
	if (t->fd == fd) {
	    if (*t->se && t->slot)
		*t->slot = -ECANCELED;
	    t->se->simple_trigger(true);
	    *pprev = t->next;
	    t->next = _fdfree;
	    _fdfree = t;
	} else
	    pprev = &t->next;
}

void driver_tamer::store_time(const timeval &expiry,
			      tamerpriv::simple_event *se)
{
    if (se) {
	if (nt_ == tcap_)
	    expand_timers();
	(void) new(static_cast<void *>(&t_[nt_])) ttimer(expiry, ++torder_, se);
	++nt_;
	timer_reheapify_from(nt_ - 1);
    }
}

void driver_tamer::store_asap(tamerpriv::simple_event *se)
{
    if (se) {
	if (asap_tail_ - asap_head_ == asap_capmask_ + 1)
	    expand_asap();
	asap_[asap_tail_ & asap_capmask_] = se;
	++asap_tail_;
    }
}

void driver_tamer::cull_timers()
{
    while (nt_ != 0 && t_[0].trigger_->empty()) {
	tamerpriv::simple_event::unuse(t_[0].trigger_);
	--nt_;
	if (nt_ != 0) {
	    t_[0] = t_[nt_];
	    timer_reheapify_from(0);
	}
    }
}

bool driver_tamer::empty()
{
    cull_timers();
    if (asap_head_ != asap_tail_
	|| nt_ != 0
	|| sig_any_active
	|| tamerpriv::abstract_rendezvous::has_unblocked()
	|| _nfds != 0)
	return false;
    return true;
}

void driver_tamer::once()
{
    // determine timeout
    cull_timers();
    struct timeval to, *toptr;
    if (asap_head_ != asap_tail_
	|| (nt_ != 0 && !timercmp(&t_[0].expiry_, &now, >))
	|| sig_any_active
	|| tamerpriv::abstract_rendezvous::has_unblocked()) {
	timerclear(&to);
	toptr = &to;
    } else if (nt_ == 0)
	toptr = 0;
    else {
	timersub(&t_[0].expiry_, &now, &to);
	toptr = &to;
    }

    // get rid of canceled descriptors, if any
    if (_fdcancelr.has_ready()) {
	while (_fdcancelr.join())
	    /* nada */;
	memset(_fdset[fdread]->s, 0, ((_nfds + 63) & ~63) >> 3);
	memset(_fdset[fdwrite]->s, 0, ((_nfds + 63) & ~63) >> 3);
	_nfds = 0;
	tfd **pprev = &_fd, *t;
	while ((t = *pprev))
	    if (!*t->se) {
		tamerpriv::simple_event::unuse_clean(t->se);
		*pprev = t->next;
		t->next = _fdfree;
		_fdfree = t;
	    } else {
		FD_SET(t->fd, &_fdset[t->action]->fds);
		if (t->fd >= _nfds)
		    _nfds = t->fd + 1;
		pprev = &t->next;
	    }
    }

    // select!
    int nfds = _nfds;
    if (nfds > 0 || sig_pipe[0] >= 0) {
	memcpy(_fdset[fdread + 2], _fdset[fdread], ((nfds + 63) & ~63) >> 3);
	memcpy(_fdset[fdwrite + 2], _fdset[fdwrite], ((nfds + 63) & ~63) >> 3);
	if (sig_pipe[0] >= 0) {
	    FD_SET(sig_pipe[0], &_fdset[fdread + 2]->fds);
	    if (sig_pipe[0] > nfds)
		nfds = sig_pipe[0] + 1;
	}
	nfds = select(nfds, &_fdset[fdread + 2]->fds,
		      &_fdset[fdwrite + 2]->fds, 0, toptr);
    }

    // run signals
    if (sig_any_active)
	dispatch_signals();

    // run asaps
    while (asap_head_ != asap_tail_) {
	tamerpriv::simple_event *se = asap_[asap_head_ & asap_capmask_];
	++asap_head_;
	se->simple_trigger(false);
    }

    // run file descriptors
    if (nfds > 0) {
	tfd **pprev = &_fd, *t;
	while ((t = *pprev))
	    if (t->action <= fdwrite
		&& (t->fd >= FD_SETSIZE
		    || FD_ISSET(t->fd, &_fdset[t->action + 2]->fds))) {
		FD_CLR(t->fd, &_fdset[t->action]->fds);
		if (*t->se && t->slot)
		    *t->slot = 0;
		t->se->simple_trigger(true);
		_fdcancelr.join(); // reap the notifier we just triggered
		*pprev = t->next;
		t->next = _fdfree;
		_fdfree = t;
	    } else if (!*t->se) {
		tamerpriv::simple_event::unuse_clean(t->se);
		*pprev = t->next;
		t->next = _fdfree;
		_fdfree = t;
	    } else
		pprev = &t->next;
    }

    // run the timers that worked
    if (nt_ != 0) {
	set_now();
	while (nt_ != 0 && !timercmp(&t_[0].expiry_, &now, >)) {
	    tamerpriv::simple_event *trigger = t_[0].trigger_;
	    --nt_;
	    if (nt_ != 0) {
		t_[0] = t_[nt_];
		timer_reheapify_from(0);
	    }
	    trigger->simple_trigger(false);
	}
    }

    // run active closures
    while (tamerpriv::abstract_rendezvous *r = tamerpriv::abstract_rendezvous::pop_unblocked())
	r->run();
}

#if 0
void driver_tamer::print_fds()
{
    tfd *t = _fd;
    while (t) {
	fprintf(stderr, "%d.%d ", t->fd, t->action);
	t = t->next;
    }
    if (_fd)
	fprintf(stderr, "\n");
}
#endif

void driver_tamer::loop()
{
    while (1)
	once();
}

}

driver *driver::make_tamer()
{
    return new driver_tamer;
}

}
