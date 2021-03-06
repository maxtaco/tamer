#ifndef TAMER_DRIVER_HH
#define TAMER_DRIVER_HH 1
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
#include <tamer/xdriver.hh>
namespace tamer {

/** @file <tamer/driver.hh>
 *  @brief  Functions for registering primitive events and managing the event
 *  loop.
 */

/** @brief  Initialize the Tamer event loop.
 *
 *  Must be called at least once before any primitive Tamer events are
 *  registered.
 */
void initialize();

/** @brief  Clean up the Tamer event loop.
 *
 *  Delete the driver. Should not be called unless all Tamer objects are
 *  deleted.
 */
void cleanup();

/** @brief  Fetches Tamer's current time.
 *  @return  Current timestamp.
 */
inline timeval &now() {
    return driver::main->now;
}

/** @brief  Sets Tamer's current time to the current timestamp.
 */
inline void set_now() {
    driver::main->set_now();
}

/** @brief  Test whether driver events are pending.
 *  @return  True if no driver events are pending, otherwise false.
 *
 *  If @c driver_empty() is true, then @c once() will hang forever.
 */
inline bool driver_empty() {
    return driver::main->empty();
}

/** @brief  Run driver loop once. */
inline void once() {
    driver::main->once();
}

/** @brief  Run driver loop indefinitely. */
inline void loop() {
    driver::main->loop();
}

/** @brief  Register event for file descriptor readability.
 *  @param  fd  File descriptor.
 *  @param  e   Event.
 *
 *  Triggers @a e when @a fd becomes readable.  Cancels @a e when @a fd is
 *  closed.
 */
inline void at_fd_read(int fd, const event<> &e) {
    driver::main->at_fd_read(fd, e);
}

inline void at_fd_read(int fd, const event<int> &e) {
    driver::main->at_fd_read(fd, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
inline void at_fd_read(int fd, event<> &&e) {
    driver::main->at_fd_read(fd, TAMER_MOVE(e));
}

inline void at_fd_read(int fd, event<int> &&e) {
    driver::main->at_fd_read(fd, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for file descriptor writability.
 *  @param  fd  File descriptor.
 *  @param  e   Event.
 *
 *  Triggers @a e when @a fd becomes writable.  Cancels @a e when @a fd is
 *  closed.
 */
inline void at_fd_write(int fd, const event<> &e) {
    driver::main->at_fd_write(fd, e);
}

inline void at_fd_write(int fd, const event<int> &e) {
    driver::main->at_fd_write(fd, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
inline void at_fd_write(int fd, event<> &&e) {
    driver::main->at_fd_write(fd, TAMER_MOVE(e));
}

inline void at_fd_write(int fd, event<int> &&e) {
    driver::main->at_fd_write(fd, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for a given time.
 *  @param  expiry  Time.
 *  @param  e       Event.
 *
 *  Triggers @a e at timestamp @a expiry, or soon afterwards.
 */
inline void at_time(const timeval &expiry, const event<> &e) {
    driver::main->at_time(expiry, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline void at_time(const timeval &expiry, event<> &&e) {
    driver::main->at_time(expiry, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for a given delay.
 *  @param  delay  Delay time.
 *  @param  e      Event.
 *
 *  Triggers @a e when @a delay seconds have elapsed since @c now(), or soon
 *  afterwards.
 */
inline void at_delay(const timeval &delay, const event<> &e) {
    driver::main->at_delay(delay, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline void at_delay(const timeval &delay, event<> &&e) {
    driver::main->at_delay(delay, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for a given delay.
 *  @param  delay  Delay time.
 *  @param  e      Event.
 *
 *  Triggers @a e when @a delay seconds have elapsed since @c now(), or soon
 *  afterwards.
 */
inline void at_delay(double delay, const event<> &e) {
    driver::main->at_delay(delay, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline void at_delay(double delay, event<> &&e) {
    driver::main->at_delay(delay, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for a given delay.
 *  @param  delay  Delay time.
 *  @param  e      Event.
 *
 *  Triggers @a e when @a delay seconds have elapsed since @c now(), or soon
 *  afterwards.
 */
inline void at_delay_sec(int delay, const event<> &e) {
    driver::main->at_delay_sec(delay, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline void at_delay_sec(int delay, event<> &&e) {
    driver::main->at_delay_sec(delay, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for a given delay.
 *  @param  delay  Delay time in milliseconds.
 *  @param  e      Event.
 *
 *  Triggers @a e when @a delay milliseconds have elapsed since @c now(), or
 *  soon afterwards.
 */
inline void at_delay_msec(int delay, const event<> &e) {
    driver::main->at_delay_msec(delay, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline void at_delay_msec(int delay, event<> &&e) {
    driver::main->at_delay_msec(delay, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for a given delay.
 *  @param  delay  Delay time in microseconds.
 *  @param  e      Event.
 *
 *  Triggers @a e when @a delay microseconds have elapsed since @c now(), or
 *  soon afterwards.
 */
inline void at_delay_usec(int delay, const event<> &e) {
    driver::main->at_delay_usec(delay, e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline void at_delay_usec(int delay, event<> &&e) {
    driver::main->at_delay_usec(delay, TAMER_MOVE(e));
}
#endif

/** @brief  Register event for signal occurrence.
 *  @param  signo  Signal number.
 *  @param  e      Event.
 *
 *  Triggers @a e soon after @a signo is received.  The signal @a signo
 *  is blocked while @a e is triggered and unblocked afterwards.
 */
inline void at_signal(int signo, const event<> &e) {
    driver::at_signal(signo, e);
}

/** @brief  Register event to trigger soon.
 *  @param  e  Event.
 *
 *  Triggers @a e the next time through the driver loop.
 */
inline void at_asap(const event<> &e) {
    driver::main->at_asap(e);
}

#if TAMER_HAVE_CXX_RVALUE_REFERENCES
/** @overload */
inline void at_asap(event<> &&e) {
    driver::main->at_asap(TAMER_MOVE(e));
}
#endif

}
#endif /* TAMER_DRIVER_HH */
