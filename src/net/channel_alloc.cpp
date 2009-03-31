// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "channel_alloc.hpp"

channel_alloc::channel_alloc(node_id new_id, logger &l)
    throw() : id(new_id), ingresses(), in_use(), log(l) { }

channel_alloc::~channel_alloc() throw() { }

bool channel_alloc::is_claimed(const virtual_queue_node_id &q) throw(err) {
    return in_use.find(q) != in_use.end();
}

void channel_alloc::claim(const virtual_queue_node_id &q) throw(err) {
    assert(!is_claimed(q));
    in_use.insert(q);
}

void channel_alloc::release(const virtual_queue_node_id &q) throw(err) {
    assert(is_claimed(q));
    in_use.erase(q);
}

void channel_alloc::add_ingress(shared_ptr<ingress> ing) throw(err) {
    ingresses.push_back(ing);
}
