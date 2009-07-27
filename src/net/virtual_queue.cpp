// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include "virtual_queue.hpp"
#include "channel_alloc.hpp"

common_alloc::common_alloc(unsigned max_slots) throw()
    : size(max_slots), max_size(max_slots) { }

virtual_queue::virtual_queue(node_id new_node_id, virtual_queue_id new_vq_id,
                             node_id new_src_node_id,
                             shared_ptr<router> new_rt,
                             shared_ptr<channel_alloc> new_vc_alloc,
                             shared_ptr<pressure_tracker> new_pt,
                             shared_ptr<common_alloc> new_alloc,
                             logger &l) throw()
    : id(make_tuple(new_node_id, new_vq_id)), src_node_id(new_src_node_id),
      q(), rt(new_rt), vc_alloc(new_vc_alloc), pressures(new_pt),
      ingress_remaining(0), ingress_flow(0),
      egress_remaining(0), egress_flow_old(0), egress_flow_new(0), egress_vq(),
      alloc(new_alloc), old_flows(), stale_size(0), log(l) { }

void virtual_queue::push(const flit &f) throw(err) {
    assert(!full());
    alloc->alloc();
    flit new_flit(f);
    if (ingress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        ingress_flow = head.get_flow_id();
        ingress_remaining = head.get_length();
        map<flow_id, unsigned>::const_iterator i = old_flows.find(ingress_flow);
        if (i == old_flows.end()) old_flows[ingress_flow] = 0;
        ++old_flows[ingress_flow];
        tie(next_node, next_flow) = rt->route(src_node_id, ingress_flow);
        if (q.empty()) {
            egress_flow_old = ingress_flow;
            egress_flow_new = next_flow;
        }
        LOG(log,3) << "[queue " << id << "] ingress: " << head
                   << " (flow " << ingress_flow;
        if (next_flow != ingress_flow) {
            new_flit = head_flit(next_flow, head.get_length());
            LOG(log,3) << " -> " << next_flow;
        }
        LOG(log,3) << ", #" << hex << setfill('0') << setw(8)
                   << new_flit.get_uid() << ")" << endl;
    } else {
        --ingress_remaining;
        LOG(log,3) << "[queue " << id << "] ingress: " << f
                   << " (flow " << ingress_flow << ", #"
                   << hex << setfill('0') << setw(8)
                   << new_flit.get_uid() << ")" << endl;
    }
    if (egress_vq.is_valid() && q.empty()) { // continuing flit gets to vq head
        pressures->inc(next_node, egress_vq);
    }
    q.push(make_tuple(new_flit, next_node, ingress_flow, next_flow));
}

void virtual_queue::pop() throw(err) {
    assert(!empty());
    flit f(0); node_id n; flow_id of; flow_id nf; tie(f,n,of,nf) = q.front();
    alloc->dealloc();
    if (egress_remaining == 0) {
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        egress_flow_old = of; egress_flow_new = nf;
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
    q.pop();
    if (egress_remaining == 0) {
        vc_alloc->release(make_tuple(n, egress_vq));
        assert(old_flows.find(egress_flow_old) != old_flows.end());
        --old_flows[egress_flow_old];
        egress_vq = virtual_queue_id(); // invalid vqid
    }
    // decrease pressure if next flit is new flow, or VC is empty
    if (!egress_vq.is_valid() || q.empty()) {
        pressures->dec(n, egress_vq);
    }
    --stale_size;
}

flow_id virtual_queue::get_egress_old_flow_id() const throw (err) {
    assert(!empty());
    if (egress_remaining == 0) {
        flit f(0); node_id n; flow_id of; flow_id nf;
        tie(f,n,of,nf) = q.front();
        return of;
    } else {
        return egress_flow_old;
    }
}

flow_id virtual_queue::get_egress_new_flow_id() const throw (err) {
    assert(!empty());
    if (egress_remaining == 0) {
        flit f(0); node_id n; flow_id of; flow_id nf;
        tie(f,n,of,nf) = q.front();
        return nf;
    } else {
        return egress_flow_new;
    }
}

uint32_t virtual_queue::get_egress_flow_length() const throw (err) {
    assert(!empty());
    if (egress_remaining == 0) {
        flit f(0); node_id n; flow_id of; flow_id nf;
        tie(f,n,of,nf) = q.front();
        const head_flit &head = reinterpret_cast<const head_flit &>(f);
        return head.get_length();
    } else {
        return egress_remaining;
    }
}

void virtual_queue::set_front_vq_id(const virtual_queue_id &vqid) throw(err) {
    assert(!empty());
    assert(!egress_vq.is_valid());
    assert(vqid.is_valid());
    flit f(0); node_id n; flow_id of; flow_id nf; tie(f,n,of,nf) = q.front();
    LOG(log,3) << "[queue " << id << "] granted next hop "
               << make_tuple(n, vqid) << " (flow ";
    if (egress_flow_new == egress_flow_old) {
        LOG(log,3) << egress_flow_old;
    } else {
        LOG(log,3) << egress_flow_old << "->" << egress_flow_new;
    }
    LOG(log,3) << ")" << endl;
    vc_alloc->claim(make_tuple(n, vqid));
    pressures->inc(n, vqid); // increase pressure for a head flit
    egress_vq = vqid;
}

bool virtual_queue::has_old_flow(const flow_id &flow) const throw(err) {
    map<flow_id, unsigned>::const_iterator i = old_flows.find(flow);
    return ((i != old_flows.end()) && (i->second > 0));
}
