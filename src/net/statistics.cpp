// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <set>
#include <algorithm>
#include <iomanip>
#include "statistics.hpp"

statistics::statistics(const uint64_t &sys_time, const uint64_t &start) throw()
    : system_time(sys_time), start_time(start),
      sim_start_time(microsec_clock::local_time()),
      sim_end_time(sim_start_time) { }

void statistics::start_sim() throw() {
    sim_start_time = microsec_clock::local_time();
    sim_end_time = sim_start_time;
}

void statistics::end_sim() throw() {
    sim_end_time = microsec_clock::local_time();
}

void statistics::send_flit(const flow_id &f) throw() {
    if (system_time >= start_time) {
        flit_counter_t::iterator i = sent_flits.find(f);
        if (i == sent_flits.end()) {
            sent_flits[f] = 1;
        } else {
            sent_flits[f]++;
        }
    }
}

void statistics::receive_flit(const flow_id &f) throw() {
    if (system_time >= start_time) {
        flit_counter_t::iterator i = received_flits.find(f);
        if (i == received_flits.end()) {
            received_flits[f] = 1;
        } else {
            received_flits[f]++;
        }
    }
}


ostream &operator<<(ostream &out, statistics &stats) {
    if (stats.sim_start_time == stats.sim_end_time) {
        stats.sim_end_time = microsec_clock::local_time();
    }
    time_duration sim_time = stats.sim_end_time - stats.sim_start_time;
    uint64_t stats_time = stats.system_time - stats.start_time;
    uint64_t total_sent = 0;
    uint64_t total_received = 0;
    set<flow_id> flow_ids;
    typedef statistics::flit_counter_t flit_counter_t;
    for (flit_counter_t::const_iterator i = stats.sent_flits.begin();
         i != stats.sent_flits.end(); ++i) {
        flow_ids.insert(i->first);
        total_sent += i->second;
    }
    for (flit_counter_t::const_iterator i = stats.received_flits.begin();
         i != stats.received_flits.end(); ++i) {
        flow_ids.insert(i->first);
        total_received += i->second;
    }
    double cycles_per_s = ((static_cast<double>(stats.system_time) * 1.0e6) /
                            static_cast<double>(sim_time.total_microseconds()));
    out << "total simulation time:   " << dec << sim_time << endl
        << "total simulation cycles: " << dec << stats.system_time
        << " (" << cycles_per_s << " Hz)" << endl
        << "total statistics cycles: " << dec << stats_time
        << " (statistics start after cycle " << dec << stats.start_time
        << ")" << endl
        << "flit counts:" << endl;
    for (set<flow_id>::const_iterator i = flow_ids.begin();
         i != flow_ids.end(); ++i) {
        const flow_id &f = *i;
        uint64_t sent, received;
        flit_counter_t::const_iterator fci;
        if ((fci = stats.sent_flits.find(f))
            != stats.sent_flits.end()) {
            sent = fci->second;
        } else {
            sent = 0;
        }
        if ((fci = stats.received_flits.find(f))
            != stats.received_flits.end()) {
            received = fci->second;
        } else {
            received = 0;
        }
        out << "    flow " << f << ": " << dec << sent
            << " sent and " << dec << received
            << " received (" << dec << (sent >= received ? sent - received : 0)
            << " in flight)" << endl;
    }
    out << "    all flows: " << dec << total_sent
        << " sent and " << dec << total_received
        << " received (" << dec
        << (total_sent >= total_received ? total_sent - total_received : 0)
        << " in flight)" << endl;
    return out;
}

