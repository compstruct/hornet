// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <algorithm>
#include <iomanip>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "set_channel_alloc.hpp"
#include "random.hpp"

set_channel_alloc::set_channel_alloc(node_id n, bool one_q_per_f,
                                     bool one_f_per_q,
                                     std::shared_ptr<tile_statistics> s,
                                     logger &l,
                                     std::shared_ptr<random_gen> r)
    : channel_alloc(n, one_q_per_f, one_f_per_q, l), egresses(), 
      stats(s),ran(r) {
    if (one_q_per_f || one_f_per_q) {
        LOG(log,3) << "node " << get_id() << " set channel alloc: ";
        if (one_q_per_f && !one_f_per_q) {
            LOG(log,3) << "one queue per flow";
        } else if (!one_q_per_f && one_f_per_q) {
            LOG(log,3) << "one flow per queue";
        } else {
            LOG(log,3) << "one queue per flow, one flow per queue";
        }
        LOG(log,3) << endl;
    }
}

set_channel_alloc::~set_channel_alloc() { }

void set_channel_alloc::add_egress(std::shared_ptr<egress> egr) {
    assert(egresses.find(egr->get_target_id()) == egresses.end());
    egresses[egr->get_target_id()] = egr;
}

void
set_channel_alloc::add_route(const node_id &src, const flow_id &f,
                             const node_id &dst, const flow_id &nf,
                             const vector<std::tuple<virtual_queue_id,double> > &qis)
    {
    if (egresses.find(dst) == egresses.end())
        throw err_bad_next_hop(get_id().get_numeric_id(), f.get_numeric_id(),
                               dst.get_numeric_id());
    const ingress::queues_t &eqs = egresses[dst]->get_remote_queues();
    route_query_t rq = route_query_t(src,f,dst,nf);
    assert(routes.find(rq) == routes.end());
    if (!qis.empty()) { // add only specified queues with given propensities
        for (vector<std::tuple<virtual_queue_id,double> >::const_iterator idi =
                 qis.begin(); idi != qis.end(); ++idi) {
            virtual_queue_id vqid = get<0>(*idi);
            double prop = get<1>(*idi);
            assert(prop > 0);
            ingress::queues_t::const_iterator eqi = eqs.find(vqid);
            if (eqi == eqs.end())
                throw err_bad_next_hop_queue(get_id().get_numeric_id(),
                                             f.get_numeric_id(),
                                             dst.get_numeric_id(),
                                             vqid.get_numeric_id());
            routes[rq].push_back(make_tuple(eqi->second, prop));
        }
    } else { // add all egress queues with equal propensity
        for (ingress::queues_t::const_iterator eqi = eqs.begin();
             eqi != eqs.end(); ++eqi) {
            routes[rq].push_back(make_tuple(eqi->second, 1.0));
        }
    }
}

void set_channel_alloc::allocate() {
    int num_free_eqs = 0;
    for (egresses_t::const_iterator ei = egresses.begin();
         ei != egresses.end(); ++ei) {
        const ingress::queues_t &eqs = ei->second->get_remote_queues();
        for (ingress::queues_t::const_iterator eqi = eqs.begin();
             eqi != eqs.end(); ++eqi) {
            const std::shared_ptr<virtual_queue> &eq = eqi->second;
            if (!is_claimed(eq->get_id()) && !eq->back_is_mid_packet()) {
                ++num_free_eqs;
            }
        }
    }
    typedef vector<std::shared_ptr<virtual_queue> > qs_t;
    qs_t in_qs;
    qs_t in_qs_port;
    qs_t in_qs_bridge;
    for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end();
         ++ii) {
        const ingress::queues_t &iqs = (*ii)->get_queues();
        for (ingress::queues_t::const_iterator qi = iqs.begin();
             qi != iqs.end(); ++qi) {
            if (!qi->second->front_is_empty()
                && qi->second->front_is_head_flit()
                && qi->second->front_node_id().is_valid()
                && qi->second->front_new_flow_id().is_valid()
                && !qi->second->front_vq_id().is_valid()) {
                in_qs.push_back(qi->second);
            }
        }
    }
    int num_reqs = in_qs.size();
    int stage1_act_num_port = 0;
    int stage1_act_num_bridge = 0;
    int num_grants = 0;
    int stage2_act_num_port = 0;
    int stage2_act_num_bridge = 0;

    va_req_t va_req,va_req_port,va_req_bridge;
    va_req.clear();
    va_req_port.clear();
    va_req_bridge.clear();
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);
    random_shuffle(in_qs.begin(), in_qs.end(), rr_fn);
    for (qs_t::iterator qi = in_qs.begin(); qi != in_qs.end(); ++qi) {
            std::shared_ptr<virtual_queue> &iq = *qi;
        assert(!iq->front_is_empty());
        route_query_t rq(iq->get_src_node_id(), iq->front_old_flow_id(),
                         iq->front_node_id(), iq->front_new_flow_id());
        assert(routes.find(rq) != routes.end());
        const route_queues_t &route_qs = routes[rq];
        route_queues_t free_qs;
        route_queues_t free_qs_port;
        route_queues_t free_qs_bridge;
        double prop_sum = 0.0;
        int active_bridge = 0;
        for (route_queues_t::const_iterator rqi = route_qs.begin();
             rqi != route_qs.end(); ++rqi) {
            std::shared_ptr<virtual_queue> rq = get<0>(*rqi);
            double prop = get<1>(*rqi);
            if (rq->back_is_empty()) { // OQPF/OFPQ do not apply
                if (!is_claimed(rq->get_id()) && !rq->back_is_mid_packet()) {
                    prop_sum += prop;
                    free_qs.push_back(make_tuple(rq, prop_sum));
                    if(rq->get_ingress_id().get_name() == "X") {
                      free_qs_bridge.push_back(make_tuple(rq,prop_sum)); 
                      active_bridge = 1;
                    } else
                      free_qs_port.push_back(make_tuple(rq,prop_sum));
                }
            } else if (one_queue_per_flow
                       && rq->back_has_old_flow(iq->front_new_flow_id())) {
                // VC has current flow; permit only this VC
                prop_sum = 1.0;
                free_qs.clear();
                if (!is_claimed(rq->get_id()) && !rq->back_is_mid_packet()) {
                    prop_sum += prop;
                    free_qs.push_back(make_tuple(rq, prop_sum));
                    if(rq->get_ingress_id().get_name() == "X") {
                      free_qs_bridge.push_back(make_tuple(rq,prop_sum)); 
                      active_bridge = 1;
                    } else
                      free_qs_port.push_back(make_tuple(rq,prop_sum));
                }
                break;
            } else if (one_flow_per_queue
                       && !rq->back_has_old_flow(iq->front_new_flow_id())) {
                // VC has another flow so cannot have ours
                continue;
            } else if (!is_claimed(rq->get_id()) && !rq->back_is_mid_packet()) {
                prop_sum += prop;
                free_qs.push_back(make_tuple(rq, prop_sum));
                if(rq->get_ingress_id().get_name() == "X") {
                   free_qs_bridge.push_back(make_tuple(rq,prop_sum)); 
                   active_bridge = 1;
                } else
                   free_qs_port.push_back(make_tuple(rq,prop_sum));
            }
            if (active_bridge == 1) stage1_act_num_bridge += 1;
        }

        stage1_act_num_port = num_reqs - stage1_act_num_bridge;

        if (!free_qs.empty()) {
            double r = ran->random_range_double(prop_sum);
            for (route_queues_t::const_iterator oqi = free_qs.begin();
                 oqi != free_qs.end(); ++oqi) {
                std::shared_ptr<virtual_queue> oq = get<0>(*oqi);
                double prop = get<1>(*oqi);

                if (r < prop) {
                    iq->front_set_vq_id(get<1>(oq->get_id()));
                    ++num_grants;
                    if(oq->get_ingress_id().get_name() == "X")
                       ++stage2_act_num_bridge;
                    else
                       ++stage2_act_num_port;
                    break;
                }
            }
            va_req.push_back(free_qs.size());
            va_req_port.push_back(free_qs_port.size());
            va_req_bridge.push_back(free_qs_bridge.size());
        }
    }

    uint32_t total_stage1_req_num_port = 0, total_stage2_req_num_port = va_req_port.size();
    while(!va_req_port.empty())
        { 
           total_stage1_req_num_port += va_req_port.back();
           va_req_port.pop_back();
        }

    double stage1_avg_req_port, stage2_avg_req_port;
    if(stage1_act_num_port != 0) { 
       stage1_avg_req_port = static_cast<double> (total_stage1_req_num_port) / 
                             static_cast<double> (stage1_act_num_port);
    } else stage1_avg_req_port = 0;

    if(stage2_act_num_port != 0) {
       stage2_avg_req_port = static_cast<double> (total_stage2_req_num_port) / 
                             static_cast<double> (stage2_act_num_port);
    } else stage2_avg_req_port = 0;

    uint32_t total_stage1_req_num_bridge = 0, total_stage2_req_num_bridge = va_req_bridge.size();
    while(!va_req_bridge.empty())
        { 
           total_stage1_req_num_bridge += va_req_bridge.back();
           va_req_bridge.pop_back();
        }

    double stage1_avg_req_bridge, stage2_avg_req_bridge;
    if(stage1_act_num_bridge != 0) { 
       stage1_avg_req_bridge = static_cast<double> (total_stage1_req_num_bridge) / 
                             static_cast<double> (stage1_act_num_bridge);
    } else stage1_avg_req_bridge = 0;

    if(stage2_act_num_bridge != 0) {
       stage2_avg_req_bridge = static_cast<double> (total_stage2_req_num_bridge) / 
                             static_cast<double> (stage2_act_num_bridge);
    } else stage2_avg_req_bridge = 0;

    stats->va_alloc(id, stage1_act_num_port, stage1_avg_req_port, 
                        stage2_act_num_port, stage2_avg_req_port,
                        stage1_act_num_bridge, stage1_avg_req_bridge,
                        stage2_act_num_bridge, stage2_avg_req_bridge);

    if (num_reqs > 0) {
        int grant_perc = static_cast<int>(100 * static_cast<double>(num_grants)
                                          / static_cast<double>(num_reqs));
        LOG(log,3) << "[vc alloc " << get_id() << "] granted " << dec
            << num_grants << " of " << num_reqs
            << " request" << (num_reqs == 1 ? "" : "s") << " from "
            << num_free_eqs << " free neighbor queue"
            << (num_free_eqs == 1 ? "" : "s") << " ("
            << grant_perc << "%)" << endl;
    }
}
