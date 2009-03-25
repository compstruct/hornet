// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include "pressure_tracker.hpp"

pressure_tracker::pressure_tracker(const node_id &parent, logger &l) throw()
    : id(parent), log(l) { }

void pressure_tracker::inc(const node_id &n,
                           const virtual_queue_id &q) throw(err) {
    tuple<node_id, virtual_queue_id> tgt = make_tuple(n,q);
    if (pressures.find(tgt) == pressures.end())
        throw err_bad_neighbor(id.get_numeric_id(), n.get_numeric_id());
    pressures[tgt]++;
}

void pressure_tracker::dec(const node_id &n,
                           const virtual_queue_id &q) throw(err) {
    tuple<node_id, virtual_queue_id> tgt = make_tuple(n,q);
    if (pressures.find(tgt) == pressures.end())
        throw err_bad_neighbor(id.get_numeric_id(), n.get_numeric_id());
    assert(pressures[tgt] > 0);
    pressures[tgt]--;
}

pressure_t pressure_tracker::get(const node_id &n, const virtual_queue_id &q)
    throw(err) {
    tuple<node_id, virtual_queue_id> tgt = make_tuple(n,q);
    if (pressures.find(tgt) == pressures.end())
        throw err_bad_neighbor(get_id().get_numeric_id(), n.get_numeric_id());
    return pressures[tgt];
}

void pressure_tracker::add_egress_queue(const node_id &n,
                                        const virtual_queue_id &q) throw(err) {
    tuple<node_id, virtual_queue_id> tgt = make_tuple(n,q);
    if (pressures.find(tgt) != pressures.end())
        throw err_duplicate_egress(get_id().get_numeric_id(),
                                   n.get_numeric_id());
    pressures[tgt] = 0;
}
