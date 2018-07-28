// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "channel_alloc.hpp"

channel_alloc::channel_alloc(node_id new_id, bool one_q_per_f,
                             bool one_f_per_q, logger &l)
    : id(new_id), one_queue_per_flow(one_q_per_f),
      one_flow_per_queue(one_f_per_q), ingresses(), in_use(), log(l) { }

channel_alloc::~channel_alloc() { }

bool channel_alloc::is_claimed(const virtual_queue_node_id &q) {
    return in_use.find(q) != in_use.end();
}

void channel_alloc::claim(const virtual_queue_node_id &q) {
    assert(!is_claimed(q));
    in_use.insert(q);
}

void channel_alloc::release(const virtual_queue_node_id &q) {
    assert(is_claimed(q));
    in_use.erase(q);
}

void channel_alloc::add_ingress(std::shared_ptr<ingress> ing) {
    ingresses.push_back(ing);
}
