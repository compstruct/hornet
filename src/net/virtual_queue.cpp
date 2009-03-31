// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include "virtual_queue.hpp"
#include "channel_alloc.hpp"

common_alloc::common_alloc(unsigned max_slots) throw()
    : size(max_slots), max_size(max_slots) { }

virtual_queue::virtual_queue(node_id new_node_id, virtual_queue_id new_vq_id,
                             shared_ptr<router> new_rt,
                             shared_ptr<channel_alloc> new_vc_alloc,
                             shared_ptr<pressure_tracker> new_pt,
                             shared_ptr<common_alloc> new_alloc,
                             logger &l) throw()
    : id(make_tuple(new_node_id, new_vq_id)), q(), rt(new_rt),
      vc_alloc(new_vc_alloc), pressures(new_pt),
      ingress_remaining(0), ingress_flow(0),
      egress_remaining(0), egress_flow(0), egress_vq(),
      alloc(new_alloc), stale_size(0), log(l) { }

void virtual_queue::push(const flit &f) throw(err) {
    assert(!full());
    alloc->alloc();
    if (ingress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        ingress_flow = head.get_flow_id();
        if (q.empty()) egress_flow = ingress_flow;
        ingress_remaining = head.get_length();
        next_node = rt->route(ingress_flow);
        LOG(log,3) << "[queue " << id << "] ingress: " << head
            << " (flow " << ingress_flow << ")" << endl;
    } else {
        --ingress_remaining;
        LOG(log,3) << "[queue " << id << "] ingress: " << f
            << " (flow " << ingress_flow << ")" << endl;
    }
    if (egress_vq.is_valid() && q.empty()) { // continuing flit gets to head
        pressures->inc(next_node, egress_vq);
    }
    q.push(make_tuple(f, next_node));
}

void virtual_queue::pop() throw(err) {
    assert(!empty());
    flit f(0); node_id n; tie(f,n) = q.front();
    alloc->dealloc();
    if (egress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        egress_flow = head.get_flow_id();
        egress_remaining = head.get_length();
        LOG(log,3) << "[queue " << id << "] egress:  " << head
            << " (flow " << egress_flow << ")" << endl;
    } else {
        --egress_remaining;
        LOG(log,3) << "[queue " << id << "] egress:  " << f
            << " (flow " << egress_flow << ")" << endl;
    }
    q.pop();
    if (egress_remaining == 0) {
        vc_alloc->release(make_tuple(n, egress_vq));
        egress_vq = virtual_queue_id(); // invalid vqid
    }
    // decrease pressure if next flit is new flow, or VC is empty
    if (!egress_vq.is_valid() || q.empty()) {
        pressures->dec(n, egress_vq);
    }
    --stale_size;
}

flow_id virtual_queue::get_egress_flow_id() const throw (err) {
    assert(!empty());
    if (egress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(q.front());
        return head.get_flow_id();
    } else {
        return egress_flow;
    }
}

uint32_t virtual_queue::get_egress_flow_length() const throw (err) {
    assert(!empty());
    if (egress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(q.front());
        return head.get_length();
    } else {
        return egress_remaining;
    }
}

void virtual_queue::set_front_vq_id(const virtual_queue_id &vqid) throw(err) {
    assert(!empty());
    assert(!egress_vq.is_valid());
    assert(vqid.is_valid());
    flit f(0); node_id n; tie(f,n) = q.front();
    LOG(log,3) << "[queue " << id << "] granted next hop "
               << make_tuple(n, vqid)
               << " (flow " << egress_flow << ")" << endl;
    vc_alloc->claim(make_tuple(n, vqid));
    pressures->inc(n, vqid); // increase pressure for a head flit
    egress_vq = vqid;
}
