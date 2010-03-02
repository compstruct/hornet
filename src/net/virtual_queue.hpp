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
#include <boost/thread.hpp>
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

class channel_alloc;

// The virtual_queue class models a switch virtual channel.
//
// Methods *modifying* the queue may only be called during the positive edge
// of the clock.  Any modifications to the back of the queue (pushes, etc)
// must not be reflected at the front of the queue during the same positive
// edge (and vice versa); the negative_edge method of the queue propagates
// updates.
//
// The virtual_queue also keeps track of packet boundaries (by counting flits),
// next-hop port assignment, next-hop VC assignment, and flow ID renaming.

class virtual_queue {
public:
    explicit virtual_queue(node_id parent_id, virtual_queue_id queue_id,
                           node_id src_node_id, uint32_t max_size,
                           shared_ptr<channel_alloc> vc_alloc,
                           shared_ptr<pressure_tracker> pressures,
                           shared_ptr<statistics> stats,
                           logger &log) throw();

    // methods which do not change over the life of the queue
    const virtual_queue_node_id &get_id() const throw();
    node_id get_src_node_id() const throw();

    // methods reading the front ("read end") of the queue
    bool front_is_empty() const throw();
    bool front_is_head_flit() const throw();
    node_id front_node_id() const throw();
    virtual_queue_id front_vq_id() const throw();
    flow_id front_old_flow_id() const throw();
    flow_id front_new_flow_id() const throw();
    uint32_t front_num_remaining_flits_in_packet() const throw();
    const flit &front_flit() const throw();

    // methods modifying the front ("read end") of the queue
    void front_pop() throw();
    void front_set_next_hop(const node_id &nid, const flow_id &fid) throw();
    void front_set_vq_id(const virtual_queue_id &vqid) throw();

    // methods reading/modifying the back ("write end") of the queue
    void back_push(const flit &) throw();
    bool back_is_full() const throw();
    bool back_is_mid_packet() const throw();

    // true iff "stale" view of the VC is empty
    // (not taking into account any front_pop() calls in this clock cycle)
    bool back_is_empty() const throw(); // required for EDVCA

    // true iff "stale" view of the VC has a flit with old flow ID f
    // (not taking into account any front_pop() calls in this clock cycle)
    bool back_has_old_flow(const flow_id &f) throw(); // required for EDVCA

    // other methods
    void tick_positive_edge() throw();
    void tick_negative_edge() throw();

    // methods accessing both ends of the queue

    // may only be called after all negedges have completed
    bool is_drained() const throw();

private:
    uint32_t front_size() const throw();

private:
    const virtual_queue_node_id id;
    const node_id src_node_id;
    const uint32_t buffer_size;

    typedef vector<flit> buffer_t;

    // circular buffer with at least one free slot (to distinguish full/empty);
    buffer_t contents;

    // accessed by front (pop) end
    buffer_t::size_type front_head;
    buffer_t::size_type front_stale_tail; // one slot past tail actually
    uint32_t front_egress_packet_flits_remaining; // 0 iff packet complete
    flow_id front_old_flow; // invalid iff VC is empty and packet boundary
    node_id front_next_hop_node; // invalid iff not yet routed
    flow_id front_next_hop_flow; // invalid iff not yet routed
    virtual_queue_id front_next_hop_vq; // invalid if not yet assigned
    vector<virtual_queue_node_id> front_vqs_to_release_at_negedge;

    // accessed by back (push) end
    buffer_t::size_type back_stale_head;
    buffer_t::size_type back_tail; // one slot past tail actually
    uint32_t back_ingress_packet_flits_remaining; // 0 iff packet complete
    uint32_t back_stale_egress_packet_flits_remaining; // 0 iff packet complete

    shared_ptr<channel_alloc> vc_alloc;
    shared_ptr<pressure_tracker> pressures;

    shared_ptr<statistics> stats;
    logger &log;

    mutable recursive_mutex front_mutex;
    mutable recursive_mutex back_mutex;
private:
    friend ostream &operator<<(ostream &, const virtual_queue &);
};

ostream &operator<<(ostream &out, const virtual_queue &vq);

#endif // __VIRTUAL_QUEUE_HPP__

