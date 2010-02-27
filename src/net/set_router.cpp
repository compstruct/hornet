// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "error.hpp"
#include "random.hpp"
#include "set_router.hpp"

set_router::set_router(node_id i, logger &l, shared_ptr<BoostRand> r) throw() 
    : router(i,l), routes(), ran(r) { }

set_router::~set_router() throw() { }

void set_router::add_egress(shared_ptr<egress> egress) throw(err) { }

void set_router::route() throw(err) {
    for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end();
         ++ii) {
        const node_id src = (*ii)->get_src_node_id();
        const ingress::queues_t &iqs = (*ii)->get_queues();
        for (ingress::queues_t::const_iterator qi = iqs.begin();
             qi != iqs.end(); ++qi) {
            if (!qi->second->front_is_empty()
                && qi->second->front_is_head_flit()
                && !qi->second->front_node_id().is_valid()) {
                assert(!qi->second->front_new_flow_id().is_valid());
                assert(!qi->second->front_vq_id().is_valid());
                const flow_id f = qi->second->front_old_flow_id();
                route_query_t rq = route_query_t(src, f);
                routes_t::iterator ri = routes.find(rq);
                if (ri == routes.end()) {
                    throw exc_bad_flow_from(id.get_numeric_id(),
                                            src.get_numeric_id(),
                                            f.get_numeric_id());
                }
                const route_nodes_t &nodes = ri->second;
                assert(!nodes.empty());
                double r = ran->random_range_double(nodes.back().get<2>());
                node_id dst_n;
                flow_id dst_f;
                for (route_nodes_t::const_iterator ni = nodes.begin();
                     ni != nodes.end(); ++ni) {
                    node_id n; flow_id nf; double prop; tie(n,nf,prop) = *ni;
                    if (r < prop) {
                        dst_n = n;
                        dst_f = nf;
                        break;
                    }
                }
                qi->second->front_set_next_hop(dst_n, dst_f);
            }
        }
    }
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
        node_id n; flow_id nf; double prop; tie(n,nf,prop) = *di;
        assert(prop > 0);
        prop_sum += prop;
        routes[rq].push_back(make_tuple(n, nf, prop_sum));
    }
    if (dsts.size() == 1) {
        node_id n; flow_id nf; double prop; tie(n,nf,prop) = dsts.front();
        LOG(log,4) << "router " << get_id() << " routing flow " << f
                   << " from node " << src << " to node " << n;
        if (nf != f) LOG(log,4) << " as " << nf;
        LOG(log,4) << endl;
    } else {
        LOG(log,4) << "router " << get_id() << " routing flow " << f
                   << " from node " << src << " to nodes ";
        for (route_nodes_t::const_iterator di = dsts.begin();
             di != dsts.end(); ++di) {
            node_id n; flow_id nf; double prop; tie(n,nf,prop) = *di;
            LOG(log,4) << (di == dsts.begin() ? "" : ", ")
                       << n;
            if (nf != f) LOG(log,4) << " as " << nf;
            LOG(log,4) << dec << fixed << setprecision(4)
                       << " (" << (prop * 100) << "%)";
        }
        LOG(log,4) << endl;
    }
}
