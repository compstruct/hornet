// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "ingress.hpp"

ingress::ingress(const ingress_id &new_id, const node_id &new_src_node_id,
                 const set<virtual_queue_id> &vq_ids,
                 unsigned flits_per_queue, unsigned xbar_bw,
                 std::shared_ptr<channel_alloc> vca,
                 std::shared_ptr<pressure_tracker> pt,
                 std::shared_ptr<tile_statistics> st,
                 std::shared_ptr<vcd_writer> vcd,
                 logger &l)
    : id(new_id), bw_to_xbar(xbar_bw), src_node_id(new_src_node_id), vqs(),
      stats(st), log(l) {
    const node_id &parent_id = id.get_node_id().get_numeric_id();
    for (set<virtual_queue_id>::const_iterator i = vq_ids.begin();
         i != vq_ids.end(); ++i) {
        if (vqs.find(*i) != vqs.end())
            throw err_duplicate_queue(parent_id.get_numeric_id(),
                                      i->get_numeric_id());
        std::shared_ptr<virtual_queue>
            q(new virtual_queue(parent_id, *i, src_node_id, new_id,
                                flits_per_queue,
                                vca, pt, stats, vcd, log));
        vqs[get<1>(q->get_id())] = q;
    }
}

void ingress::add_queue(std::shared_ptr<virtual_queue> vq) {
    if (vqs.find(get<1>(vq->get_id())) != vqs.end())
        throw err_duplicate_queue(id.get_node_id().get_numeric_id(),
                                  get<1>(vq->get_id()).get_numeric_id());
    assert(next_hops.find(get<1>(vq->get_id())) == next_hops.end());
    vqs[get<1>(vq->get_id())] = vq;
    next_hops[get<1>(vq->get_id())] =
        make_tuple(node_id(), virtual_queue_id());
}

void ingress::tick_positive_edge() {
    for (queues_t::iterator qi = vqs.begin(); qi != vqs.end(); ++qi) {
        qi->second->tick_positive_edge();
    }
    LOG(log,11) << *this;
}

void ingress::tick_negative_edge() {
    for (queues_t::iterator qi = vqs.begin(); qi != vqs.end(); ++qi) {
        qi->second->tick_negative_edge();
    }
}

bool ingress::is_drained() const {
    bool drained = true;
    for (queues_t::const_iterator qi = vqs.begin(); qi != vqs.end(); ++qi) {
        drained &= qi->second->is_drained();
    }
    return drained;
}

ostream &operator<<(ostream &out, const ingress &i) {
    out << "[port " << i.get_id() << "] ingress " << endl;
    for (ingress::queues_t::const_iterator qi = i.vqs.begin();
         qi != i.vqs.end(); ++qi) {
        out << *qi->second;
    }
    return out;
}

