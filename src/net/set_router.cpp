// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "error.hpp"
#include "random.hpp"
#include "set_router.hpp"

set_router::set_router(node_id i, logger &l) throw() : router(i,l), routes() { }

set_router::~set_router() throw() { }

node_id set_router::route(node_id src, flow_id f) throw(err) {
    route_query_t rq = route_query_t(src, f);
    routes_t::iterator ri = routes.find(rq);
    if (ri == routes.end())
        throw exc_bad_flow_from(id.get_numeric_id(), src.get_numeric_id(),
                                f.get_numeric_id());
    const route_nodes_t &nodes = ri->second;
    assert(!nodes.empty());
    double r = random_range_double(nodes.back().get<1>());
    node_id dst(0xdeadbeef);
    for (route_nodes_t::const_iterator ni = nodes.begin();
         ni != nodes.end(); ++ni) {
        node_id n; double prop; tie(n,prop) = *ni;
        if (r < prop) {
            dst = n;
            break;
        }
    }
    return dst;
}

void set_router::add_route(const node_id &src, const flow_id &f,
                           const route_nodes_t &dsts) throw(err) {
    assert(!dsts.empty());
    route_query_t rq = route_query_t(src, f);
    if (routes.find(rq) != routes.end()) {
        throw err_duplicate_flow(get_id().get_numeric_id(),
                                 f.get_numeric_id());
    }
    double prop_sum = 0.0;
    for (route_nodes_t::const_iterator di = dsts.begin();
         di != dsts.end(); ++di) {
        node_id n; double prop; tie(n,prop) = *di;
        assert(prop > 0);
        prop_sum += prop;
        routes[rq].push_back(make_tuple(n, prop_sum));
    }
    if (dsts.size() == 1) {
        node_id n; double prop; tie(n,prop) = dsts.front();
        LOG(log,4) << "router " << get_id() << " routing flow " << f
                   << " from node " << src << " to node " << n << endl;
    } else {
        LOG(log,4) << "router " << get_id() << " routing flow " << f
                   << " from node " << src << " to nodes ";
        for (route_nodes_t::const_iterator di = dsts.begin();
             di != dsts.end(); ++di) {
            node_id n; double prop; tie(n,prop) = *di;
            LOG(log,4) << (di == dsts.begin() ? "" : ", ")
                       << n << dec << fixed << setprecision(4)
                       << " (" << (prop * 100) << "%)";
        }
        LOG(log,4) << endl;
    }
}
