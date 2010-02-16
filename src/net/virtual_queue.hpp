// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __VIRTUAL_QUEUE_HPP__
#define __VIRTUAL_QUEUE_HPP__

#include <iostream>
#include <cassert>
#include <cstddef>
#include <utility>
#include <deque>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "error.hpp"
#include "logger.hpp"
#include "cstdint.hpp"
#include "flit.hpp"
#include "node_id.hpp"
#include "statistics.hpp"
#include "virtual_queue_id.hpp"
#include "pressure_tracker.hpp"

using namespace std;
using namespace boost;

// common allocator allows several virtual queues to share memory
// release events are not observed until the next positive tick
class common_alloc {
public:
    explicit common_alloc(unsigned max_slots) throw();
    bool full() const throw();
    void alloc(unsigned num_slots = 1) throw();
    void dealloc(unsigned num_slots = 1) throw();
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
private:
    const unsigned max_capacity;
    unsigned capacity; // # full 
    unsigned stale_capacity; // not updated by dealloc() until negative clock edge
    common_alloc();
    common_alloc(const common_alloc &);
};

class channel_alloc;

class virtual_queue {
public:
    explicit virtual_queue(node_id parent_id, virtual_queue_id queue_id,
                           node_id src_node_id,
                           shared_ptr<channel_alloc> vc_alloc,
                           shared_ptr<pressure_tracker> pressures,
                           shared_ptr<common_alloc> alloc,
                           shared_ptr<statistics> stats,
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
    void set_front_next_hop(const node_id &nid, const flow_id &fid) throw(err);
    void set_front_vq_id(const virtual_queue_id &vqid) throw(err);
    bool ingress_new_flow() const throw(); // can accept new flit sequence
    bool egress_new_flow() const throw();  // next flit is a head flit
    flow_id get_egress_old_flow_id() const throw(err);
    flow_id get_egress_new_flow_id() const throw(err);
    uint32_t get_egress_flow_length() const throw(err);
    bool has_old_flow(const flow_id &flow) const throw(err);
    node_id get_src_node_id() const throw();
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
    bool is_drained() const throw(); // uses real size
private:
    const virtual_queue_node_id id;
    node_id src_node_id;
    typedef deque<flit> flits_queue_t;
    flits_queue_t q;
    shared_ptr<channel_alloc> vc_alloc;
    shared_ptr<pressure_tracker> pressures;
    unsigned ingress_remaining;
    flow_id ingress_flow;
    unsigned egress_remaining;
    flow_id egress_flow_old; // only valid for non-head flits
    flow_id egress_flow_new;
    node_id egress_node;
    virtual_queue_id egress_vq;
    const shared_ptr<common_alloc> alloc;
    map<flow_id, unsigned> old_flows; // count of packets for a given flow
    size_t stale_size; // resynchronized at negative clock edge
    shared_ptr<statistics> stats;
    logger &log;
};

#endif // __VIRTUAL_QUEUE_HPP__

