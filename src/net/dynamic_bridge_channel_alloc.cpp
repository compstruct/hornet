// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "random.hpp"
#include "dynamic_bridge_channel_alloc.hpp"

dynamic_bridge_channel_alloc::dynamic_bridge_channel_alloc(node_id n,
                                                           logger &l) throw()
    : bridge_channel_alloc(n, l), queues() { }

dynamic_bridge_channel_alloc::~dynamic_bridge_channel_alloc() throw() { }

void dynamic_bridge_channel_alloc::add_queue(shared_ptr<virtual_queue> q)
    throw(err) {
    queues.push_back(q);
}

virtual_queue_id dynamic_bridge_channel_alloc::request(flow_id f) throw(err) {
    random_shuffle(queues.begin(), queues.end(), random_range);
    for (queues_t::iterator qi = queues.begin(); qi != queues.end(); ++qi) {
        virtual_queue_id q = (*qi)->get_id().get<1>();
        if (!is_claimed(make_tuple(get_id(), q))) {
            return q;
        }
    }
    return virtual_queue_id(); // invalid ID
}
