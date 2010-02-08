// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <algorithm>
#include <iomanip>
#include "set_channel_alloc.hpp"
#include "random.hpp"

set_channel_alloc::set_channel_alloc(node_id n, bool one_q_per_f,
                                     bool one_f_per_q, logger &l,
                                     shared_ptr<BoostRand> r) throw()
    : channel_alloc(n, one_q_per_f, one_f_per_q, l), egresses(), ran(r) {
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

set_channel_alloc::~set_channel_alloc() throw() { }

void set_channel_alloc::add_egress(shared_ptr<egress> egr) throw(err) {
    assert(egresses.find(egr->get_target_id()) == egresses.end());
    egresses[egr->get_target_id()] = egr;
}

void
set_channel_alloc::add_route(const node_id &src, const flow_id &f,
                             const node_id &dst, const flow_id &nf,
                             const vector<tuple<virtual_queue_id,double> > &qis)
    throw(err) {
    if (egresses.find(dst) == egresses.end())
        throw err_bad_next_hop(get_id().get_numeric_id(), f.get_numeric_id(),
                               dst.get_numeric_id());
    const ingress::queues_t &eqs = egresses[dst]->get_remote_queues();
    route_query_t rq = route_query_t(src,f,dst,nf);
    assert(routes.find(rq) == routes.end());
    if (!qis.empty()) { // add only specified queues with given propensities
        for (vector<tuple<virtual_queue_id,double> >::const_iterator idi =
                 qis.begin(); idi != qis.end(); ++idi) {
            virtual_queue_id vqid; double prop; tie(vqid,prop) = *idi;
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

void set_channel_alloc::allocate() throw(err) {
    int num_free_eqs = 0;
    for (egresses_t::const_iterator ei = egresses.begin();
         ei != egresses.end(); ++ei) {
        const ingress::queues_t &eqs = ei->second->get_remote_queues();
        for (ingress::queues_t::const_iterator eqi = eqs.begin();
             eqi != eqs.end(); ++eqi) {
            const shared_ptr<virtual_queue> &eq = eqi->second;
            if (!is_claimed(eq->get_id()) && eq->ingress_new_flow()) {
                ++num_free_eqs;
            }
        }
    }
    typedef vector<shared_ptr<virtual_queue> > qs_t;
    qs_t in_qs;
    for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end();
         ++ii) {
        const ingress::queues_t &iqs = (*ii)->get_queues();
        for (ingress::queues_t::const_iterator qi = iqs.begin();
             qi != iqs.end(); ++qi) {
            if (qi->second->egress_new_flow()
                && !qi->second->front_vq_id().is_valid()) {
                in_qs.push_back(qi->second);
            }
        }
    }
    int num_reqs = in_qs.size();
    int num_grants = 0;
    random_shuffle(in_qs.begin(), in_qs.end(), random_range);
    for (qs_t::iterator qi = in_qs.begin(); qi != in_qs.end(); ++qi) {
        shared_ptr<virtual_queue> &iq = *qi;
        assert(!iq->empty());
        route_query_t rq(iq->get_src_node_id(), iq->get_egress_old_flow_id(),
                         iq->front_node_id(), iq->get_egress_new_flow_id());
        assert(routes.find(rq) != routes.end());
        const route_queues_t &route_qs = routes[rq];
        route_queues_t free_qs;
        double prop_sum = 0.0;
        for (route_queues_t::const_iterator rqi = route_qs.begin();
             rqi != route_qs.end(); ++rqi) {
            shared_ptr<virtual_queue> rq; double prop; tie(rq,prop) = *rqi;
            if (rq->empty()) {
                if (!is_claimed(rq->get_id()) && rq->ingress_new_flow()) {
                    prop_sum += prop;
                    free_qs.push_back(make_tuple(rq, prop_sum));
                }
            } else if (one_queue_per_flow
                       && rq->has_old_flow(iq->get_egress_new_flow_id())) {
                // VC has current flow; permit only this VC
                prop_sum = 1.0;
                free_qs.clear();
                if (!is_claimed(rq->get_id()) && rq->ingress_new_flow()) {
                    prop_sum += prop;
                    free_qs.push_back(make_tuple(rq, prop_sum));
                }
                break;
            } else if (one_flow_per_queue
                       && !rq->has_old_flow(iq->get_egress_new_flow_id())) {
                // VC has another flow so cannot have ours
                continue;
            } else if (!is_claimed(rq->get_id()) && rq->ingress_new_flow()) {
                prop_sum += prop;
                free_qs.push_back(make_tuple(rq, prop_sum));
            }
        }
        if (!free_qs.empty()) {
            //double r = random_range_double(prop_sum);
            double r = ran->random_range_double(prop_sum);
            for (route_queues_t::const_iterator oqi = free_qs.begin();
                 oqi != free_qs.end(); ++oqi) {
                shared_ptr<virtual_queue> oq; double prop; tie(oq,prop) = *oqi;
                if (r < prop) {
                    iq->set_front_vq_id(oq->get_id().get<1>());
                    ++num_grants;
                    break;
                }
            }
        }
    }
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
