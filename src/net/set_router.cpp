// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include "error.hpp"
#include "random.hpp"
#include "set_router.hpp"

set_router::set_router(node_id i, logger &l, std::shared_ptr<random_gen> r) 
    : router(i,l), routes(), ran(r) { }

set_router::~set_router() { }

void set_router::add_egress(std::shared_ptr<egress> eg) { 
    assert(egresses.find(eg->get_target_id()) == egresses.end());
    egresses[eg->get_target_id()] = eg;
}

void set_router::route() {
    map<uint32_t, int> next_hop_cost;
    for (egresses_t::const_iterator ei = egresses.begin(); ei != egresses.end(); ++ei) {
        next_hop_cost[ei->first.get_numeric_id()] = 0;
        if (multi_path_routing() == RT_ADAPTIVE_QUEUE) {
            /* add -1 for each available queues per egress port */
            const ingress::queues_t &eqs = ei->second->get_remote_queues();
            for (ingress::queues_t::const_iterator eqi = eqs.begin(); eqi != eqs.end(); ++eqi) {
                const std::shared_ptr<virtual_queue> &eq = eqi->second;
                if (!m_vca->is_claimed(eq->get_id()) &&  
                    !eq->back_is_full() && !eq->back_is_mid_packet()) {
                    --next_hop_cost[ei->first.get_numeric_id()];
                }
            }
        } else if (multi_path_routing() == RT_ADAPTIVE_PACKET) {
            /* add +1 for each head packet routed for the egress port */
            for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end(); ++ii) {
                const ingress::queues_t &iqs = (*ii)->get_queues();
                for (ingress::queues_t::const_iterator qi = iqs.begin(); qi != iqs.end(); ++qi) {
                    if (!qi->second->front_is_empty()) {
                        if (qi->second->front_vq_id().is_valid() ) {
                            ++next_hop_cost[qi->second->front_node_id().get_numeric_id()];
                        }
                    }
                }
            }
        }
    }

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
                node_id dst_n;
                flow_id dst_f;
                int min_cost = INT_MAX;
                double prop_sum = 0.0f;
                route_nodes_t next_hops;
                for (route_nodes_t::const_iterator ni = nodes.begin(); ni != nodes.end(); ++ni) {
                    node_id n = get<0>(*ni);
                    flow_id nf = get<1>(*ni);
                    double prop = get<2>(*ni);
                    if (min_cost > next_hop_cost[n.get_numeric_id()]) {
                        next_hops.clear();
                        min_cost = next_hop_cost[n.get_numeric_id()];
                        prop_sum = prop;
                        next_hops.push_back(make_tuple(n, nf, prop_sum));
                    } else if (min_cost == next_hop_cost[n.get_numeric_id()]) {
                        prop_sum += prop;
                        next_hops.push_back(make_tuple(n, nf, prop_sum));
                    }
                }
                assert(!next_hops.empty());
                double r = ran->random_range_double(get<2>(next_hops.back()));
                for (route_nodes_t::const_iterator ni = next_hops.begin();
                     ni != next_hops.end(); ++ni) {
                    node_id n = get<0>(*ni);
                    flow_id nf = get<1>(*ni);
                    double prop = get<2>(*ni);
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
                           const route_nodes_t &dsts) {
    assert(!dsts.empty());
    route_query_t rq = route_query_t(src, f);
    if (routes.find(rq) != routes.end()) {
        throw err_duplicate_flow(get_id().get_numeric_id(),
                                 f.get_numeric_id());
    }
    double prop_sum = 0.0;
    for (route_nodes_t::const_iterator di = dsts.begin();
         di != dsts.end(); ++di) {
        node_id n = get<0>(*di);
        flow_id nf = get<1>(*di);
        double prop = get<2>(*di);
        assert(prop > 0);
        prop_sum += prop;
        routes[rq].push_back(make_tuple(n, nf, prop_sum));
    }
    if (dsts.size() == 1) {
        node_id n = get<0>(dsts.front());
        flow_id nf = get<1>(dsts.front());
        LOG(log,4) << "router " << get_id() << " routing flow " << f
                   << " from node " << src << " to node " << n;
        if (nf != f) LOG(log,4) << " as " << nf;
        LOG(log,4) << endl;
    } else {
        LOG(log,4) << "router " << get_id() << " routing flow " << f
                   << " from node " << src << " to nodes ";
        for (route_nodes_t::const_iterator di = dsts.begin();
             di != dsts.end(); ++di) {
            node_id n = get<0>(*di);
            flow_id nf = get<1>(*di);
            double prop = get<2>(*di);
            LOG(log,4) << (di == dsts.begin() ? "" : ", ")
                       << n;
            if (nf != f) LOG(log,4) << " as " << nf;
            LOG(log,4) << dec << fixed << setprecision(4)
                       << " (" << (prop * 100) << "%)";
        }
        LOG(log,4) << endl;
    }
}
