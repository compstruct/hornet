// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cassert>
#include "dynamic_channel_alloc.hpp"
#include "random.hpp"

dynamic_channel_alloc::dynamic_channel_alloc(node_id n, logger &l) throw()
    : channel_alloc(n, l), egresses() { }

dynamic_channel_alloc::~dynamic_channel_alloc() throw() { }

void dynamic_channel_alloc::add_egress(shared_ptr<egress> egr) throw(err) {
    egresses.push_back(egr);
}

void dynamic_channel_alloc::allocate() throw(err) {
    typedef vector<shared_ptr<virtual_queue> > qs_t;
    typedef map<node_id, qs_t> demand_qs_t;
    demand_qs_t demands;
    for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end();
         ++ii) {
        const ingress::queues_t &iqs = (*ii)->get_queues();
        for (ingress::queues_t::const_iterator qi = iqs.begin();
             qi != iqs.end(); ++qi) {
            if (qi->second->egress_new_flow()
                && !qi->second->front_vq_id().is_valid()) {
                demands[qi->second->front_node_id()].push_back(qi->second);
            }
        }
    }
    for (egresses_t::iterator ei = egresses.begin(); ei != egresses.end();
         ++ei) {
        qs_t free_eqs;
        const ingress::queues_t &all_egress_qs = (*ei)->get_remote_queues();
        for (ingress::queues_t::const_iterator qi = all_egress_qs.begin();
             qi != all_egress_qs.end(); ++qi) {
            if (!is_claimed(qi->second->get_id())
                && qi->second->ingress_new_flow()) {
                free_eqs.push_back(qi->second);
            }
        }
        qs_t &demand_iqs = demands[(*ei)->get_target_id()];
        random_shuffle(free_eqs.begin(), free_eqs.end(), random_range);
        random_shuffle(demand_iqs.begin(), demand_iqs.end(), random_range);
        qs_t::iterator iqi, eqi;
        for (iqi = demand_iqs.begin(), eqi = free_eqs.begin();
             iqi != demand_iqs.end() && eqi != free_eqs.end();
             ++iqi, ++eqi) {
            (*iqi)->set_front_vq_id((*eqi)->get_id().get<1>());
        }
    }
}
