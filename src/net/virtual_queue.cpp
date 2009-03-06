// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include "virtual_queue.hpp"

common_alloc::common_alloc(unsigned max_slots) throw()
    : size(max_slots), max_size(max_slots) { }

virtual_queue::virtual_queue(node_id new_node_id, virtual_queue_id new_vq_id,
                             shared_ptr<common_alloc> new_alloc,
                             shared_ptr<logger> new_log) throw()
    : queue<flit>(), id(make_pair(new_node_id, new_vq_id)),
      stale_size(0), ingress_remaining(0), ingress_flow(0),
      egress_remaining(0), egress_flow(0), alloc(new_alloc), log(new_log) { }

void virtual_queue::claim(const node_id &target) throw(err) {
    if (claimed) throw err_claimed_queue(id.first.get_numeric_id(),
                                         target.get_numeric_id());
    claimed = true;
}

ostream &operator<<(ostream &out, const node_id &id) {
    return out << hex << setfill('0') << setw(2) << id.id;
}

ostream &operator<<(ostream &out, const virtual_queue_id &id) {
    return out << hex << setfill('0') << setw(2) << id.id;
}

ostream &operator<<(ostream &out, const pair<node_id, virtual_queue_id> &id) {
    return out << id.first << ":" << id.second;
}

