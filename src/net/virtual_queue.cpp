// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include "virtual_queue.hpp"
#include "channel_alloc.hpp"

common_alloc::common_alloc(unsigned max_slots) throw()
    : max_capacity(max_slots), capacity(max_slots),
      stale_capacity(max_slots) { }

bool common_alloc::full() const throw() { return stale_capacity == 0; }

void common_alloc::alloc(unsigned n) throw() {
    assert(n <= stale_capacity);
    stale_capacity -= n;
    capacity -= n;
}

void common_alloc::dealloc(unsigned n) throw() {
    assert(capacity + n <= max_capacity);
    capacity += n;
}

void common_alloc::tick_positive_edge() throw(err) { }

void common_alloc::tick_negative_edge() throw(err) {
    stale_capacity = capacity;
}

virtual_queue::virtual_queue(node_id new_node_id, virtual_queue_id new_vq_id,
                             node_id new_src_node_id,
                             shared_ptr<channel_alloc> new_vc_alloc,
                             shared_ptr<pressure_tracker> new_pt,
                             shared_ptr<common_alloc> new_alloc,
                             shared_ptr<statistics> st, logger &l) throw()
    : id(make_tuple(new_node_id, new_vq_id)), src_node_id(new_src_node_id),
      q(), vc_alloc(new_vc_alloc), pressures(new_pt),
      ingress_remaining(0), ingress_flow(0),
      egress_remaining(0), egress_flow_old(0), egress_flow_new(0), egress_vq(),
      alloc(new_alloc), old_flows(), stale_size(0), stats(st), log(l) {
    stats->register_queue(id);
}

const virtual_queue_node_id &virtual_queue::get_id() const throw() {
    return id;
}

bool virtual_queue::egress_new_flow() const throw () {
    return !empty() && egress_remaining == 0;
}

bool virtual_queue::ingress_new_flow() const throw () {
    return !full() && ingress_remaining == 0;
}

bool virtual_queue::full() const throw() { return alloc->full(); }

size_t virtual_queue::size() const throw() { return stale_size; }

bool virtual_queue::empty() const throw() { return size() == 0; }

bool virtual_queue::egress_ready() const throw(err) {
    return !empty() && front_node_id().is_valid() && front_vq_id().is_valid();
}

flit virtual_queue::front() const throw(err) {
    assert(!empty());
    assert(egress_remaining == 0 || egress_flow_old.is_valid());
    assert(egress_node.is_valid());
    assert(egress_vq.is_valid());
    assert(egress_flow_new.is_valid());
    if (egress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(q.front());
        const flow_id &fid = head.get_flow_id();
        if (fid == egress_flow_new) {
            return head;
        } else {
            return head_flit(head, egress_flow_new);
        }
    } else {
        return q.front();
    }
}

node_id virtual_queue::front_node_id() const throw(err) {
    assert(!empty());
    return egress_node;
}

virtual_queue_id virtual_queue::front_vq_id() const throw(err) {
    assert(!empty());
    return egress_vq;
}

node_id virtual_queue::get_src_node_id() const throw() {
    return src_node_id;
}

void virtual_queue::tick_positive_edge() throw(err) {
    alloc->tick_positive_edge();
}

void virtual_queue::tick_negative_edge() throw(err) {
    stale_size = q.size();
    stats->virtual_queue(id, size());
    alloc->tick_negative_edge();
    uint32_t n = 0;
    for (flits_queue_t::iterator i = q.begin(); i != q.end(); ++i, ++n) {
        LOG(log,10) << "[queue " << id << "] @" << dec << n << ": "
                    << *i << endl;
    }
}

bool virtual_queue::is_drained() const throw() {
    return q.empty();
}

void virtual_queue::push(const flit &f) throw(err) {
    assert(!full());
    alloc->alloc();
    if (ingress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        ingress_flow = head.get_flow_id();
        ingress_remaining = head.get_length();
        map<flow_id, unsigned>::const_iterator i = old_flows.find(ingress_flow);
        if (i == old_flows.end()) old_flows[ingress_flow] = 0;
        ++old_flows[ingress_flow];
        if (q.empty()) {
            egress_node = node_id(); // invalid ID
            egress_vq = virtual_queue_id(); // invalid ID
            egress_flow_old = flow_id(); // invalid ID
            egress_flow_new = flow_id(); // invalid ID
        }
        LOG(log,3) << "[queue " << id << "] ingress: " << head
                   << " (flow " << ingress_flow << ")"
                   << ", #" << hex << setfill('0') << setw(8)
                   << f.get_uid() << ")" << endl;
    } else {
        --ingress_remaining;
        LOG(log,3) << "[queue " << id << "] ingress: " << f
                   << " (flow " << ingress_flow << ", #"
                   << hex << setfill('0') << setw(8)
                   << f.get_uid() << ")" << endl;
    }
    if (egress_vq.is_valid() && q.empty()) { // continuing flit gets to vq head
        pressures->inc(egress_node, egress_vq);
    }
    q.push_back(f);
}

void virtual_queue::pop() throw(err) {
    assert(!empty());
    alloc->dealloc();
    assert(egress_remaining == 0 || egress_flow_old.is_valid());
    assert(egress_node.is_valid());
    assert(egress_vq.is_valid());
    const flit &f = q.front();
    if (egress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        egress_flow_old = head.get_flow_id();
        egress_remaining = head.get_length();
        LOG(log,3) << "[queue " << id << "] egress:  " << head
            << " (flow ";
        if (egress_flow_new == egress_flow_old) {
            LOG(log,3) << egress_flow_old;
        } else {
            LOG(log,3) << egress_flow_old << "->" << egress_flow_new;
        }
        LOG(log,3) << ", #" << hex << setfill('0') << setw(8)
                   << f.get_uid() << ")" << endl;
    } else {
        --egress_remaining;
        LOG(log,3) << "[queue " << id << "] egress:  " << f
            << " (flow ";
        if (egress_flow_new == egress_flow_old) {
            LOG(log,3) << egress_flow_old;
        } else {
            LOG(log,3) << egress_flow_old << "->" << egress_flow_new;
        }
        LOG(log,3) << ", #" << hex << setfill('0') << setw(8)
                   << f.get_uid() << ")" << endl;
    }
    q.pop_front();
    // decrease pressure if next flit is new flow, or VC is empty
    if (egress_remaining == 0 || q.empty()) {
        assert(egress_node.is_valid());
        assert(egress_vq.is_valid());
        pressures->dec(egress_node, egress_vq);
    }
    if (egress_remaining == 0) {
        vc_alloc->release(make_tuple(egress_node, egress_vq));
        assert(old_flows.find(egress_flow_old) != old_flows.end());
        --old_flows[egress_flow_old];
        egress_node = node_id(); // invalid ID
        egress_vq = virtual_queue_id(); // invalid ID
        egress_flow_new = flow_id(); // invalid ID
        egress_flow_old = flow_id(); // invalid ID
    }
    --stale_size;
}

flow_id virtual_queue::get_egress_old_flow_id() const throw (err) {
    assert(!empty());
    if (egress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(q.front());
        return head.get_flow_id();
    } else {
        return egress_flow_old;
    }
}

flow_id virtual_queue::get_egress_new_flow_id() const throw (err) {
    assert(!empty());
    return egress_flow_new;
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

void virtual_queue::set_front_next_hop(const node_id &next_node,
                                       const flow_id &next_flow) throw(err) {
    assert(!empty());
    assert(!egress_node.is_valid());
    assert(!egress_vq.is_valid());
    assert(egress_remaining == 0);
    assert(next_node.is_valid());
    assert(next_flow.is_valid());
    egress_node = next_node;
    egress_flow_new = next_flow;
    const head_flit &head = reinterpret_cast<const head_flit &>(q.front());
    LOG(log,3) << "[queue " << id << "] granted next hop node "
               << next_node << " (flow ";
    if (egress_flow_new == head.get_flow_id()) {
        LOG(log,3) << head.get_flow_id();
    } else {
        LOG(log,3) << head.get_flow_id() << "->" << egress_flow_new;
    }
    LOG(log,3) << ")" << endl;
}

void virtual_queue::set_front_vq_id(const virtual_queue_id &vqid) throw(err) {
    assert(!empty());
    assert(egress_node.is_valid());
    assert(!egress_vq.is_valid());
    assert(egress_remaining == 0);
    assert(vqid.is_valid());
    egress_vq = vqid;
    const head_flit &head = reinterpret_cast<const head_flit &>(q.front());
    LOG(log,3) << "[queue " << id << "] granted next hop queue "
               << make_tuple(egress_node, egress_vq) << " (flow ";
    if (egress_flow_new == head.get_flow_id()) {
        LOG(log,3) << head.get_flow_id();
    } else {
        LOG(log,3) << head.get_flow_id() << "->" << egress_flow_new;
    }
    LOG(log,3) << ")" << endl;
    vc_alloc->claim(make_tuple(egress_node, egress_vq));
    pressures->inc(egress_node, egress_vq); // increase pressure for a head flit
}

bool virtual_queue::has_old_flow(const flow_id &flow) const throw(err) {
    map<flow_id, unsigned>::const_iterator i = old_flows.find(flow);
    return ((i != old_flows.end()) && (i->second > 0));
}
