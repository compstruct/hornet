// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATIC_ROUTER_HPP__
#define __STATIC_ROUTER_HPP__

#include <map>
#include "router.hpp"

class static_router : public router {
public:
    static_router(node_id id, logger &log) throw();
    virtual ~static_router() throw();
    virtual node_id route(flow_id flow) throw(err);
    void add_route(flow_id flow, node_id n) throw(err);
private:
    typedef map<flow_id, node_id> routes_t;
    routes_t routes;
};

#endif // __STATIC_ROUTER_HPP__

