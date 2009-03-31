// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include <algorithm>
#include "set_channel_alloc.hpp"
#include "random.hpp"

set_channel_alloc::set_channel_alloc(node_id n, logger &l) throw()
    : channel_alloc(n, l), egresses() { }

set_channel_alloc::~set_channel_alloc() throw() { }

void set_channel_alloc::add_egress(shared_ptr<egress> egr) throw(err) {
    assert(egresses.find(egr->get_target_id()) == egresses.end());
    egresses[egr->get_target_id()] = egr;
}

void set_channel_alloc::add_route(const node_id &src, const flow_id &f,
                                  const node_id &dst,
                                  const vector<virtual_queue_id> &qids)
    throw(err) {
    if (egresses.find(dst) == egresses.end())
        throw err_bad_next_hop(get_id().get_numeric_id(), f.get_numeric_id(),
                               dst.get_numeric_id());
    const ingress::queues_t &eqs = egresses[dst]->get_remote_queues();
    route_query_t rq = route_query_t(src,f,dst);
    assert(routes.find(rq) == routes.end());
    if (!qids.empty()) { // add only specified queues
        for (vector<virtual_queue_id>::const_iterator idi = qids.begin();
             idi != qids.end(); ++idi) {
            ingress::queues_t::const_iterator eqi = eqs.find(*idi);
            if (eqi == eqs.end())
                throw err_bad_next_hop_queue(get_id().get_numeric_id(),
                                             f.get_numeric_id(),
                                             dst.get_numeric_id(),
                                             idi->get_numeric_id());
            routes[rq].push_back(eqi->second);
        }
    } else { // add all egress queues
        for (ingress::queues_t::const_iterator eqi = eqs.begin();
             eqi != eqs.end(); ++eqi) {
            routes[rq].push_back(eqi->second);
        }
    }
}

void set_channel_alloc::allocate() throw(err) {
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
    random_shuffle(in_qs.begin(), in_qs.end(), random_range);
    for (qs_t::iterator qi = in_qs.begin(); qi != in_qs.end(); ++qi) {
        shared_ptr<virtual_queue> &iq = *qi;
        assert(!iq->empty());
        route_query_t rq(iq->get_src_node_id(), iq->get_egress_flow_id(),
                         iq->front_node_id());
        assert(routes.find(rq) != routes.end());
        const qs_t &route_qs = routes[rq];
        qs_t free_qs;
        for (qs_t::const_iterator rqi = route_qs.begin(); rqi != route_qs.end();
             ++rqi) {
            if (!is_claimed((*rqi)->get_id()) && (*rqi)->ingress_new_flow()) {
                free_qs.push_back(*rqi);
            }            
        }
        if (!free_qs.empty()) {
            int n = free_qs.size() == 1 ? 0 : random_range(free_qs.size());
            shared_ptr<virtual_queue> oq = free_qs[n];
            iq->set_front_vq_id(oq->get_id().get<1>());
        }
    }
}
