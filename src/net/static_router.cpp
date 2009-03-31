// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "error.hpp"
#include "static_router.hpp"

static_router::static_router(node_id i, logger &l) throw()
    : router(i, l), routes() { }

static_router::~static_router() throw() { }

node_id static_router::route(node_id src, flow_id f) throw(err) {
    route_query_t rq = route_query_t(src, f);
    routes_t::iterator ri = routes.find(rq);
    if (ri == routes.end())
        throw exc_bad_flow_from(id.get_numeric_id(), src.get_numeric_id(),
                                f.get_numeric_id());
    return ri->second;
}

void static_router::add_route(node_id src, flow_id f,
                              node_id dst) throw(err) {
    route_query_t rq = route_query_t(src, f);
    if (routes.find(rq) != routes.end()) {
        throw err_duplicate_flow(get_id().get_numeric_id(),
                                 f.get_numeric_id());
    }
    LOG(log,4) << "router " << get_id() << " routing flow " << f
               << " from node " << src << " to node " << dst << endl;
    routes[rq] = dst;
}
