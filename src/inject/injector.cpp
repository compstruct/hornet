// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <algorithm>
#include "random.hpp"
#include "injector.hpp"

injector::injector(const pe_id &id, const uint64_t &t,
                   shared_ptr<id_factory<packet_id> > pif,
                   shared_ptr<statistics> st, logger &l,
                   shared_ptr<BoostRand> r, shared_ptr<vcd_writer> v) throw(err)
    : pe(id), system_time(t), net(), events(), next_event(events.begin()),
      waiting_packets(), incoming_packets(), flows(), flow_ids(), queue_ids(),
      packet_id_factory(pif), stats(st), log(l), ran(r), vcd(v) { }

injector::~injector() throw() { }

void injector::connect(shared_ptr<bridge> net_bridge) throw(err) {
    net = net_bridge;
    shared_ptr<vector<uint32_t> > qs = net->get_ingress_queue_ids();
    queue_ids.clear();
    copy(qs->begin(), qs->end(),
         back_insert_iterator<vector<uint32_t> >(queue_ids));
}

void injector::add_event(const uint64_t &t, const flow_id &f,
                         unsigned l, const uint64_t &p) throw(err) {
    assert(events.empty() || t >= events.back().get<0>());
    events.push_back(make_tuple(t, f, l, p));
    next_event = events.begin();
    if (find(flow_ids.begin(), flow_ids.end(), f) == flow_ids.end()) {
        flow_ids.push_back(f);
        flows[f] = make_tuple(UINT64_MAX, 0, 0);
    }
}

void injector::tick_positive_edge() throw(err) {
    for (; next_event != events.end() && next_event->get<0>() == system_time;
         ++next_event) {
        tick_t t; flow_id f; len_t l; period_t p;
        tie(t, f, l, p) = *next_event;
        flows[f] = make_tuple(system_time, l, p);
        LOG(log,2) << "[injector " << get_id() << "] flow " << f;
        if (l == 0) {
            LOG(log,2) << " off" << endl;
        } else {
            LOG(log,2) << " sending a " << dec << l << "-flit packet every "
                       << dec << p << " tick" << (p == 1 ? "" : "s") << endl;
        }
    }
    for (uint32_t i = 0; i < 32; ++i) {
        if (incoming_packets.find(i) != incoming_packets.end()) {
            incoming_packet &ip = incoming_packets[i];
            if (net->get_transmission_done(ip.xmit)) {
                stats->complete_packet(ip.flow, ip.len, ip.id);
                incoming_packets.erase(i);
            }
        }
    }
    uint32_t waiting = net->get_waiting_queues();
    if (waiting != 0) {
        random_shuffle(queue_ids.begin(), queue_ids.end(), random_range);
        for (vector<uint32_t>::iterator i = queue_ids.begin();
             i != queue_ids.end(); ++i) {
            if (((waiting >> *i) & 1)
                && (incoming_packets.find(*i) == incoming_packets.end())) {
                incoming_packet &ip = incoming_packets[*i];
                ip.flow = net->get_queue_flow_id(*i);
                ip.len = net->get_queue_length(*i);
                ip.xmit = net->receive(NULL, *i, net->get_queue_length(*i),
                                       &ip.id);
            }
        }
    }
    random_shuffle(flow_ids.begin(), flow_ids.end(), random_range);
    for (vector<flow_id>::iterator fi = flow_ids.begin(); fi != flow_ids.end(); ++fi) {
        tick_t t; len_t l; period_t p;
        tie(t, l, p) = flows[*fi];
        if (l != 0 && t == system_time) {
            waiting_packet pkt = { packet_id_factory->get_fresh_id(), *fi, l };
            waiting_packets[*fi].push(pkt);
            stats->offer_packet(*fi, l, pkt.id);
            flows[*fi].get<0>() = t + p;
        }
    }
    unsigned num_qs = net->get_egress()->get_remote_queues().size();
    for (unsigned i = 0; i < num_qs; ++i) { // allow one flow multi-queue bursts
        for (vector<flow_id>::iterator fi = flow_ids.begin();
             fi != flow_ids.end(); ++fi) {
            queue<waiting_packet> &q = waiting_packets[*fi];
            if (!q.empty()) {
                waiting_packet &pkt = q.front();
                if (net->send(fi->get_numeric_id(), NULL, pkt.len, pkt.id)) q.pop();
            }
        }
    }
}

void injector::tick_negative_edge() throw(err) { }

bool injector::is_drained() const throw() {
    bool drained = true;
    drained &= next_event == events.end();
    drained &= net->get_waiting_queues() == 0;
    for (uint32_t i = 0; i < 32; ++i) {
        drained &= incoming_packets.find(i) == incoming_packets.end();
    }
    for (vector<flow_id>::const_iterator fi = flow_ids.begin();
         fi != flow_ids.end(); ++fi) {
        tick_t t; len_t l; period_t p;
        flows_t::const_iterator fli = flows.find(*fi);
        if (fli != flows.end()) {
            tie(t, l, p) = fli->second;
            drained &= l == 0;
            waiting_packets_t::const_iterator wpi = waiting_packets.find(*fi);
            drained &= ((wpi == waiting_packets.end()) || wpi->second.empty());
        }
    }
    return drained;
}
