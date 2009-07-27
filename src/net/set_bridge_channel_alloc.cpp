// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "random.hpp"
#include "set_bridge_channel_alloc.hpp"

set_bridge_channel_alloc::set_bridge_channel_alloc(node_id n, bool one_q_per_f,
                                                   bool one_f_per_q,
                                                   logger &l) throw()
    : bridge_channel_alloc(n, one_q_per_f, one_f_per_q, l), queues(),
      routes() { }

set_bridge_channel_alloc::~set_bridge_channel_alloc() throw() { }

void set_bridge_channel_alloc::add_queue(shared_ptr<virtual_queue> q)
    throw(err) {
    virtual_queue_id qid = q->get_id().get<1>();
    if (queues.find(qid) != queues.end())
        throw err_duplicate_queue(get_id().get_numeric_id(),
                                  qid.get_numeric_id());
    queues[qid] = q;
}

void
set_bridge_channel_alloc::
add_route(const flow_id &f, const vector<tuple<virtual_queue_id,double> > &qids)
    throw(err) {
    if (routes.find(f) != routes.end())
        throw err_duplicate_flow(get_id().get_numeric_id(), f.get_numeric_id());
    if (!qids.empty()) { // add only specified queues with given propensities
        for (vector<tuple<virtual_queue_id,double> >::const_iterator idi =
                 qids.begin(); idi != qids.end(); ++idi) {
            virtual_queue_id vqid; double prop; tie(vqid,prop) = *idi;
            assert(prop > 0);
            queues_t::iterator qi = queues.find(vqid);
            if (qi == queues.end())
                throw err_bad_next_hop_queue(get_id().get_numeric_id(),
                                             f.get_numeric_id(),
                                             get_id().get_numeric_id(),
                                             vqid.get_numeric_id());
            routes[f].push_back(make_tuple(qi->second, prop));
        }
    } else { // add all queues with equal propensity
        for(queues_t::iterator qi = queues.begin(); qi != queues.end(); ++qi) {
            routes[f].push_back(make_tuple(qi->second, 1.0));
        }
    }
}

virtual_queue_id set_bridge_channel_alloc::request(flow_id f) throw(err) {
    if (routes.find(f) == routes.end())
        throw exc_bad_flow(get_id().get_numeric_id(), f.get_numeric_id());
    const route_queues_t &qs = routes[f];
    route_queues_t free_qs;
    double prop_sum = 0.0;
    for (route_queues_t::const_iterator qi = qs.begin(); qi != qs.end(); ++qi) {
        shared_ptr<virtual_queue> q; double prop; tie(q,prop) = *qi;
        if (q->empty()) {
            if (!is_claimed(q->get_id()) && q->ingress_new_flow()) {
                prop_sum += prop;
                free_qs.push_back(make_tuple(q, prop_sum));
            }
        } else if (one_queue_per_flow && q->has_old_flow(f)) {
            // VC has current flow; permit only this VC
            prop_sum = 1.0;
            free_qs.clear();
            if (!is_claimed(q->get_id()) && q->ingress_new_flow()) {
                free_qs.push_back(make_tuple(q, prop_sum));
            }
            break;
        } else if (one_flow_per_queue && !q->has_old_flow(f)) {
            // VC has another flow so cannot have ours
            continue;
        } else if (!is_claimed(q->get_id()) && q->ingress_new_flow()) {
            prop_sum += prop;
            free_qs.push_back(make_tuple(q, prop_sum));
        }
    }
    if (!free_qs.empty()) {
        virtual_queue_id vqid;
        double r = random_range_double(prop_sum);
        for (route_queues_t::const_iterator oqi = free_qs.begin();
             oqi != free_qs.end(); ++oqi) {
            shared_ptr<virtual_queue> oq; double prop; tie(oq,prop) = *oqi;
            if (r < prop) {
                vqid = oq->get_id().get<1>();
                break;
            }
        }
        return vqid;
    } else {
        return virtual_queue_id(); // invalid ID
    }
}
