// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <algorithm>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "random.hpp"
#include "crossbar.hpp"

crossbar::crossbar(node_id parent, std::shared_ptr<tile_statistics> s,
                   std::shared_ptr<vcd_writer> v, logger &l,
                   std::shared_ptr<random_gen> r)
    : id(parent), ingresses(), egresses(), stats(s), vcd(v), log(l), ran(r) {
    if (vcd) {
        vector<string> path;
        ostringstream oss;
        oss << id;
        path.push_back("xbars");
        path.push_back("demand");
        path.push_back(oss.str());
        vcd->new_signal(&vcd_hooks.v_xbar_demand, path, 32);
        path[1] = "use";
        vcd->new_signal(&vcd_hooks.v_xbar_use, path, 32);
    }
}

void crossbar::add_ingress(node_id src, std::shared_ptr<ingress> ing) {
    if (ingresses.find(src) != ingresses.end()) {
        throw err_duplicate_ingress(get_id().get_numeric_id(),
                                    src.get_numeric_id());
    }
    ingresses[src] = ing;
    rebuild_queues();
}

void crossbar::add_egress(node_id dst, std::shared_ptr<egress> egr) {
    if (egresses.find(dst) != egresses.end()) {
        throw err_duplicate_egress(get_id().get_numeric_id(),
                                   dst.get_numeric_id());
    }
    egresses[dst] = egr;
    rebuild_queues();
}

void crossbar::rebuild_queues() {
    typedef ingress::queues_t iq_t;
    typedef iq_t::const_iterator iqi_t;
    ingress_qs.clear();
    egress_qs.clear();
    vector<std::tuple<node_id, iqi_t, iqi_t> > iq_iters;
    vector<std::tuple<iqi_t, iqi_t> > eq_iters;
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
        for (vector<std::tuple<node_id, iqi_t,iqi_t> >::iterator iqii =
                 iq_iters.begin();
             iqii != iq_iters.end(); ++iqii) {
            if (get<1>(*iqii) != get<2>(*iqii)) {
                more_to_do = true;
                ingress_qs.push_back(make_tuple(get<0>(*iqii),
                                                get<1>(*iqii)->second));
                (get<1>(*iqii))++;
            }
        }
        for (vector<std::tuple<iqi_t,iqi_t> >::iterator eqii = eq_iters.begin();
             eqii != eq_iters.end(); ++eqii) {
            if (get<0>(*eqii) != get<1>(*eqii)) {
                more_to_do = true;
                egress_qs.push_back(get<0>(*eqii)->second);
                (get<0>(*eqii))++;
            }
        }
    }
}

void crossbar::tick_positive_edge() {
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
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);
    random_shuffle(egress_qs.begin(), egress_qs.end(), rr_fn);
    random_shuffle(ingress_qs.begin(), ingress_qs.end(), rr_fn);
    vqs_t ingress_ready_qs; // ingress VCs that are ready to transmit
    int stage1_act_num = 0; 
    int stage1_act_num_port = 0; 
    int stage1_act_num_bridge = 0; 
    int total_stage1_req_num = 0; 
    int total_stage1_req_num_port = 0; 
    int total_stage1_req_num_bridge = 0; 
    map<ingress_id, uint64_t> sw_act_ingress;
    sw_act_ingress.clear();
    for (nvqs_t::iterator iqi = ingress_qs.begin();
         iqi != ingress_qs.end(); ++iqi) {
         node_id &in_node = get<0>(*iqi);
         std::shared_ptr<virtual_queue> &iq = get<1>(*iqi);
         if (!iq->front_is_empty()
            && iq->front_node_id().is_valid()
            && iq->front_vq_id().is_valid()) {
            ingress_id ig = iq->get_ingress_id();
            total_stage1_req_num ++;
            if(iq->get_ingress_id().get_name() == "B")
               total_stage1_req_num_bridge ++;
            else total_stage1_req_num_port ++;
            if(sw_act_ingress.find(ig) == sw_act_ingress.end()) {
               sw_act_ingress[ig] = 0;
               stage1_act_num ++;
               if(iq->get_ingress_id().get_name() == "B")
                  stage1_act_num_bridge ++;
               else
                  stage1_act_num_port ++;
            }
            if(ibws[in_node] > 0) {
            --ibws[in_node];
            egress_demands[iq->front_node_id()]++;
            ingress_ready_qs.push_back(iq);
            }
        }
    }
    int num_reqs = ingress_ready_qs.size();
    int num_sent = 0;
    int num_eqs = 0;
    int stage2_act_num = 0; 
    int stage2_act_num_port = 0; 
    int stage2_act_num_bridge = 0; 
    int total_stage2_req_num = 0; 
    int total_stage2_req_num_port = 0; 
    int total_stage2_req_num_bridge = 0; 
    map<ingress_id, uint64_t> sw_req_egress;
    sw_req_egress.clear();
  //  for (map<ingress_id, uint64_t>::iterator eqi = sw_req_egress.begin();
  //       eqi != sw_req_egress.end(); ++eqi) {
  //       if(sw_req_egress.second().get_ingress_id().get_name()== "X")
            
    for (vqs_t::iterator eqi = egress_qs.begin();
         eqi != egress_qs.end(); ++eqi) {
        std::shared_ptr<virtual_queue> &eq = *eqi;
        node_id out_node = get<0>(eq->get_id());
        virtual_queue_id out_q = get<1>(eq->get_id());
        LOG(log,12) << "[xbar " << id << "]     egress queue "
            << eq->get_id() << endl;
        if (eq->back_is_powered_on() && !eq->back_is_full()) {
            ++num_eqs;
            for (vqs_t::iterator iqi = ingress_ready_qs.begin();
                 iqi != ingress_ready_qs.end(); ++iqi) {
                std::shared_ptr<virtual_queue> &iq = *iqi;
                bool iq_ready = (!iq->front_is_empty()
                                 && iq->front_node_id().is_valid()
                                 && iq->front_vq_id().is_valid());
                if(ebws[out_node] > 0) {
                   LOG(log,12) << "[xbar " << id
                   << "]         considering ingress queue "
                   << iq->get_id() << ": ";
                   if (iq->front_is_empty()) {
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
                }
                if (iq_ready && iq->front_node_id() == out_node
                    && iq->front_vq_id() == out_q) {
                    ingress_id eg = eq->get_ingress_id();
                    total_stage2_req_num ++;                   
                    if(eg.get_name() == "X") total_stage2_req_num_bridge ++;
                    else total_stage2_req_num_port ++;
                    if(sw_req_egress.find(eg) == sw_req_egress.end()) {
                       sw_req_egress[eg] = 0;
                       stage2_act_num ++;
                       if (eg.get_name() == "X") stage2_act_num_bridge ++;
                       else stage2_act_num_port ++;
                    }
                    if(ebws[out_node] > 0) {
                       LOG(log,12) << "[xbar " << id
                           << "]         queue " << iq->get_id()
                           << " wins arbitration" << endl;
                       flit f = iq->front_flit();
                       iq->front_pop();
                       eq->back_push(f);
                       stats->vq_rd(iq->get_id(),iq->get_ingress_id());
                       stats->vq_wr(eq->get_id(),eq->get_ingress_id());
                       ++num_sent;
                       --ebws[out_node];
                    }
                 break; // exit ingress_ready_qs loop
              }
           }
       }
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

    double stage1_avg_req_bridge, stage2_avg_req_bridge;
    if(stage1_act_num_bridge != 0) {
          stage1_avg_req_bridge = static_cast<double> (total_stage1_req_num_bridge) /
                                static_cast<double> (stage1_act_num_bridge);
    } else stage1_avg_req_bridge = 0;
    if(stage2_act_num_bridge != 0) {
          stage2_avg_req_bridge = static_cast<double> (total_stage2_req_num_bridge) /
                                static_cast<double> (stage2_act_num_bridge);
    } else stage2_avg_req_bridge = 0;

    stats->sw_alloc(id, stage1_act_num_port, stage1_avg_req_port, 
                        stage2_act_num_port, stage2_avg_req_port,
                        stage1_act_num_bridge, stage1_avg_req_bridge, 
                        stage2_act_num_bridge, stage2_avg_req_bridge);

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
    if (vcd) {
        vcd->add_value(&vcd_hooks.v_xbar_use, num_sent);
        vcd->add_value(&vcd_hooks.v_xbar_demand, num_reqs);
    }
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

void crossbar::tick_negative_edge() { }

