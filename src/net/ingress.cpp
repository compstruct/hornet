// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "ingress.hpp"

ostream &operator<<(ostream &out, const ingress_id &id) {
    return out << id.parent << ":" << id.name;
}

ingress::ingress(const ingress_id &new_id, const set<virtual_queue_id> &vq_ids,
                 unsigned flits_per_queue, shared_ptr<router> rt,
                 shared_ptr<channel_alloc> vca, shared_ptr<pressure_tracker> pt,
                 logger &l) throw(err) : id(new_id), vqs(), log(l) {
    const node_id &parent_id = id.get_node_id().get_numeric_id();
    for (set<virtual_queue_id>::const_iterator i = vq_ids.begin();
         i != vq_ids.end(); ++i) {
        shared_ptr<common_alloc> alloc = 
            shared_ptr<common_alloc>(new common_alloc(flits_per_queue));
        if (vqs.find(*i) != vqs.end())
            throw err_duplicate_queue(parent_id.get_numeric_id(),
                                      i->get_numeric_id());
        shared_ptr<virtual_queue> q(new virtual_queue(parent_id, *i, rt,
                                                      vca, pt, alloc, log));
        vqs[q->get_id().get<1>()] = q;
    }
}

void ingress::add_queue(shared_ptr<virtual_queue> vq) throw(err) {
    if (vqs.find(vq->get_id().get<1>()) != vqs.end())
        throw err_duplicate_queue(id.get_node_id().get_numeric_id(),
                                  vq->get_id().get<1>().get_numeric_id());
    assert(next_hops.find(vq->get_id().get<1>()) == next_hops.end());
    vqs[vq->get_id().get<1>()] = vq;
    next_hops[vq->get_id().get<1>()] = make_tuple(node_id(), virtual_queue_id());
}

void ingress::tick_positive_edge() throw(err) {
    for (queues_t::iterator qi = vqs.begin(); qi != vqs.end(); ++qi) {
        qi->second->tick_positive_edge();
    }
    LOG(log,11) << *this;
}

void ingress::tick_negative_edge() throw(err) {
    for (queues_t::iterator qi = vqs.begin(); qi != vqs.end(); ++qi) {
        qi->second->tick_negative_edge();
    }
}

ostream &operator<<(ostream &out, const ingress &i) {
    out << "[port " << i.get_id() << "] ingress " << endl;
    for (ingress::queues_t::const_iterator qi = i.vqs.begin();
         qi != i.vqs.end(); ++qi) {
        out << "[port " << i.get_id() << "]     queue "
            << qi->first << " has length " << qi->second->size() << endl;
    }
    return out;
}

