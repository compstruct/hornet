// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include "dynamic_channel_alloc.hpp"

dynamic_channel_alloc::dynamic_channel_alloc(node_id n, logger &l) throw()
    : channel_alloc(n, l), egresses(), in_use() { }

dynamic_channel_alloc::~dynamic_channel_alloc() throw() { }

virtual_queue_id dynamic_channel_alloc::request(node_id n, flow_id)
    throw(err) {
    egress_queues_t::iterator eqi;
    if ((eqi = egresses.find(n)) == egresses.end()) {
        cerr << "bad dynalloc" << endl;
        throw err_bad_neighbor(get_id().get_numeric_id(), n.get_numeric_id());
    }
    vq_deque_t &qs = eqi->second;
    virtual_queue_id q = virtual_queue_id(); // invalid vq ID
    vq_deque_t::iterator qi;
    int shift = 1; // how much to shift queue for round-robin
    for (qi = qs.begin(); !q.is_valid() && qi != qs.end(); ++qi, ++shift) {
        if ((*qi)->ingress_new_flow()
            && in_use.find((*qi)->get_id().get<1>()) == in_use.end()) {
            q = (*qi)->get_id().get<1>();
            in_use.insert(q);
        }
    }
    for (int i = 0; i < shift; ++i) {
        qs.push_back(qs.front()); qs.pop_front();
    }
    return q;
}

void dynamic_channel_alloc::release(virtual_queue_id q) throw(err) {
    assert(in_use.find(q) != in_use.end());
    in_use.erase(q);
}

void dynamic_channel_alloc::add_egress(node_id dst, shared_ptr<egress> egr)
    throw(err) {
    if (egresses.find(dst) != egresses.end()) {
        throw err_duplicate_egress(get_id().get_numeric_id(),
                                   dst.get_numeric_id());
    }
    egresses[dst] = vq_deque_t();
    const ingress::queues_t &qs = egr->get_remote_queues();
    for (ingress::queues_t::const_iterator qi = qs.begin();
         qi != qs.end(); ++qi) {
        egresses[dst].push_back(qi->second);
    }
}

