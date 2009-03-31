// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "egress.hpp"

egress::egress(egress_id new_id, shared_ptr<ingress> new_tgt,
               shared_ptr<pressure_tracker> pt, unsigned bw, logger &l)
    throw(err)
    : id(new_id), target_id(new_tgt->get_id().get_node_id()), target(new_tgt),
      pressures(pt), bandwidth(bw), log(l) {
    pt->add_egress(target_id);
}

const egress_id &egress::get_id() const throw() { return id; }

const node_id &egress::get_target_id() const throw() { return target_id; }

unsigned egress::get_bandwidth() const throw() { return bandwidth; }

void egress::set_bandwidth(unsigned b) throw() { bandwidth = b; }

double egress::get_pressure() const throw() {
    return pressures->get(target_id);
}

const ingress::queues_t &egress::get_remote_queues() const throw() {
    return target->get_queues();
}
