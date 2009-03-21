// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include "static_channel_alloc.hpp"

static_channel_alloc::static_channel_alloc(node_id n, logger &l) throw()
    : channel_alloc(n, l), routes(), in_use() { }

static_channel_alloc::~static_channel_alloc() throw() { }

virtual_queue_id static_channel_alloc::request(node_id n, flow_id f)
    throw(err) {
    if (routes.find(n) == routes.end())
        throw err_bad_neighbor(get_id().get_numeric_id(), n.get_numeric_id());
    map<flow_id, virtual_queue_id> &f2vq = routes[n];
    if (f2vq.find(f) == f2vq.end())
        throw exc_bad_flow(get_id().get_numeric_id(), f.get_numeric_id());
    virtual_queue_id &q = f2vq[f];
    if (in_use.find(q) != in_use.end()) {
        return virtual_queue_id(); // invalid vq ID
    } else {
        in_use.insert(q);
        return q;
    }
}

void static_channel_alloc::release(virtual_queue_id q) throw(err) {
    assert(in_use.find(q) != in_use.end());
    in_use.erase(q);
}

void static_channel_alloc::add_route(node_id dst, flow_id f,
                                     virtual_queue_id q) throw(err) {
    map<flow_id, virtual_queue_id> &f2vq = routes[dst];
    if (f2vq.find(f) != f2vq.end())
        throw err_duplicate_flow(get_id().get_numeric_id(), f.get_numeric_id());
    LOG(log,4) << "channel alloc " << get_id() << " routing flow " << f
        << " on node " << dst << " to queue " << q << endl;
    f2vq[f] = q;
}

