#ifndef TAMER_RENDEZVOUS_HH
#define TAMER_RENDEZVOUS_HH 1
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
#include <cassert>
#include <tamer/xbase.hh>
namespace tamer {

/** @file <tamer/rendezvous.hh>
 *  @brief  The rendezvous template classes.
 */

/** @class rendezvous tamer/event.hh <tamer/tamer.hh>
 *  @brief  A set of watched events.
 *
 *  Rendezvous may also be declared with one or zero template arguments, as in
 *  <tt>rendezvous<T0></tt> or <tt>rendezvous<></tt>.  Each specialized
 *  rendezvous class has functions similar to the full-featured rendezvous,
 *  but with parameters to @c join appropriate to the template arguments.
 *  Specialized rendezvous implementations are often more efficient than the
 *  full @c rendezvous.
 */
template <typename I0, typename I1>
class rendezvous : public tamerpriv::explicit_rendezvous { public:

    /** @brief  Construct rendezvous.
     *  @param  flags  tamer::rnormal (default) or tamer::rvolatile
     *
     *  Pass tamer::rvolatile as the @a flags parameter to create a volatile
     *  rendezvous.  Volatile rendezvous do not generate error messages when
     *  their events are dereferenced before trigger. */
    inline rendezvous(rendezvous_flags flags = rnormal);

    /** @brief  Destroy rendezvous. */
    inline ~rendezvous();

    /** @brief  Test if any events are ready.
     *
     *  An event is ready if it has triggered, but no @a join or @c twait
     *  has reported it yet.
     */
    inline bool has_ready() const {
	return ready_;
    }

    /** @brief  Test if any events are waiting.
     *
     *  An event is waiting until it is either triggered or canceled.
     */
    inline bool has_waiting() const {
	return waiting_;
    }

    /** @brief  Test if any events are ready or waiting.
     */
    inline bool has_events() const {
	return has_ready() || has_waiting();
    }

    /** @brief  Report the next ready event.
     *  @param[out]  i0  Set to the first event ID of the ready event.
     *  @param[out]  i1  Set to the second event ID of the ready event.
     *  @return  True if there was a ready event, false otherwise.
     *
     *  @a i0 and @a i1 were modified if and only if @a join returns true.
     */
    inline bool join(I0 &i0, I1 &i1);

    /** @brief  Remove all events from this rendezvous.
     *
     *  Every event waiting on this rendezvous is made empty.  After clear(),
     *  has_events() will return false.
     */
    void clear();

    /** @internal
     *  @brief  Add an occurrence to this rendezvous.
     *  @param  e   The occurrence.
     *  @param  i0  The occurrence's first event ID.
     *  @param  i1  The occurrence's first event ID.
     */
    inline void add(tamerpriv::simple_event *e, const I0 &i0, const I1 &i1);

  private:

    struct eventid {
	I0 i0;
	I1 i1;
	eventid(I0 i0_, I1 i1_)
	    : i0(TAMER_MOVE(i0_)), i1(TAMER_MOVE(i1_)) {
	}
    };

};

template <typename I0, typename I1>
inline rendezvous<I0, I1>::rendezvous(rendezvous_flags flags)
    : explicit_rendezvous(flags)
{
}

template <typename I0, typename I1>
inline rendezvous<I0, I1>::~rendezvous()
{
    if (waiting_ || ready_)
	clear();
}

template <typename I0, typename I1>
inline void rendezvous<I0, I1>::add(tamerpriv::simple_event *e, const I0 &i0, const I1 &i1)
{
    eventid *eid = new eventid(i0, i1);
    e->initialize(this, reinterpret_cast<uintptr_t>(eid));
}

template <typename I0, typename I1>
inline bool rendezvous<I0, I1>::join(I0 &i0, I1 &i1)
{
    if (ready_) {
	eventid *eid = reinterpret_cast<eventid *>(pop_ready());
	i0 = TAMER_MOVE(eid->i0);
	i1 = TAMER_MOVE(eid->i1);
	delete eid;
	return true;
    } else
	return false;
}

template <typename I0, typename I1>
void rendezvous<I0, I1>::clear()
{
    for (tamerpriv::simple_event *e = waiting_; e; e = e->next())
	delete reinterpret_cast<eventid *>(e->rid());
    abstract_rendezvous::remove_waiting();
    for (tamerpriv::simple_event *e = ready_; e; e = e->next())
	delete reinterpret_cast<eventid *>(e->rid());
    explicit_rendezvous::remove_ready();
}


/** @cond specialized_rendezvous */

template <typename I0>
class rendezvous<I0, void> : public tamerpriv::explicit_rendezvous { public:

    inline rendezvous(rendezvous_flags flags = rnormal);
    inline ~rendezvous();

    inline bool join(I0 &);
    void clear();

    inline bool has_ready() const {
	return ready_;
    }
    inline bool has_waiting() const {
	return waiting_;
    }
    inline bool has_events() const {
	return has_ready() || has_waiting();
    }

    inline void add(tamerpriv::simple_event *e, const I0 &i0);

};

template <typename I0>
inline rendezvous<I0, void>::rendezvous(rendezvous_flags flags)
    : explicit_rendezvous(flags)
{
}

template <typename I0>
inline rendezvous<I0, void>::~rendezvous()
{
    if (waiting_ || ready_)
	clear();
}

template <typename I0>
inline void rendezvous<I0, void>::add(tamerpriv::simple_event *e, const I0 &i0)
{
    I0 *eid = new I0(i0);
    e->initialize(this, reinterpret_cast<uintptr_t>(eid));
}

template <typename I0>
inline bool rendezvous<I0, void>::join(I0 &i0)
{
    if (ready_) {
	I0 *eid = reinterpret_cast<I0 *>(pop_ready());
	i0 = TAMER_MOVE(*eid);
	delete eid;
	return true;
    } else
	return false;
}

template <typename I0>
void rendezvous<I0, void>::clear()
{
    for (tamerpriv::simple_event *e = waiting_; e; e = e->next())
	delete reinterpret_cast<I0 *>(e->rid());
    abstract_rendezvous::remove_waiting();
    for (tamerpriv::simple_event *e = ready_; e; e = e->next())
	delete reinterpret_cast<I0 *>(e->rid());
    explicit_rendezvous::remove_ready();
}


template <>
class rendezvous<uintptr_t> : public tamerpriv::explicit_rendezvous { public:

    inline rendezvous(rendezvous_flags flags = rnormal);
    inline ~rendezvous();

    inline bool has_ready() const {
	return ready_;
    }
    inline bool has_waiting() const {
	return waiting_;
    }
    inline bool has_events() const {
	return has_ready() || has_waiting();
    }

    inline bool join(uintptr_t &);
    void clear();

    inline void add(tamerpriv::simple_event *e, uintptr_t i0) TAMER_NOEXCEPT;

};

inline rendezvous<uintptr_t>::rendezvous(rendezvous_flags flags)
    : explicit_rendezvous(flags)
{
}

inline rendezvous<uintptr_t>::~rendezvous()
{
    if (waiting_ || ready_)
	clear();
}

inline void rendezvous<uintptr_t>::add(tamerpriv::simple_event *e, uintptr_t i0) TAMER_NOEXCEPT
{
    e->initialize(this, i0);
}

inline bool rendezvous<uintptr_t>::join(uintptr_t &i0)
{
    if (ready_) {
	i0 = pop_ready();
	return true;
    } else
	return false;
}


template <typename T>
class rendezvous<T *> : public rendezvous<uintptr_t> { public:

    typedef rendezvous<uintptr_t> inherited;

    rendezvous(rendezvous_flags flags = rnormal)
	: inherited(flags) {
    }

    inline bool join(T *&i0) {
	if (ready_) {
	    i0 = reinterpret_cast<T *>(pop_ready());
	    return true;
	} else
	    return false;
    }

    inline void add(tamerpriv::simple_event *e, T *i0) TAMER_NOEXCEPT {
	inherited::add(e, reinterpret_cast<uintptr_t>(i0));
    }

};


template <>
class rendezvous<int> : public rendezvous<uintptr_t> { public:

    typedef rendezvous<uintptr_t> inherited;

    rendezvous(rendezvous_flags flags = rnormal)
	: inherited(flags) {
    }

    inline bool join(int &i0) {
	if (ready_) {
	    i0 = static_cast<int>(pop_ready());
	    return true;
	} else
	    return false;
    }

    inline void add(tamerpriv::simple_event *e, int i0) TAMER_NOEXCEPT {
	inherited::add(e, static_cast<uintptr_t>(i0));
    }

};


template <>
class rendezvous<bool> : public rendezvous<uintptr_t> { public:

    typedef rendezvous<uintptr_t> inherited;

    rendezvous(rendezvous_flags flags = rnormal)
	: inherited(flags) {
    }

    inline bool join(bool &i0) {
	if (ready_) {
	    i0 = static_cast<bool>(pop_ready());
	    return true;
	} else
	    return false;
    }

    inline void add(tamerpriv::simple_event *e, bool i0) TAMER_NOEXCEPT {
	inherited::add(e, static_cast<uintptr_t>(i0));
    }

};


template <>
class rendezvous<void> : public tamerpriv::explicit_rendezvous { public:

    inline rendezvous(rendezvous_flags flags = rnormal)
	: explicit_rendezvous(flags) {
    }
    inline ~rendezvous() {
	if (waiting_ || ready_)
	    clear();
    }

    inline bool has_ready() const {
	return ready_;
    }
    inline bool has_waiting() const {
	return waiting_;
    }
    inline bool has_events() const {
	return has_ready() || has_waiting();
    }

    inline bool join() {
	if (ready_) {
	    (void) pop_ready();
	    return true;
	} else
	    return false;
    }
    void clear();

    inline void add(tamerpriv::simple_event *e) TAMER_NOEXCEPT {
	e->initialize(this, 1);
    }

};


class gather_rendezvous : public tamerpriv::abstract_rendezvous { public:

    inline gather_rendezvous(tamerpriv::tamer_closure *c)
	: abstract_rendezvous(rnormal, tamerpriv::rgather), linked_closure_(c) {
    }
    inline ~gather_rendezvous() {
	if (waiting_)
	    clear();
    }

    void clear();

    inline bool has_waiting() const {
	return waiting_;
    }

    inline void add(tamerpriv::simple_event *e) TAMER_NOEXCEPT {
	e->initialize(this, 1);
    }

  private:

    tamerpriv::tamer_closure *linked_closure_;
    friend class tamerpriv::abstract_rendezvous;

};

/** @endcond */

}
#endif /* TAMER__RENDEZVOUS_HH */
