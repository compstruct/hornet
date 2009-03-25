// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "egress.hpp"

ostream &operator<<(ostream &out, const egress_id &id) {
    return out << id.parent << ":" << id.name;
}

egress::egress(egress_id new_id, shared_ptr<ingress> new_tgt,
               shared_ptr<pressure_tracker> pt, unsigned bw, logger &l)
    throw(err)
    : id(new_id), target_id(new_tgt->get_id().get_node_id()), target(new_tgt),
      pressures(pt), bandwidth(bw), log(l) {
    const ingress::queues_t &qs = target->get_queues();
    for (ingress::queues_t::const_iterator qi = qs.begin(); qi != qs.end();
         ++qi) {
        node_id n; virtual_queue_id q; tie(n, q) = qi->second->get_id();
        pt->add_egress_queue(n, q);
    }
}

const egress_id &egress::get_id() const throw() { return id; }

unsigned egress::get_bandwidth() const throw() { return bandwidth; }

void egress::set_bandwidth(unsigned b) throw() { bandwidth = b; }

pressure_t egress::get_pressure() const throw() {
    const ingress::queues_t &qs = get_remote_queues();
    pressure_t pressure = 0;
    for (ingress::queues_t::const_iterator qi = qs.begin(); qi != qs.end();
         ++qi) {
        node_id n; virtual_queue_id q; tie(n,q) = qi->second->get_id();
        assert(n == target_id);
        pressure += pressures->get(n, q);
    }
    return pressure;
}

const ingress::queues_t &egress::get_remote_queues() const throw() {
    return target->get_queues();
}

void egress::tick_positive_edge() throw(err) { }

void egress::tick_negative_edge() throw(err) { }
