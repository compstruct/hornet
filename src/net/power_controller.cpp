// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "power_controller.hpp"

power_controller::power_controller(node_id new_id,
                                   shared_ptr<tile_statistics> new_stats,
                                   shared_ptr<vcd_writer> new_vcd,
                                   logger &new_log) throw()
    : id(new_id), stats(new_stats), vcd(new_vcd), log(new_log) { }

const node_id &power_controller::get_id() const throw() { return id; }

void power_controller::add_ingress(node_id src,
                                   shared_ptr<ingress> ing) throw(err) {
    if (ingresses.find(src) != ingresses.end()) {
        throw err_duplicate_ingress(get_id().get_numeric_id(),
                                    src.get_numeric_id());
    }
    ingresses[src] = ing;
}

void power_controller::add_egress(node_id dst,
                                  shared_ptr<egress> egress) throw(err) { }

void power_controller::adjust_power() throw(err) {
}

