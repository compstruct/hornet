// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __VIRTUAL_QUEUE_HPP__
#define __VIRTUAL_QUEUE_HPP__

#include <iostream>
#include <cassert>
#include <cstddef>
#include <utility>
#include <queue>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "error.hpp"
#include "logger.hpp"
#include "cstdint.hpp"
#include "flit.hpp"
#include "node_id.hpp"
#include "virtual_queue_id.hpp"
#include "router.hpp"
#include "pressure_tracker.hpp"

using namespace std;
using namespace boost;

// common allocator allows several virtual queues to share memory
class common_alloc {
public:
    explicit common_alloc(unsigned max_slots) throw();
    unsigned free_slots() const throw();
    bool full() const throw();
    void alloc(unsigned num_slots = 1) throw();
    void dealloc(unsigned num_slots = 1) throw();
private:
    unsigned size;
    unsigned max_size;
    common_alloc();
    common_alloc(const common_alloc &);
};

inline bool common_alloc::full() const throw() { return size == 0; }

inline void common_alloc::alloc(unsigned n) throw() {
    assert(n <= size);
    size -= n;
}

inline void common_alloc::dealloc(unsigned n) throw() {
    assert(size + n <= max_size);
    size += n;
}

inline unsigned common_alloc::free_slots() const throw() { return size; }

class channel_alloc;

// virtual queues support one flow at a time
class virtual_queue {
public:
    explicit virtual_queue(node_id parent_id, virtual_queue_id queue_id,
                           node_id src_node_id,
                           shared_ptr<router> flow_router,
                           shared_ptr<channel_alloc> vc_alloc,
                           shared_ptr<pressure_tracker> pressures,
                           shared_ptr<common_alloc> alloc,
                           logger &log) throw();
    const virtual_queue_node_id &get_id() const throw();
    void push(const flit &) throw(err); // does not update stale size
    void pop() throw(err); // updates stale size
    size_t size() const throw(); // reports stale size
    bool empty() const throw(); // uses stale size
    bool full() const throw();  // uses real size only
    bool egress_ready() const throw(err);
    flit front() const throw(err);
    node_id front_node_id() const throw(err); // dest node id for front flit
    virtual_queue_id front_vq_id() const throw(err);
    void set_front_vq_id(const virtual_queue_id &vqid) throw(err);
    bool ingress_new_flow() const throw(); // can accept new flit sequence
    bool egress_new_flow() const throw();  // next flit is a head flit
    flow_id get_egress_flow_id() const throw(err);
    uint32_t get_egress_flow_length() const throw(err);
    node_id get_src_node_id() const throw();
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
private:
    const virtual_queue_node_id id;
    node_id src_node_id;
    queue<tuple<flit, node_id> > q;
    shared_ptr<router> rt;
    shared_ptr<channel_alloc> vc_alloc;
    shared_ptr<pressure_tracker> pressures;
    node_id next_node; // for next incoming flit
    unsigned ingress_remaining;
    flow_id ingress_flow;
    unsigned egress_remaining;
    flow_id egress_flow; // only valid for non-head flits
    virtual_queue_id egress_vq;
    const shared_ptr<common_alloc> alloc;
    size_t stale_size; // resynchronized at negative clock edge
    logger &log;
};

inline const virtual_queue_node_id &virtual_queue::get_id() const throw() {
    return id;
}

inline bool virtual_queue::egress_new_flow() const throw () {
    return !empty() && egress_remaining == 0;
}

inline bool virtual_queue::ingress_new_flow() const throw () {
    return !full() && ingress_remaining == 0;
}


inline bool virtual_queue::full() const throw() { return alloc->full(); }

inline size_t virtual_queue::size() const throw() { return stale_size; }

inline bool virtual_queue::empty() const throw() { return size() == 0; }

inline bool virtual_queue::egress_ready() const throw(err) {
    return !empty() && front_vq_id().is_valid();
}

inline flit virtual_queue::front() const throw(err) {
    assert(!empty());
    return q.front().get<0>();
}

inline node_id virtual_queue::front_node_id() const throw(err) {
    assert(!empty());
    return q.front().get<1>();
}

inline virtual_queue_id virtual_queue::front_vq_id() const throw(err) {
    assert(!empty());
    return egress_vq;
}

inline node_id virtual_queue::get_src_node_id() const throw() {
    return src_node_id;
}

inline void virtual_queue::tick_positive_edge() throw(err) { }

inline void virtual_queue::tick_negative_edge() throw(err) {
    stale_size = q.size();
}

#endif // __VIRTUAL_QUEUE_HPP__

