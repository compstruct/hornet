// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "error.hpp"
#include "static_router.hpp"

static_router::static_router(node_id i, logger &l) throw()
    : router(i, l), routes() { }

static_router::~static_router() throw() { }

node_id static_router::route(flow_id flow) throw(err) {
    routes_t::iterator ri = routes.find(flow);
    if (ri == routes.end())
        throw exc_bad_flow(id.get_numeric_id(), flow.get_numeric_id());
    return ri->second;
}

void static_router::add_route(flow_id f, node_id n) throw(err) {
    if (routes.find(f) != routes.end()) {
        throw err_duplicate_flow(get_id().get_numeric_id(),
                                 f.get_numeric_id());
    }
    log << verbosity(4) << "router " << get_id() << " routing flow " << f
        << " to node " << n << endl;
    routes[f] = n;
}

