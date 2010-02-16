// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <algorithm>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "random.hpp"
#include "crossbar.hpp"

crossbar::crossbar(node_id parent, shared_ptr<statistics> s, logger &l, 
                   shared_ptr<BoostRand> r) throw()
    : id(parent), ingresses(), egresses(), stats(s), log(l), ran(r) { }

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
    vector<tuple<node_id, iqi_t, iqi_t> > iq_iters;
    vector<tuple<iqi_t, iqi_t> > eq_iters;
    for (ingresses_t::iterator ii = ingresses.begin(); ii != ingresses.end();
         ++ii) {
        const iq_t &qs = ii->second->get_queues();
        iq_iters.push_back(make_tuple(ii->first, qs.begin(), qs.end()));
    }
    for (egresses_t::iterator ei = egresses.begin(); ei != egresses.end();
         ++ei) {
        const iq_t &qs = ei->second->get_remote_queues();
        eq_iters.push_back(make_tuple(qs.begin(), qs.end()));
    }
    bool more_to_do = true;
    while (more_to_do) {
        more_to_do = false;
        for (vector<tuple<node_id, iqi_t,iqi_t> >::iterator iqii =
                 iq_iters.begin();
             iqii != iq_iters.end(); ++iqii) {
            if (iqii->get<1>() != iqii->get<2>()) {
                more_to_do = true;
                ingress_qs.push_back(make_tuple(iqii->get<0>(),
                                                iqii->get<1>()->second));
                (iqii->get<1>())++;
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
    map<node_id, unsigned> ibws; // remaining bandwidths for each ingress port
    map<node_id, unsigned> ebws; // remaining bandwidths for each egress
    map<node_id, unsigned> egress_demands;
    for (ingresses_t::iterator i = ingresses.begin();
         i != ingresses.end(); ++i) {
        ibws[i->first] = i->second->get_bw_to_xbar();
    }
    for (egresses_t::iterator i = egresses.begin(); i != egresses.end(); ++i) {
        LOG(log,12) << "[xbar " << id << "]     egress to "
            << i->first << " has bandwidth "
            << dec << i->second->get_bandwidth() << endl;
        ebws[i->first] = i->second->get_bandwidth();
        egress_demands[i->first] = 0;
    }
    boost::function<int(int)> rr_fn = bind(&BoostRand::random_range, ran, _1);
    random_shuffle(egress_qs.begin(), egress_qs.end(), rr_fn);
    random_shuffle(ingress_qs.begin(), ingress_qs.end(), rr_fn);
    vqs_t ingress_ready_qs;
    for (nvqs_t::iterator iqi = ingress_qs.begin();
         iqi != ingress_qs.end(); ++iqi) {
        node_id &in_node = iqi->get<0>();
        shared_ptr<virtual_queue> &iq = iqi->get<1>();
        if (ibws[in_node] > 0 && iq->egress_ready()) {
            egress_demands[iq->front_node_id()]++;
            ingress_ready_qs.push_back(iq);
            --ibws[in_node];
        }
    }
    int num_reqs = ingress_ready_qs.size();
    int num_sent = 0;
    int num_eqs = 0;
    for (vqs_t::iterator eqi = egress_qs.begin();
         eqi != egress_qs.end(); ++eqi) {
        node_id out_node; virtual_queue_id out_q;
        shared_ptr<virtual_queue> &eq = *eqi;
        tie(out_node,out_q) = eq->get_id();
        LOG(log,12) << "[xbar " << id << "]     egress queue "
            << eq->get_id() << endl;
        if (ebws[out_node] > 0 && !eq->full()) {
            ++num_eqs;
            for (vqs_t::iterator iqi = ingress_ready_qs.begin();
                 iqi != ingress_ready_qs.end(); ++iqi) {
                shared_ptr<virtual_queue> &iq = *iqi;
                bool iq_ready = iq->egress_ready();
                LOG(log,12) << "[xbar " << id
                            << "]         considering ingress queue "
                            << iq->get_id() << ": ";
                if (iq->empty()) {
                    LOG(log, 12) << "empty" << endl;
                } else {
                    if (iq_ready) {
                        virtual_queue_node_id vqn =
                            make_tuple(iq->front_node_id(), iq->front_vq_id());
                        LOG(log, 12) << "-> " << vqn << " (ready)";
                    } else {
                        LOG(log, 12) << " (not ready)";
                    }
                    LOG(log, 12) << endl;
                }
                if (iq_ready && iq->front_node_id() == out_node
                    && iq->front_vq_id() == out_q) {
                    LOG(log,12) << "[xbar " << id
                        << "]         queue " << iq->get_id()
                        << " wins arbitration" << endl;
                    flit f = iq->front();
                    iq->pop();
                    eq->push(f);
                    --ebws[out_node];
                    ++num_sent;
                    break; // exit ingress_ready_qs loop
                }
            }
        }
    }
    unsigned total_bw = 0;
    for (egresses_t::iterator i = egresses.begin(); i != egresses.end(); ++i) {
        unsigned total = i->second->get_bandwidth();
        total_bw += total;
        unsigned left = ebws[i->first];
        unsigned demand = egress_demands[i->second->get_target_id()];
        unsigned used = total - left;
        double req_frac =
            static_cast<double>(used) / static_cast<double>(demand);
        double bw_frac =
            static_cast<double>(used) / static_cast<double>(total);
        stats->cxn_xmit(id, i->second->get_target_id(), used,
                        req_frac, bw_frac);
    }
    double req_frac =
        static_cast<double>(num_sent) / static_cast<double>(num_reqs);
    double bw_frac =
        static_cast<double>(num_sent) / static_cast<double>(total_bw);
    stats->xbar(id, num_sent, num_reqs, req_frac, bw_frac);
    if (num_reqs > 0) {
        LOG(log,3) << "[xbar " << id << "] sent " << dec
            << num_sent << " of " << num_reqs
            << " flit" << (num_reqs == 1 ? "" : "s")
            << " (" << static_cast<int>(100 * req_frac) << "%) via "
            << num_eqs << " free neighbor queue" << (num_eqs == 1 ? "" : "s")
            << " (" << static_cast<int>(100 * bw_frac) << "% egress bw)"
            << endl;
    }
}

void crossbar::tick_negative_edge() throw(err) { }

