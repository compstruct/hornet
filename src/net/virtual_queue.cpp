// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include "virtual_queue.hpp"

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
      alloc(new_alloc), stale_size(0), increased_pressure(false), log(l) { }

ostream &operator<<(ostream &out, const tuple<node_id, virtual_queue_id> &id) {
    return out << id.get<0>() << ":" << id.get<1>();
}

