// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cmath>
#include <set>
#include <algorithm>
#include <iomanip>
#include "statistics.hpp"

running_stats::running_stats() throw()
    : minimum(-log(0)), maximum(log(0)),
      mean(0), var_numer(0), weight_sum(0) { }

void running_stats::add(double sample, double weight) throw() {
    if (!isnan(sample) && !isnan(weight)) { // NaN -> invalid sample
        if (sample < minimum) minimum = sample;
        if (sample > maximum) maximum = sample;
        if (weight > 0) {
            double s = weight_sum + weight;
            double q = sample - mean;
            double r = q * weight / s;
            mean += r;
            var_numer += r * weight_sum * q;
            weight_sum = s;
        }
    }
}

double running_stats::get_min() const throw() { return minimum; }

double running_stats::get_max() const throw() { return maximum; }

double running_stats::get_mean() const throw() { return mean; }

double running_stats::get_std_dev() const throw() {
    return weight_sum == 0 ? 0 : sqrt(var_numer/weight_sum);
}

reorder_buffer::reorder_buffer() throw()
    : sent_packets(), buffered_packets(), buffer_length(0),
      received_count(0), out_of_order_count(0) { }

void reorder_buffer::send_packet(const head_flit &flt) throw() {
    sent_packets.push(flt.get_uid());
}

void reorder_buffer::receive_packet(const head_flit &flt) throw() {
    if (sent_packets.empty() || (flt.get_uid() < sent_packets.front()))
        return; // packet sent before statistics collection started
    assert((buffer_length == 0) == (buffered_packets.empty()));
    assert(buffered_packets.find(flt.get_uid()) == buffered_packets.end());
    ++received_count;
    if (flt.get_uid() != sent_packets.front()) { // out-of-order arrival
        buffered_packets[flt.get_uid()] = flt.get_length();
        buffer_length += flt.get_length();
        ++out_of_order_count;
    } else { // in-order arrival
        sent_packets.pop();
        while (!sent_packets.empty()
               && (buffered_packets.find(sent_packets.front())
                   != buffered_packets.end())) { // next packet already buffered
            assert(buffer_length >= buffered_packets[sent_packets.front()]);
            buffer_length -= buffered_packets[sent_packets.front()];
            buffered_packets.erase(sent_packets.front());
            assert((buffer_length == 0) == (buffered_packets.empty()));
            sent_packets.pop();
        }
    }
}

uint32_t reorder_buffer::get_buffer_length() const throw() {
    return buffer_length;
}

uint32_t reorder_buffer::get_received_count() const throw() {
    return received_count;
}

uint32_t reorder_buffer::get_out_of_order_count() const throw() {
    return out_of_order_count;
}

statistics::statistics(const uint64_t &sys_time, const uint64_t &start,
                       logger &l) throw()
    : system_time(sys_time), start_time(start),
      sim_start_time(microsec_clock::local_time()),
      sim_end_time(sim_start_time), original_flows(),
      have_first_flit_id(false), first_flit_id(-1),
      offered_flits(), total_offered_flits(0),
      sent_flits(), total_sent_flits(0),
      received_flits(), total_received_flits(0),
      flit_departures(), flow_flit_lat_stats(), total_flit_lat_stats(),
      packet_flows(), packet_offers(), packet_sends(),
      flow_packet_lat_stats(), total_packet_lat_stats(),
      link_switches(), reorder_buffers(), flow_reorder_stats(), log(l) { }

void statistics::start_sim() throw() {
    sim_start_time = microsec_clock::local_time();
    sim_end_time = sim_start_time;
}

void statistics::end_sim() throw() {
    sim_end_time = microsec_clock::local_time();
    for (flow_stats_t::iterator fsi = flow_reorder_stats.begin();
         fsi != flow_reorder_stats.end(); ++fsi) {
        const flow_id &fid = fsi->first;
        assert(reorder_buffers.find(fid) != reorder_buffers.end());
        assert(last_received_times.find(fid) != last_received_times.end());
        fsi->second.add(reorder_buffers[fsi->first].get_buffer_length(),
                        system_time - last_received_times[fid]);
    }
    for (packet_timestamp_t::const_iterator poi = packet_offers.begin();
         poi != packet_offers.end(); ++poi) {
        const packet_id &pid = poi->first;
        double flight_time = system_time - poi->second;
        total_packet_lat_stats.add(flight_time, 1);
        assert(packet_flows.find(pid) != packet_flows.end());
        flow_packet_lat_stats[packet_flows[pid]].add(flight_time, 1);
        packet_flows.erase(pid);
    }
    packet_offers.clear();
}

flow_id statistics::get_original_flow(flow_id f) const throw() {
    flow_renames_t::const_iterator fri;
    while ((fri = original_flows.find(f)) != original_flows.end())
        f = fri->second;
    return f;
}

void statistics::send_flit(const flow_id &fid, const flit &flt) throw() {
    assert(get_original_flow(fid) == fid);
    if (system_time >= start_time) { // count as sent flit
        if (!have_first_flit_id) {
            first_flit_id = flt.get_uid();
            have_first_flit_id = true;
        }
        total_sent_flits++;
        flit_counter_t::iterator i = sent_flits.find(fid);
        if (i == sent_flits.end()) {
            sent_flits[fid] = 1;
        } else {
            sent_flits[fid]++;
        }
        assert(total_offered_flits >= total_sent_flits);
        assert(flit_departures.find(flt.get_uid()) == flit_departures.end());
        flit_departures[flt.get_uid()] = system_time;
    } else {
        assert(offered_flits.find(fid) != offered_flits.end());
        --offered_flits[fid];
        --total_offered_flits;
    }
    assert(offered_flits[fid] >= sent_flits[fid]);
    assert(total_offered_flits >= total_sent_flits);
}

void statistics::receive_flit(const flow_id &org_fid, const flit &flt) throw() {
    flow_id fid = get_original_flow(org_fid);
    if (have_first_flit_id && flt.get_uid() >= first_flit_id) {
        flit_timestamp_t::iterator di = flit_departures.find(flt.get_uid());
        if (di != flit_departures.end()) {
            double flight_time = system_time - di->second;
            total_received_flits++;
            total_flit_lat_stats.add(flight_time, 1);
            if (received_flits.find(fid) == received_flits.end()) {
                received_flits[fid] = 1;
            } else {
                received_flits[fid]++;
            }
            flow_flit_lat_stats[fid].add(flight_time, 1);
            flit_departures.erase(flt.get_uid());
        }
    }
}

void statistics::offer_packet(const flow_id &fid, const uint32_t orig_len,
                              const packet_id &pid) throw() {
    const uint32_t len = orig_len + 1; // original length plus head flit
    assert(packet_flows.find(pid) == packet_flows.end());
    packet_flows[pid] = fid;
    assert(packet_offers.find(pid) == packet_offers.end());
    packet_offers[pid] = system_time;
    // count flits as offered but forget them if they're sent before stats start
    total_offered_flits += len;
    flit_counter_t::iterator i = offered_flits.find(fid);
    if (i == offered_flits.end()) {
        offered_flits[fid] = len;
    } else {
        offered_flits[fid] += len;
    }
}

void statistics::send_packet(const flow_id &fid, const head_flit &flt) throw() {
    const uint32_t len = flt.get_length() + 1; // original length plus head flit
    const packet_id &pid = flt.get_packet_id();
    if (packet_offers.find(pid) == packet_offers.end()) { // no separate offer
        offer_packet(fid, len - 1, pid);
    }
    if (system_time >= start_time) {
        assert(packet_sends.find(pid) == packet_sends.end());
        packet_sends[pid] = system_time;
        reorder_buffers[fid].send_packet(flt);
    } else { // pretend packet was never offered
        packet_offers.erase(pid);
        packet_flows.erase(pid);
        assert(offered_flits.find(flt.get_flow_id()) != offered_flits.end());
        assert(offered_flits[flt.get_flow_id()] >= len);
    }
}

void statistics::receive_packet(const flow_id &fid,
                                const head_flit &flt) throw() {
    if (system_time >= start_time) {
        if (last_received_times.find(fid) == last_received_times.end())
            last_received_times[fid] = start_time;
        flow_reorder_stats[fid].add(reorder_buffers[fid].get_buffer_length(),
                                    system_time - last_received_times[fid]);
        reorder_buffers[fid].receive_packet(flt);
        last_received_times[fid] = system_time;
    }
}

void statistics::complete_packet(const flow_id &fid, const uint32_t len,
                                 const packet_id &pid) throw() {
    if (system_time >= start_time) {
        packet_timestamp_t::iterator poi = packet_offers.find(pid);
        packet_timestamp_t::iterator psi = packet_sends.find(pid);
        if (poi != packet_offers.end() && psi != packet_sends.end()) {
            double flight_time = system_time - poi->second;
            total_packet_lat_stats.add(flight_time, 1);
            flow_packet_lat_stats[fid].add(flight_time, 1);
            packet_offers.erase(pid);
            packet_flows.erase(pid);
        }
    }
}

void statistics::register_links(const egress_id &src, const egress_id &dst,
                                unsigned num_links) throw() {
    for (unsigned n = 0; n < num_links; ++n) {
        sub_link_id l(src,dst,n);
        assert(link_switches.find(l) == link_switches.end());
        link_switches[l] = 0;
    }
}

void statistics::register_flow_rename(const flow_id &from,
                                      const flow_id &to) throw (err) {
    assert(from != to);
    if (original_flows.find(to) != original_flows.end()) {
        if (original_flows[to] != from) {
            throw err_duplicate_flow_rename(to.get_numeric_id(),
                                            original_flows[to].get_numeric_id(),
                                            from.get_numeric_id());
        }
    } else {
        original_flows[to] = from;
    }
}

void statistics::switch_links(const egress_id &src, const egress_id &dst,
                              unsigned min_link, unsigned num_links) throw() {
    if (system_time >= start_time) {
        for (unsigned n = min_link; n < min_link + num_links; ++n) {
            sub_link_id l(src,dst,n);
            assert(link_switches.find(l) != link_switches.end());
            link_switches[l]++;
        }
    }
}

void statistics::xbar(node_id xbar_id, int flits, double req_frac,
                      double bw_frac) throw() {
    xbar_xmit_stats[xbar_id].add(flits, 1);
    xbar_demand_stats[xbar_id].add(req_frac, 1);
    xbar_bw_stats[xbar_id].add(bw_frac, 1);
}

void statistics::cxn_xmit(node_id src, node_id dst, unsigned used,
                          double req_frac, double bw_frac) throw() {
    cxn_id cxn = make_tuple(src,dst);
    cxn_xmit_stats[cxn].add(used, 1);
    cxn_demand_stats[cxn].add(req_frac, 1);
    cxn_bw_stats[cxn].add(bw_frac, 1);
}

static ostream &operator<<(ostream &out,
                           const tuple<egress_id, egress_id, unsigned> &l) {
    egress_id src, dst; unsigned no; tie(src,dst,no) = l;
    return out << src << "<->" << dst << "/"
               << hex << setfill('0') << setw(2) << no;
}

ostream &operator<<(ostream &out, statistics &stats) {
    if (stats.sim_start_time == stats.sim_end_time) {
        stats.sim_end_time = microsec_clock::local_time();
    }
    time_duration sim_time = stats.sim_end_time - stats.sim_start_time;
    uint64_t stats_time = stats.system_time - stats.start_time;
    set<flow_id> flow_ids;
    typedef statistics::flit_counter_t flit_counter_t;
    for (flit_counter_t::const_iterator i = stats.offered_flits.begin();
         i != stats.offered_flits.end(); ++i) {
        flow_ids.insert(i->first);
    }
    for (flit_counter_t::const_iterator i = stats.sent_flits.begin();
         i != stats.sent_flits.end(); ++i) {
        flow_ids.insert(i->first);
    }
    for (flit_counter_t::const_iterator i = stats.received_flits.begin();
         i != stats.received_flits.end(); ++i) {
        flow_ids.insert(i->first);
    }
    double cycles_per_s = ((static_cast<double>(stats.system_time) * 1.0e6) /
                            static_cast<double>(sim_time.total_microseconds()));
    out << "total simulation time:   " << dec << sim_time << endl
        << "total simulation cycles: " << dec << stats.system_time
        << " (" << cycles_per_s << " Hz)" << endl
        << "total statistics cycles: " << dec << stats_time
        << " (statistics start after cycle " << dec << stats.start_time
        << ")" << endl << endl;
    
    out << "flit counts:" << endl;
    for (set<flow_id>::const_iterator i = flow_ids.begin();
         i != flow_ids.end(); ++i) {
        const flow_id &f = *i;
        uint64_t offered, sent, received;
        flit_counter_t::const_iterator fci;
        if ((fci = stats.offered_flits.find(f))
            != stats.offered_flits.end()) {
            offered = fci->second;
        } else {
            offered = 0;
        }
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
        assert(offered >= sent);
        assert(sent >= received);
        out << "    flow " << f << ": offered " << dec
            << offered << ", sent " << sent << ", received "
            << received << " (" << sent - received << " in flight)"
            << endl;
    }
    assert(stats.total_offered_flits >= stats.total_sent_flits);
    assert(stats.total_sent_flits >= stats.total_received_flits);
    out << "    all flows counts: "
        << "offered " << dec << stats.total_offered_flits
        << ", sent " << stats.total_sent_flits
        << ", received " << stats.total_received_flits
        << " (" << stats.total_sent_flits - stats.total_received_flits
        << " in flight)" << endl << endl;

    out << "end-to-end injected packet latencies (mean +/- s.d., in # cycles):"
        << endl;
    for (set<flow_id>::const_iterator i = flow_ids.begin();
         i != flow_ids.end(); ++i) {
        const flow_id &f = *i;
        if (stats.flow_packet_lat_stats.find(f) != stats.flow_packet_lat_stats.end()) {
            running_stats &inc_stats = stats.flow_packet_lat_stats[f];
            out << "    flow " << f << ": "
                << dec << inc_stats.get_mean() << " +/- "
                << inc_stats.get_std_dev() << endl;
        }
    }
    out << "    all flows end-to-end packet latency: "
        << dec << stats.total_packet_lat_stats.get_mean()
        << " +/- " << stats.total_packet_lat_stats.get_std_dev() << endl << endl;

    out << "in-network sent flit latencies (mean +/- s.d., in # cycles):"
        << endl;
    for (set<flow_id>::const_iterator i = flow_ids.begin();
         i != flow_ids.end(); ++i) {
        const flow_id &f = *i;
        if (stats.received_flits.find(f) != stats.received_flits.end()) {
            assert(stats.flow_flit_lat_stats.find(f) != stats.flow_flit_lat_stats.end());
            uint64_t received = stats.received_flits[f];
            assert(received > 0);
            running_stats &inc_stats = stats.flow_flit_lat_stats[f];
            out << "    flow " << f << ": "
                << dec << inc_stats.get_mean() << " +/- "
                << inc_stats.get_std_dev() << endl;
        }
    }
    out << "    all flows in-network flit latency: "
        << dec << stats.total_flit_lat_stats.get_mean()
        << " +/- " << stats.total_flit_lat_stats.get_std_dev() << endl << endl;
    
    out << "link switch counts:" << endl;
    uint64_t total_switches = 0;
    typedef statistics::link_switch_counter_t link_switch_counter_t;
    for (link_switch_counter_t::const_iterator i = stats.link_switches.begin();
         i != stats.link_switches.end(); ++i) {
        total_switches += i->second;
        out << "    link " << i->first << ": bandwidths changed "
            << dec << i->second << " time" << (i->second == 1 ? "" : "s")
            << endl;
    }
    out << "    all links: bandwidths changed " << dec << total_switches
        << " time" << (total_switches == 1 ? "" : "s") << endl << endl;

    out << "xbar transmission statistics (mean +/- s.d.):" << endl;
    for (statistics::node_stats_t::const_iterator nsi =
         stats.xbar_xmit_stats.begin();
         nsi != stats.xbar_xmit_stats.end(); ++nsi) {
        const running_stats &xmit = stats.xbar_xmit_stats[nsi->first];
        if (xmit.get_mean() == 0) continue; // no activity, don't report
        const running_stats &dem = stats.xbar_demand_stats[nsi->first];
        const running_stats &bw = stats.xbar_bw_stats[nsi->first];
        out << "    xbar " << nsi->first << ":" << fixed << setprecision(2)
            << " flits " << xmit.get_mean() << " +/- " << xmit.get_std_dev()
            << setprecision(0) << " demand " << 100 * dem.get_mean() << "% +/- "
            << 100 * dem.get_std_dev() << "%"
            << " bw " << 100 * bw.get_mean() << "% +/- "
            << 100 * bw.get_std_dev() << "%" << endl;
    }
    out << endl;
    out << "link transmission statistics (mean +/- s.d.):" << endl;
    for (statistics::cxn_stats_t::const_iterator csi =
         stats.cxn_xmit_stats.begin();
         csi != stats.cxn_xmit_stats.end(); ++csi) {
        node_id src, dst; tie(src,dst) = csi->first;
        const running_stats &xmit = stats.cxn_xmit_stats[csi->first];
        if (xmit.get_mean() == 0) continue; // no activity, don't report
        const running_stats &dem = stats.cxn_demand_stats[csi->first];
        const running_stats &bw = stats.cxn_bw_stats[csi->first];
        out << "    link " << src << "->" << dst << ":"
            << fixed << setprecision(2)
            << " flits " << xmit.get_mean() << " +/- " << xmit.get_std_dev()
            << setprecision(0) << " demand " << 100 * dem.get_mean() << "% +/- "
            << 100 * dem.get_std_dev() << "%"
            << " bw " << 100 * bw.get_mean() << "% +/- "
            << 100 * bw.get_std_dev() << "%" << endl;
    }
    bool have_out_of_order = false;
    out << endl;
    out << "out-of-order packet counts and reorder buffer sizes "
        << "(in # flits):" << endl;
    for (statistics::flow_stats_t::iterator fsi =
             stats.flow_reorder_stats.begin();
         fsi != stats.flow_reorder_stats.end(); ++fsi) {
        const flow_id &fid = fsi->first;
        assert(stats.reorder_buffers.find(fid) != stats.reorder_buffers.end());
        const reorder_buffer &rob = stats.reorder_buffers[fid];
        if (rob.get_out_of_order_count() > 0) {
            have_out_of_order = true;
            const running_stats &rs = fsi->second;
            double frac = (static_cast<double>(rob.get_out_of_order_count()) /
                           static_cast<double>(rob.get_received_count()));
            out << "    flow " << fid << ": "
                << dec << setprecision(0) << rob.get_out_of_order_count()
                << " of " << rob.get_received_count() << " packet"
                << (rob.get_received_count() == 1 ? "" : "s")
                << " (" << (frac * 100) << "%)" << "; buffer max. "
                << rs.get_max() << setprecision(2) << ", mean "
                << rs.get_mean() << " +/- " << rs.get_std_dev() << endl;
        }
    }
    if (!have_out_of_order) {
        out << "    all packets arrived in correct order" << endl;
    }
    return out;
}

