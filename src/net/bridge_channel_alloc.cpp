// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "bridge_channel_alloc.hpp"

bridge_channel_alloc::bridge_channel_alloc(node_id new_id, logger &l)
    throw() : id(new_id), in_use(), log(l) { }

bridge_channel_alloc::~bridge_channel_alloc() throw() { }

bool bridge_channel_alloc::is_claimed(const virtual_queue_node_id &q)
    throw(err) {
    return in_use.find(q) != in_use.end();
}

void bridge_channel_alloc::claim(const virtual_queue_node_id &q) throw(err) {
    assert(!is_claimed(q));
    in_use.insert(q);
}

void bridge_channel_alloc::release(const virtual_queue_node_id &q) throw(err) {
    assert(is_claimed(q));
    in_use.erase(q);
}
