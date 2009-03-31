// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <algorithm>
#include "random.hpp"
#include "crossbar.hpp"

crossbar::crossbar(node_id parent, logger &l) throw()
    : id(parent), ingresses(), egresses(), log(l) { }

void crossbar::add_ingress(node_id src, shared_ptr<ingress> ing) throw(err) {
    if (ingresses.find(src) != ingresses.end()) {
        throw err_duplicate_ingress(get_id().get_numeric_id(),
                                    src.get_numeric_id());
    }
    ingresses[src] = ing;
    rebuild_queues();
}

void crossbar::add_egress(node_id dst, shared_ptr<egress> egr) throw(err) {
    if (egresses.find(dst) != egresses.end()) {
        throw err_duplicate_egress(get_id().get_numeric_id(),
                                   dst.get_numeric_id());
    }
    egresses[dst] = egr;
    rebuild_queues();
}

void crossbar::rebuild_queues() throw() {
    typedef ingress::queues_t iq_t;
    typedef iq_t::const_iterator iqi_t;
    ingress_qs.clear();
    egress_qs.clear();
    vector<tuple<iqi_t, iqi_t> > iq_iters;
    vector<tuple<iqi_t, iqi_t> > eq_iters;
    for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end();
         ++ii) {
        const iq_t &qs = ii->second->get_queues();
        iq_iters.push_back(make_tuple(qs.begin(), qs.end()));
    }
    for (egresses_t::iterator ei = egresses.begin(); ei != egresses.end();
         ++ei) {
        const iq_t &qs = ei->second->get_remote_queues();
        eq_iters.push_back(make_tuple(qs.begin(), qs.end()));
    }
    bool more_to_do = true;
    while (more_to_do) {
        more_to_do = false;
        for (vector<tuple<iqi_t,iqi_t> >::iterator iqii = iq_iters.begin();
             iqii != iq_iters.end(); ++iqii) {
            if (iqii->get<0>() != iqii->get<1>()) {
                more_to_do = true;
                ingress_qs.push_back(iqii->get<0>()->second);
                (iqii->get<0>())++;
            }
        }
        for (vector<tuple<iqi_t,iqi_t> >::iterator eqii = eq_iters.begin();
             eqii != eq_iters.end(); ++eqii) {
            if (eqii->get<0>() != eqii->get<1>()) {
                more_to_do = true;
                egress_qs.push_back(eqii->get<0>()->second);
                (eqii->get<0>())++;
            }
        }
    }
}

void crossbar::tick_positive_edge() throw(err) {
    LOG(log,12) << "[xbar " << id << "] arbitration" << endl;
    map<node_id, unsigned> bws; // remaining bandwidths for each egress
    for (egresses_t::iterator i = egresses.begin(); i != egresses.end(); ++i) {
        LOG(log,12) << "[xbar " << id << "]     egress to "
            << i->first << " has bandwidth "
            << dec << i->second->get_bandwidth() << endl;
        bws[i->first] = i->second->get_bandwidth();
    }
    random_shuffle(egress_qs.begin(), egress_qs.end(), random_range);
    random_shuffle(ingress_qs.begin(), ingress_qs.end(), random_range);
    for (vqids_t::iterator eqi = egress_qs.begin();
         eqi != egress_qs.end(); ++eqi) {
        node_id n; virtual_queue_id q;
        shared_ptr<virtual_queue> &eq = *eqi;
        tie(n,q) = eq->get_id();
        LOG(log,12) << "[xbar " << id << "]     egress queue "
            << eq->get_id() << endl;
        if (bws[n] > 0 && !eq->full()) {
            for (vqids_t::iterator iqi = ingress_qs.begin();
                 iqi != ingress_qs.end(); ++iqi) {
                shared_ptr<virtual_queue> &iq = *iqi;
                bool iq_ready = iq->egress_ready();
                LOG(log,12) << "[xbar " << id
                            << "]         considering ingress queue "
                            << iq->get_id() << ": ";
                if (iq->empty()) {
                    LOG(log, 12) << "empty" << endl;
                } else {
                    LOG(log, 12) << "-> node " << iq->front_node_id();
                    if (iq_ready) {
                        LOG(log, 12) << " (ready)" << endl;
                    } else {
                        LOG(log, 12) << " (not ready)" << endl;
                    }
                }
                if (iq->egress_ready() && iq->front_node_id() == n
                    && iq->front_vq_id() == q) {
                    LOG(log,12) << "[xbar " << id
                        << "]         queue " << iq->get_id()
                        << " wins arbitration" << endl;
                    flit f = iq->front();
                    iq->pop();
                    eq->push(f);
                    --bws[n];
                    break; // exit ingress_qs loop
                }
            }
        }
    }
}

void crossbar::tick_negative_edge() throw(err) { }
