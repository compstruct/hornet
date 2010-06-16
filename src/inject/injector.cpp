// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <algorithm>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include "random.hpp"
#include "injector.hpp"

injector::injector(const pe_id &id, const uint64_t &t,
                   shared_ptr<id_factory<packet_id> > pif,
                   shared_ptr<tile_statistics> st, logger &l,
                   shared_ptr<random_gen> r) throw(err)
    : pe(id), system_time(t), next_event(events.begin()),
      packet_id_factory(pif), stats(st), log(l), ran(r) { }

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
        flows[f] = make_tuple(UINT64_MAX, 0, UINT64_MAX);
    }
}

void injector::set_stop_darsim() throw(err) { }

void injector::tick_positive_edge() throw(err) {
    for (; next_event != events.end() && next_event->get<0>() == system_time;
         ++next_event) {
        tick_t t; flow_id f; len_t l; period_t p;
        tie(t, f, l, p) = *next_event;
        if (p > 0) {
            flows[f] = make_tuple(system_time, l, p);
        } else if (l > 0) {
            waiting_packet pkt =
                { packet_id_factory->get_fresh_id(), f, l, stats->is_started() };
            waiting_packets[f].push(pkt);
            if (pkt.count_in_stats) {
                stats->offer_packet(pkt.flow, pkt.id, pkt.len);
            }
        }
        LOG(log,2) << "[injector " << get_id() << "] flow " << f;
        if (l == 0) {
            LOG(log,2) << " off" << endl;
        } else if (p == 0) {
            LOG(log,2) << " sending a "
                       << dec << (l + 1) << "-flit packet" << endl;
        } else {
            LOG(log,2) << " sending a "
                       << dec << (l + 1) << "-flit packet every "
                       << dec << p << " tick" << (p == 1 ? "" : "s") << endl;
        }
    }
    for (uint32_t i = 0; i < 32; ++i) {
        if (incoming_packets.find(i) != incoming_packets.end()) {
            incoming_packet &ip = incoming_packets[i];
            if (net->get_transmission_done(ip.xmit)) {
                incoming_packets.erase(i);
            }
        }
    }
    uint32_t waiting = net->get_waiting_queues();
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);
    if (waiting != 0) {
        random_shuffle(queue_ids.begin(), queue_ids.end(), rr_fn);
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
    random_shuffle(flow_ids.begin(), flow_ids.end(), rr_fn);
    for (vector<flow_id>::iterator fi = flow_ids.begin();
         fi != flow_ids.end(); ++fi) {
        tick_t t; len_t l; period_t p; tie(t, l, p) = flows[*fi];
        if (l != 0 && t == system_time) {
            waiting_packet pkt = { packet_id_factory->get_fresh_id(), *fi, l,
                                   stats->is_started() };
            waiting_packets[*fi].push(pkt);
            if (pkt.count_in_stats) {
                stats->offer_packet(pkt.flow, pkt.id, pkt.len);
            }
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
                if (net->send(fi->get_numeric_id(), NULL, pkt.len, pkt.id,
                              pkt.count_in_stats)) {
                    q.pop();
                }
            }
        }
    }
}

void injector::tick_negative_edge() throw(err) { }

void injector::add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err) { }

bool injector::work_queued() throw(err) {
    return 0;
}

bool injector::is_ready_to_offer() throw(err) {
    return 0;
}

uint64_t injector::next_pkt_time() throw(err) {
    uint64_t next_time = UINT64_MAX;
    if (!incoming_packets.empty()) {
        return system_time;
    }
    for (waiting_packets_t::const_iterator wpi = waiting_packets.begin();
         wpi != waiting_packets.end(); ++wpi) {
        if (!wpi->second.empty()) return system_time;
    }
    if (next_event != events.end()) {
        next_time = next_event->get<0>();
    }
    for (flows_t::const_iterator fi = flows.begin(); fi != flows.end(); ++fi) {
        if (fi->second.get<0>() < next_time) {
            next_time = fi->second.get<0>();
        }
    }
    return next_time;
}

bool injector::is_drained() const throw() {
    bool drained = true;
    drained &= net->get_waiting_queues() == 0;
    for (uint32_t i = 0; i < 32; ++i) {
        drained &= incoming_packets.find(i) == incoming_packets.end();
    }
    for (waiting_packets_t::const_iterator wpi = waiting_packets.begin();
         wpi != waiting_packets.end(); ++wpi) {
        drained &= wpi->second.empty();
    }
    return drained;
}
