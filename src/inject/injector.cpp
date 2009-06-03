// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cstdint.hpp"
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <algorithm>
#include "random.hpp"
#include "injector.hpp"

injector::injector(const pe_id &id, uint64_t &t, shared_ptr<statistics> st,
                   logger &l) throw(err)
    : pe(id), system_time(t), net(), events(), next_event(events.begin()),
      flows(), flow_ids(), queue_ids(), stats(st), log(l) { }

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
    uint32_t waiting = net->get_waiting_queues();
    if (waiting != 0) {
        random_shuffle(queue_ids.begin(), queue_ids.end(), random_range);
        for (vector<uint32_t>::iterator i = queue_ids.begin();
             i != queue_ids.end(); ++i) {
            if ((waiting >> *i) & 1) {
                net->receive(NULL, *i, net->get_queue_length(*i));
            }
        }
    }
    random_shuffle(flow_ids.begin(), flow_ids.end(), random_range);
    for (vector<flow_id>::iterator fi = flow_ids.begin();
         fi != flow_ids.end(); ++fi) {
        tick_t t; len_t l; period_t p;
        tie(t, l, p) = flows[*fi];
        if (l != 0 && t <= system_time) {
            if (t + p == system_time) t += p; // forget unsent packet
            if (t == system_time) stats->offer_flits(*fi, l);
            if (net->send(fi->get_numeric_id(), NULL, l-1)) {
                t += p; // schedule next packet for p ticks
            }
            flows[*fi].get<0>() = t;
        }
    }
}

void injector::tick_negative_edge() throw(err) { }
