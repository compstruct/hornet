// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATIC_ROUTER_HPP__
#define __STATIC_ROUTER_HPP__

#include <map>
#include "boost/tuple/tuple.hpp"
#include "boost/tuple/tuple_comparison.hpp"
#include "router.hpp"

using namespace boost;

class static_router : public router {
public:
    static_router(node_id id, logger &log) throw();
    virtual ~static_router() throw();
    virtual node_id route(node_id src_node_id, flow_id flow) throw(err);
    void add_route(node_id prev, flow_id flow, node_id n) throw(err);
private:
    typedef tuple<node_id, flow_id> route_query_t;
    typedef map<route_query_t, node_id> routes_t;
    routes_t routes;
};

#endif // __STATIC_ROUTER_HPP__

