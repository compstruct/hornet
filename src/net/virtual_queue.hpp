// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __VIRTUAL_QUEUE_HPP__
#define __VIRTUAL_QUEUE_HPP__

#include <iostream>
#include <cassert>
#include <utility>
#include <queue>
#include <boost/shared_ptr.hpp>
#include "error.hpp"
#include "logger.hpp"
#include "cstdint.hpp"
#include "flit.hpp"

using namespace std;
using namespace boost;

class node_id {
public:
    node_id(uint32_t new_id) throw();
    bool operator==(const node_id &) const throw();
    bool operator<(const node_id &) const throw();
    uint32_t get_numeric_id() const throw();
    friend ostream &operator<<(ostream &, const node_id &);
private:
    explicit node_id() throw();
    const uint32_t id;
};

inline node_id::node_id(uint32_t new_id) throw() : id(new_id) { }

inline bool node_id::operator==(const node_id &o) const throw() {
    return id == o.id;
}

inline bool node_id::operator<(const node_id &o) const throw() {
    return id < o.id;
}

inline uint32_t node_id::get_numeric_id() const throw() { return id; }

ostream &operator<<(ostream &, const node_id &);

class virtual_queue_id {
public:
    virtual_queue_id(uint32_t new_id) throw();
    virtual_queue_id operator+(int) const throw();
    bool operator==(const virtual_queue_id &) const throw();
    bool operator<(const virtual_queue_id &) const throw();
    uint32_t get_numeric_id() const throw();
    friend ostream &operator<<(ostream &, const virtual_queue_id &);
private:
    explicit virtual_queue_id() throw();
    const uint32_t id;
};

inline virtual_queue_id::virtual_queue_id(uint32_t new_id) throw() : id(new_id)
{ }

inline virtual_queue_id virtual_queue_id::operator+(int diff) const throw() {
    return virtual_queue_id(id + diff);
}

inline bool
virtual_queue_id::operator==(const virtual_queue_id &o) const throw() {
    return id == o.id;
}

inline bool
virtual_queue_id::operator<(const virtual_queue_id &o) const throw() {
    return id < o.id;
}

inline uint32_t virtual_queue_id::get_numeric_id() const throw() { return id; }

ostream &operator<<(ostream &, const virtual_queue_id &);

// common allocator allows several virtual queues to share memory
class common_alloc {
public:
    explicit common_alloc(unsigned max_slots) throw();
    unsigned free_slots() const throw();
    bool full() const throw();
    void alloc(unsigned num_slots = 1) throw(err);
    void dealloc(unsigned num_slots = 1) throw(err);
private:
    unsigned size;
};

inline bool common_alloc::full() const throw() { return size == 0; }

inline void common_alloc::alloc(unsigned n) throw(err) { size -= n; } // XXX exc

inline void common_alloc::dealloc(unsigned n) throw(err) { size += n; } // XXX exc

// virtual queues support one flow at a time
class virtual_queue : public queue<flit> {
public:
    explicit virtual_queue(node_id parent_id, virtual_queue_id queue_id,
                           shared_ptr<common_alloc> alloc,
                           shared_ptr<logger> log =
                               shared_ptr<logger>(new logger()))
        throw();
    const pair<node_id, virtual_queue_id> &get_id() const throw();
    void push(const flit &);
    void pop();
    bool full() const throw();
    bool ingress_new_flow() const throw(); // can accept new flit sequence
    bool egress_new_flow() const throw();  // next flit is a head flit
    flow_id get_egress_flow_id() const throw(err);
    uint32_t get_egress_flow_length() const throw(err);
    void claim(const node_id &target) throw(err);
private:
    const pair<node_id, virtual_queue_id> id;
    unsigned ingress_remaining;
    flow_id ingress_flow;
    unsigned egress_remaining;
    flow_id egress_flow;
    const shared_ptr<common_alloc> alloc;
    shared_ptr<logger> log;
    bool claimed;
};

ostream &operator<<(ostream &, const pair<node_id, virtual_queue_id> &);

inline const pair<node_id,virtual_queue_id>
&virtual_queue::get_id() const throw() { return id; }

inline bool virtual_queue::egress_new_flow() const throw () {
    return egress_remaining == 0;
}

inline bool virtual_queue::ingress_new_flow() const throw () {
    return !full() && ingress_remaining == 0;
}

inline void virtual_queue::push(const flit &f) {
    if (ingress_new_flow()) {
        log << verbosity(3) << "[queue " << id << "] ingress: "
            << reinterpret_cast<const head_flit &>(f) << endl;
    } else {
        log << verbosity(3) << "[queue " << id << "] ingress: " << f << endl;
    }
    assert(!full());
    alloc->alloc();
    if (ingress_new_flow()) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        ingress_flow = head.get_flow_id();
        ingress_remaining = head.get_length();
    } else {
        --ingress_remaining;
    }
    queue<flit>::push(f);
}

inline void virtual_queue::pop() {
    assert(!empty());
    flit &f = front();
    if (egress_new_flow()) {
        log << verbosity(3) << "[queue " << id << "] egress: "
            << reinterpret_cast<const head_flit &>(f) << endl;
    } else {
        log << verbosity(3) << "[queue " << id << "] egress: " << f << endl;
    }
    alloc->dealloc();
    if (egress_new_flow()) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        egress_flow = head.get_flow_id();
        egress_remaining = head.get_length();
    } else {
        --egress_remaining;
    }
    queue<flit>::pop();
}

inline flow_id virtual_queue::get_egress_flow_id() const throw (err) {
    if (empty()) throw exc_empty_queue(id.first.get_numeric_id(),
                                       id.second.get_numeric_id());
    if (egress_new_flow()) {
        const head_flit &head = reinterpret_cast<const head_flit &>(front());
        return head.get_flow_id();
    } else {
        return egress_flow;
    }
}

inline uint32_t virtual_queue::get_egress_flow_length() const throw (err) {
    if (empty()) throw exc_empty_queue(id.first.get_numeric_id(),
                                       id.second.get_numeric_id());
    if (egress_new_flow()) {
        const head_flit &head = reinterpret_cast<const head_flit &>(front());
        return head.get_length();
    } else {
        return egress_remaining;
    }
}

inline bool virtual_queue::full() const throw() { return alloc->full(); }

#endif // __VIRTUAL_QUEUE_HPP__

