// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <algorithm>
#include "random.hpp"
#include "ginj.hpp"

ginj::ginj(const pe_id &id, const uint64_t &t,
                   shared_ptr<id_factory<packet_id> > pif,
                   shared_ptr<statistics> st, logger &l, shared_ptr<BoostRand> r) throw(err)
    : pe(id), system_time(t), net(), events(), next_event(events.begin()),
      waiting_packets(), waiting_packets_queue(), incoming_packets(), flows(), flow_ids(), flow_ids_queue(), queue_ids(),
      packet_id_factory(pif), stats(st), log(l), ran(r) { 
   pkt_accounting = 0;
   stop_darsim = false;
}

ginj::~ginj() throw() { 
}

void ginj::connect(shared_ptr<bridge> net_bridge) throw(err) {
    net = net_bridge;
    shared_ptr<vector<uint32_t> > qs = net->get_ingress_queue_ids();
    queue_ids.clear();
    copy(qs->begin(), qs->end(), back_insert_iterator<vector<uint32_t> >(queue_ids));
}

void ginj::add_packet(uint64_t time, const flow_id &f, unsigned l) throw(err) {
    if (!stop_darsim) {
       g_waiting_packet pkt = { packet_id_factory->get_fresh_id(), f, l, time, 0 };
       waiting_packets_queue.push(pkt);
       pkt_accounting++;
       flow_ids.push_back(f);
       
       LOG(log,2) << "[ginj " << get_id() << "] add_packet flow_id " << f << endl;
    }
}

void ginj::set_stop_darsim() throw(err) {
   LOG(log,0) << "Stop [ginj " << get_id() << "]" << endl;
   stop_darsim = true;
}

bool ginj::work_queued() throw(err) {
    if (stop_darsim) {
       return 0;
    }
    queue<g_waiting_packet> &q = waiting_packets_queue;
    if (!q.empty()) {
       return 1;
    }
    else {
       return 0;
    }
}

void ginj::tick_positive_edge() throw(err) {
    for (uint32_t i = 0; i < 32; ++i) {
       if (incoming_packets.find(i) != incoming_packets.end()) {
          g_incoming_packet &ip = incoming_packets[i];
          if (net->get_transmission_done(ip.xmit)) {
             stats->complete_packet(ip.flow, ip.len, ip.id);
             incoming_packets.erase(i);
          }
       }
    }

    uint32_t waiting = net->get_waiting_queues();
    if (waiting != 0) {
       //random_shuffle(queue_ids.begin(), queue_ids.end(), random_range);
        for (vector<uint32_t>::iterator i = queue_ids.begin(); i != queue_ids.end(); ++i) {
           if (((waiting >> *i) & 1) && (incoming_packets.find(*i) == incoming_packets.end())) {
              g_incoming_packet &ip = incoming_packets[*i];
              ip.flow = net->get_queue_flow_id(*i);
              ip.len = net->get_queue_length(*i);
              ip.xmit = net->receive(NULL, *i, net->get_queue_length(*i),  &ip.id);
           }
        }
    }

    if (!stop_darsim) {
       unsigned num_qs = net->get_egress()->get_remote_queues().size();
       for (unsigned i = 0; i < num_qs; ++i) { // allow one flow multi-queue bursts
          queue<g_waiting_packet> &q = waiting_packets_queue;
          if (!q.empty()) {
             g_waiting_packet &pkt = q.front();
             if ( (pkt.time <= system_time) && (!pkt.offered) ) {
                bool td = (pkt.time <= system_time) && (!pkt.offered);
                LOG(log,2) << "[ginj " << get_id() << "] pkt time " << dec << pkt.time << " DAR time " << dec << system_time << " TD " << td << endl;
                pkt.offered = 1;
                stats->offer_packet(pkt.flow, pkt.len, pkt.id);
             }
             if (pkt.offered) {
                vector<flow_id>::iterator fi;
                fi = find(flow_ids.begin(), flow_ids.end(), pkt.flow);
                if (net->send(fi->get_numeric_id(), NULL, pkt.len, pkt.id)) {
                   q.pop();
                   flow_ids.erase(fi);
                   pkt_accounting--;
                   LOG(log,2) << "[ginj " << get_id() << "] sent flow_id " << fi->get_numeric_id() << " " << pkt_accounting << endl;
                }
             }
          }
       }
    }
}

void ginj::tick_negative_edge() throw(err) { }

bool ginj::is_ready_to_offer() throw(err) {
   if (stop_darsim) {
      return false;
   }
   queue<g_waiting_packet> &q = waiting_packets_queue;
   if (!q.empty()) {
      g_waiting_packet &pkt = q.front();
      if ( (pkt.time <= system_time) && (!pkt.offered) ) {
         return true;
      }
      if (pkt.offered) {
         return true;
      }
      return false;
   }
   return false;
}

bool ginj::is_drained() const throw() {
   bool drained = true;
   drained &= net->get_waiting_queues() == 0;
   for (uint32_t i = 0; i < 32; ++i) {
      drained &= incoming_packets.find(i) == incoming_packets.end();
   }
   return drained;
}
