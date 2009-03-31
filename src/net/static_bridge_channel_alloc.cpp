// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "static_bridge_channel_alloc.hpp"

static_bridge_channel_alloc::static_bridge_channel_alloc(node_id n,
                                                         logger &l) throw()
    : bridge_channel_alloc(n, l), routes() { }

static_bridge_channel_alloc::~static_bridge_channel_alloc() throw() { }

void static_bridge_channel_alloc::add_route(flow_id f,
                                            virtual_queue_id q) throw(err) {
    if (routes.find(f) != routes.end())
        throw err_duplicate_flow(get_id().get_numeric_id(),
                                 f.get_numeric_id());
    LOG(log,4) << "bridge channel alloc " << get_id() << " routing flow " << f
        << " to queue " << q << endl;
    routes[f] = q;
}

virtual_queue_id static_bridge_channel_alloc::request(flow_id f) throw(err) {
    if (routes.find(f) == routes.end())
        throw exc_bad_flow(get_id().get_numeric_id(), f.get_numeric_id());
    virtual_queue_id &q = routes[f];
    if (!is_claimed(make_tuple(get_id(),q))) {
        return q;
    } else {
        return virtual_queue_id(); // invalid ID
    }
}
