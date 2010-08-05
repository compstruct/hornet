// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cmath>
#include <set>
#include <algorithm>
#include <iomanip>
#include "statistics.hpp"

void combine_left(uint64_t &lhs, const uint64_t &rhs) {
    lhs += rhs;
}

void combine_left(running_stats &lhs, const running_stats &rhs) {
    lhs.combine_with(rhs);
}

void combine_left(reorder_buffer &lhs, const reorder_buffer &rhs) {
    abort();
}

template<class K, class V>
inline void combine_left(map<K, V> &lhs, const map<K, V> &rhs) {
    typename map<K, V>::iterator li = lhs.begin();
    typename map<K, V>::const_iterator ri = rhs.begin();
    while (li != lhs.end() || ri != rhs.end()) {
        if (li != lhs.end() && ri != rhs.end() && li->first == ri->first) {
            combine_left(li->second, ri->second);
            ++li;
            ++ri;
        } else if ((ri == rhs.end())
                   || (li != lhs.end() && li->first < ri->first)) {
            ++li;
        } else {
            lhs[ri->first] = ri->second;
            ++ri;
        }
    }
}

flow_rename_table::flow_rename_table() throw () :
    table() {
}

void flow_rename_table::add_flow_rename(const flow_id &from, const flow_id &to)
        throw (err) {
    assert(from != to);
    if (table.find(to) != table.end()) {
        if (table[to] != from) {
            throw err_duplicate_flow_rename(to.get_numeric_id(),
                    table[to].get_numeric_id(), from.get_numeric_id());
        }
    } else {
        table[to] = from;
    }
}

flow_id flow_rename_table::operator[](flow_id f) const throw () {
    table_t::const_iterator ti;
    while ((ti = table.find(f)) != table.end())
        f = ti->second;
    return f;
}

running_stats::running_stats() throw () :
    minimum(-log(0)), maximum(log(0)), mean(0), var_numer(0), weight_sum(0) {
}

void running_stats::reset() throw () {
    minimum = -log(0);
    maximum = log(0);
    mean = 0;
    var_numer = 0;
    weight_sum = 0;
}

void running_stats::add(double sample, double weight) throw () {
    if (!isnan(sample) && !isnan(weight)) { // NaN -> invalid sample
        if (sample < minimum)
            minimum = sample;
        if (sample > maximum)
            maximum = sample;
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

double running_stats::get_min() const throw () {
    return minimum;
}

double running_stats::get_max() const throw () {
    return maximum;
}

double running_stats::get_mean() const throw () {
    return mean;
}

double running_stats::get_std_dev() const throw () {
    return weight_sum == 0 ? 0 : sqrt(var_numer / weight_sum);
}

void running_stats::combine_with(const running_stats &rhs) throw () {
    minimum = min(minimum, rhs.minimum);
    maximum = max(maximum, rhs.maximum);
    if (weight_sum == 0) {
        mean = rhs.mean;
        var_numer = rhs.var_numer;
        weight_sum = rhs.weight_sum;
    } else if (rhs.weight_sum != 0) {
        mean = ((mean * weight_sum + rhs.mean * rhs.weight_sum) / (weight_sum
                + rhs.weight_sum));
        var_numer += rhs.var_numer;
        weight_sum += rhs.weight_sum;
    }
}

reorder_buffer::reorder_buffer() throw () :
    next_packet_ids(), buffered_packet_lengths(), received_count(0),
            out_of_order_count(0) {
}

void reorder_buffer::receive_packet(const head_flit &flt) throw () {
    flow_id fid = flt.get_flow_id();
    packet_id pid = flt.get_packet_id();
    flow_packet_id fpid = make_tuple(fid, pid);
    if (next_packet_ids.find(fid) == next_packet_ids.end()) {
        // XXX this should be the first ID of any packet sent on this flow
        // XXX and cannot be the first received packet because that onex
        // XXX could well be out of order
        next_packet_ids[fid] = 0;
    }
    if (pid > next_packet_ids[fid]) { // out of order
        buffered_packet_lengths[fpid] = flt.get_length();
        buffer_length += flt.get_length();
    } else { // in order
        assert(pid == next_packet_ids[fid]);
        ++next_packet_ids[fid];
        ++received_count;
        while (pid = next_packet_ids[fid], fpid = make_tuple(fid, pid), (buffered_packet_lengths.find(
                fpid) != buffered_packet_lengths.end())) {
            buffer_length -= buffered_packet_lengths[fpid];
            buffered_packet_lengths.erase(fpid);
            ++next_packet_ids[fid];
            ++received_count;
            ++out_of_order_count;
        }
        assert((buffer_length == 0) == (buffered_packet_lengths.empty()));
    }
}

uint32_t reorder_buffer::get_buffer_length() const throw () {
    return buffer_length;
}

uint32_t reorder_buffer::get_received_count() const throw () {
    return received_count;
}

uint32_t reorder_buffer::get_out_of_order_count() const throw () {
    return out_of_order_count;
}

tile_statistics::tile_statistics(const uint64_t &sys_time,
        const uint64_t &new_start_time,
        shared_ptr<flow_rename_table> new_flow_renames) throw () :
            system_time(sys_time), start_time(new_start_time), flow_renames(
            new_flow_renames), offered_flits(), total_offered_flits(0),
            sent_flits(), total_sent_flits(0), received_flits(),
            total_received_flits(0), total_offered_packets(0),
            total_sent_packets(0), total_received_packets(0), 
            vqwr_flits(), total_vqwr_flits(0), vqwr_bridge(), vqwr_port(),  
            vqrd_flits(), total_vqrd_flits(0), vqrd_bridge(), vqrd_port() { } 

bool tile_statistics::is_started() throw() {
    return system_time >= start_time;
}

void tile_statistics::reset() throw () {
    offered_flits.clear();
    total_offered_flits = 0;
    sent_flits.clear();
    total_sent_flits = 0;
    received_flits.clear();
    total_received_flits = 0;
    total_offered_packets = 0;
    total_sent_packets = 0;
    total_vqwr_flits = 0;
    total_vqrd_flits = 0;
    flow_flit_lat_stats.clear();
    packet_flit_lat_stats.clear();
    total_flit_lat_stats.reset();
    xbar_xmit_stats.clear();
    xbar_demand_stats.clear();
    xbar_bw_stats.clear();
    va_act_stage1_port_stats.clear();
    va_req_stage1_port_stats.clear();
    va_act_stage2_port_stats.clear();
    va_req_stage2_port_stats.clear();
    va_act_stage1_bridge_stats.clear();
    va_req_stage1_bridge_stats.clear();
    va_act_stage2_bridge_stats.clear();
    va_req_stage2_bridge_stats.clear();
    sw_act_stage1_port_stats.clear();
    sw_req_stage1_port_stats.clear();
    sw_act_stage2_port_stats.clear();
    sw_req_stage2_port_stats.clear();
    sw_act_stage1_bridge_stats.clear();
    sw_req_stage1_bridge_stats.clear();
    sw_act_stage2_bridge_stats.clear();
    sw_req_stage2_bridge_stats.clear();
    cxn_xmit_stats.clear();
    cxn_demand_stats.clear();
    cxn_bw_stats.clear();
    link_switches.clear();
    reorder_buffers.clear();
    last_received_times.clear();
    flow_reorder_stats.clear();
    vqwr_flits.clear();
    vqrd_flits.clear();
    vqwr_bridge.clear();
    vqrd_bridge.clear();
    vqwr_port.clear();
    vqrd_port.clear();
}

void tile_statistics::start_sim() throw () {}

void tile_statistics::end_sim() throw () {
    for (flow_stats_t::iterator fsi = flow_reorder_stats.begin(); fsi
            != flow_reorder_stats.end(); ++fsi) {
        const flow_id &fid = fsi->first;
        assert(reorder_buffers.find(fid) != reorder_buffers.end());
        assert(last_received_times.find(fid) != last_received_times.end());
        fsi->second.add(reorder_buffers[fsi->first].get_buffer_length(),
                system_time - last_received_times[fid]);
    }
}

uint64_t tile_statistics::get_received_packet_count() const throw () {
    return total_received_packets;
}

void tile_statistics::send_flit(const flow_id &org_fid, const flit &flt) throw () {
    flow_id fid = (*flow_renames)[org_fid];
    total_sent_flits++;
    if (sent_flits.find(fid) == sent_flits.end()) {
        sent_flits[fid] = 1;
    } else {
        sent_flits[fid]++;
    }
}

void tile_statistics::receive_flit(const flow_id &org_fid, const flit &flt) throw () {
    flow_id fid = (*flow_renames)[org_fid];
    total_received_flits++;
    if (received_flits.find(fid) == received_flits.end()) {
        received_flits[fid] = 1;
    } else {
        received_flits[fid]++;
    }
    total_flit_lat_stats.add(flt.get_age(), 1);
    flow_flit_lat_stats[fid].add(flt.get_age(), 1);
//---pengju---//
    int index = floor(flt.get_age()/100);//10 for 64 core 20 for 1000 cores
    if (index <= 500) { //2500 for 1000 cores, 300 for 64 cores
        packet_flit_lat_stats[index]++;
    } else {
        packet_flit_lat_stats[501]++;
    }
}

void tile_statistics::offer_packet(const flow_id &flow, const packet_id &pkt,
        uint32_t len) throw () {
    offered_flits[flow] += len + 1;
    total_offered_flits += len + 1;
    ++total_offered_packets;
}

void tile_statistics::send_packet(const flow_id &flow, const packet_id &pkt,
        uint32_t len) throw () {
    ++total_sent_packets;
}

void tile_statistics::receive_packet(const head_flit &flt) throw () {
    flow_id fid = flt.get_flow_id();
    // XXX flow_reorder_stats[fid].add(reorder_buffers[fid].get_buffer_length(),
    // XXX                             system_time - last_received_times[fid]);
    // XXX reorder_buffers[fid].receive_packet(flt);
   ++total_received_packets;
}

void tile_statistics::switch_links(const egress_id &src, const egress_id &dst,
        unsigned min_link, unsigned num_links) throw () {
    for (unsigned n = min_link; n < min_link + num_links; ++n) {
        sub_link_id l(src, dst, n);
        assert(link_switches.find(l) != link_switches.end());
        link_switches[l]++;
    }
}

void tile_statistics::xbar(node_id xbar_id, int sent_flits, int req_flits,
        double req_frac, double bw_frac) throw () {
    xbar_xmit_stats[xbar_id].add(sent_flits, 1);
    xbar_demand_stats[xbar_id].add(req_frac, 1);
    xbar_bw_stats[xbar_id].add(bw_frac, 1);
    }

void tile_statistics::cxn_xmit(node_id src, node_id dst, unsigned used,
        double req_frac, double bw_frac) throw () {
    cxn_id cxn = make_tuple(src, dst);
    cxn_xmit_stats[cxn].add(used, 1);
    cxn_demand_stats[cxn].add(req_frac, 1);
    cxn_bw_stats[cxn].add(bw_frac, 1);
}

void tile_statistics::vq_wr(const virtual_queue_node_id &vq_id,
                            const ingress_id &ig_id) throw() {
     string ig_name = ig_id.get_name();
     if(ig_name != "X") {
       total_vqwr_flits++;
       if (vqwr_flits.find(vq_id) == vqwr_flits.end()) {
           vqwr_flits[vq_id] = 0;
       }
       vqwr_flits[vq_id]++;

       node_id n =vq_id.get<0>();
       if (ig_name == "B") {
           if (vqwr_bridge.find(n) == vqwr_bridge.end()) {
               vqwr_bridge[n] = 0;
           }
          vqwr_bridge[n] ++;
       } else {
            if (vqwr_port.find(n) == vqwr_port.end()) {
                vqwr_port[n] = 0;
            }
          vqwr_port[n] ++;
       }
    }
} 
 
void tile_statistics::vq_rd(const virtual_queue_node_id &vq_id,
                            const ingress_id &ig_id) throw() {
       string ig_name = ig_id.get_name();
       if(ig_name != "X") {
       total_vqrd_flits++;
       if (vqrd_flits.find(vq_id) == vqrd_flits.end()) {
           vqrd_flits[vq_id] = 0;
       }
       vqrd_flits[vq_id]++;

       node_id n =vq_id.get<0>();
       if (ig_name == "B") {
           if (vqrd_bridge.find(n) == vqrd_bridge.end()) {
               vqrd_bridge[n] = 0;
           }
          vqrd_bridge[n] ++;
       } else {
            if (vqrd_port.find(n) == vqrd_port.end()) {
                vqrd_port[n] = 0;
            }
          vqrd_port[n] ++;
       }
   }
}

void tile_statistics::va_alloc(node_id id, int va_act_stage1_port, double va_req_stage1_port,
                               int va_act_stage2_port,   double va_req_stage2_port,
                               int va_act_stage1_bridge, double va_req_stage1_bridge,
                               int va_act_stage2_bridge, double va_req_stage2_bridge) throw() {
   if(this->is_started()) {
     va_act_stage1_port_stats[id].add(va_act_stage1_port,1);
     va_req_stage1_port_stats[id].add(va_req_stage1_port,1);
     va_act_stage2_port_stats[id].add(va_act_stage2_port,1);
     va_req_stage2_port_stats[id].add(va_req_stage2_port,1);

     va_act_stage1_bridge_stats[id].add(va_act_stage1_bridge,1);
     va_req_stage1_bridge_stats[id].add(va_req_stage1_bridge,1);
     va_act_stage2_bridge_stats[id].add(va_act_stage2_bridge,1);
     va_req_stage2_bridge_stats[id].add(va_req_stage2_bridge,1);
   }
}

void tile_statistics::sw_alloc(node_id id, int sw_act_stage1_port, double sw_req_stage1_port,
                               int sw_act_stage2_port, double sw_req_stage2_port,
                               int sw_act_stage1_bridge, double sw_req_stage1_bridge,
                               int sw_act_stage2_bridge, double sw_req_stage2_bridge) throw() {
   if(this->is_started()) {
     sw_act_stage1_port_stats[id].add(sw_act_stage1_port,1);
     sw_req_stage1_port_stats[id].add(sw_req_stage1_port,1);
     sw_act_stage2_port_stats[id].add(sw_act_stage2_port,1);
     sw_req_stage2_port_stats[id].add(sw_req_stage2_port,1);

     sw_act_stage1_bridge_stats[id].add(sw_act_stage1_bridge,1);
     sw_req_stage1_bridge_stats[id].add(sw_req_stage1_bridge,1);
     sw_act_stage2_bridge_stats[id].add(sw_act_stage2_bridge,1);
     sw_req_stage2_bridge_stats[id].add(sw_req_stage2_bridge,1);
   }
}

void tile_statistics::add_ingress(node_id src, node_id dst, uint64_t num_vqs,
                                  uint64_t bw2xbar,
                                  uint64_t flits_per_vq) throw() {
    cxn_id cid = make_tuple(src,dst);

    if (num_ingresses.find(cid) == num_ingresses.end()) {
        num_ingresses[cid] = 0;
    }
    num_ingresses[cid]++;

    if (num_virtual_queues.find(cid) == num_virtual_queues.end()) {
        num_virtual_queues[cid] = 0;
    }
    num_virtual_queues[cid] += num_vqs;

    if (ingress_bw2xbar.find(cid) == ingress_bw2xbar.end()) {
        ingress_bw2xbar[cid] = 0;
    }
    ingress_bw2xbar[cid] += bw2xbar;

    if (virtual_queue_depths.find(cid) == virtual_queue_depths.end()) {
        virtual_queue_depths[cid] = 0;
    }
    virtual_queue_depths[cid] += flits_per_vq;
}

void tile_statistics::add_egress(node_id src, node_id dst, uint64_t bandwidth) throw() {
    cxn_id cid = make_tuple(src,dst);
    if (num_egresses.find(cid) == num_egresses.end()) {
        num_egresses[cid] = 0;
    }
    num_egresses[cid]++;
    if (egress_bandwidth.find(cid) == egress_bandwidth.end()) {
        egress_bandwidth[cid] = 0;
    }
    egress_bandwidth[cid] += bandwidth;
}

system_statistics::system_statistics() throw () { }

uint64_t system_statistics::get_received_packet_count() const throw () {
    uint64_t received_packets_count = 0;
    for (system_statistics::tile_stats_t::const_iterator tsi =
            tile_stats.begin(); tsi != tile_stats.end(); ++tsi) {
        combine_left(received_packets_count,
                tsi->second->get_received_packet_count());
    }
    return received_packets_count;
}

void system_statistics::add(uint32_t id, shared_ptr<tile_statistics> s) throw () {
    assert(tile_stats.find(id) == tile_stats.end());
    tile_stats[id] = s;
}

void system_statistics::reset() throw () {
    for (tile_stats_t::iterator tsi = tile_stats.begin(); tsi
            != tile_stats.end(); ++tsi) {
        tsi->second->reset();
    }
}

void system_statistics::start_sim() throw () {
    for (tile_stats_t::iterator tsi = tile_stats.begin(); tsi
            != tile_stats.end(); ++tsi) {
        tsi->second->start_sim();
    }
}

void system_statistics::end_sim() throw () {
    for (tile_stats_t::iterator tsi = tile_stats.begin(); tsi
            != tile_stats.end(); ++tsi) {
        tsi->second->end_sim();
    }
}

static ostream &operator<<(ostream &out, const tuple<egress_id, egress_id,
        unsigned> &l) {
    egress_id src, dst;
    unsigned no;
    tie(src, dst, no) = l;
    return out << src << "<->" << dst << "/" << hex << setfill('0') << setw(2)
            << no;
}

ostream &operator<<(ostream &out, const system_statistics &s) {
    
    typedef tile_statistics::flit_counter_t flit_counter_t;
    typedef tile_statistics::flow_stats_t flow_stats_t;
    typedef tile_statistics::flit_stats_t flit_stats_t;
    typedef tile_statistics::node_stats_t node_stats_t;
    typedef tile_statistics::cxn_stats_t cxn_stats_t;
    typedef tile_statistics::link_switch_counter_t link_switch_counter_t;
    typedef tile_statistics::vq_counter_t vq_counter_t;
    typedef tile_statistics::vq_node_counter_t vq_node_counter_t;
    typedef tile_statistics::node_counter_t node_counter_t;
    typedef tile_statistics::cxn_counter_t cxn_counter_t;
    typedef tile_statistics::cxn_id cxn_id;

    flit_counter_t offered_flits;
    uint64_t total_offered_flits = 0;
    flit_counter_t sent_flits;
    uint64_t total_sent_flits = 0;
    flit_counter_t received_flits;
    uint64_t total_received_flits = 0;
    uint64_t total_offered_packets = 0;
    uint64_t total_sent_packets = 0;
    uint64_t total_received_packets = 0;
    vq_counter_t vqwr_flits;
    uint64_t total_vqwr_flits = 0;
    vq_node_counter_t vqwr_bridge;
    vq_node_counter_t vqwr_port;
    vq_counter_t vqrd_flits;
    uint64_t total_vqrd_flits = 0;
    vq_node_counter_t vqrd_bridge;
    vq_node_counter_t vqrd_port;

    flow_stats_t flow_flit_lat_stats;
    flit_stats_t packet_flit_lat_stats;
    running_stats total_flit_lat_stats;
    node_stats_t xbar_xmit_stats;
    node_stats_t xbar_demand_stats;
    node_stats_t xbar_bw_stats;
    node_stats_t va_act_stage1_port_stats;
    node_stats_t va_req_stage1_port_stats;
    node_stats_t va_act_stage2_port_stats;
    node_stats_t va_req_stage2_port_stats;
    node_stats_t va_act_stage1_bridge_stats;
    node_stats_t va_req_stage1_bridge_stats;
    node_stats_t va_act_stage2_bridge_stats;
    node_stats_t va_req_stage2_bridge_stats;
    node_stats_t sw_act_stage1_port_stats;
    node_stats_t sw_req_stage1_port_stats;
    node_stats_t sw_act_stage2_port_stats;
    node_stats_t sw_req_stage2_port_stats;
    node_stats_t sw_act_stage1_bridge_stats;
    node_stats_t sw_req_stage1_bridge_stats;
    node_stats_t sw_act_stage2_bridge_stats;
    node_stats_t sw_req_stage2_bridge_stats;
    cxn_stats_t cxn_xmit_stats;
    cxn_stats_t cxn_demand_stats;
    cxn_stats_t cxn_bw_stats;
    link_switch_counter_t link_switches;
    map<flow_id, reorder_buffer> reorder_buffers;
    flow_stats_t flow_reorder_stats;

    // combine per-tile statistics
    for (system_statistics::tile_stats_t::const_iterator tsi =
         s.tile_stats.begin(); tsi != s.tile_stats.end(); ++tsi) {
        const shared_ptr<tile_statistics> &ts = tsi->second;
        combine_left(offered_flits, ts->offered_flits);
        combine_left(total_offered_flits, ts->total_offered_flits);
        combine_left(sent_flits, ts->sent_flits);
        combine_left(total_sent_flits, ts->total_sent_flits);
        combine_left(received_flits, ts->received_flits);
        combine_left(total_received_flits, ts->total_received_flits);
        combine_left(total_offered_packets, ts->total_offered_packets);
        combine_left(total_sent_packets, ts->total_sent_packets);
        combine_left(total_received_packets, ts->total_received_packets);
        combine_left(flow_flit_lat_stats, ts->flow_flit_lat_stats);
        combine_left(packet_flit_lat_stats, ts->packet_flit_lat_stats);
        combine_left(total_flit_lat_stats, ts->total_flit_lat_stats);
        combine_left(xbar_xmit_stats, ts->xbar_xmit_stats);
        combine_left(xbar_demand_stats, ts->xbar_demand_stats);
        combine_left(xbar_bw_stats, ts->xbar_bw_stats);
        combine_left(va_act_stage1_port_stats, ts->va_act_stage1_port_stats);
        combine_left(va_req_stage1_port_stats, ts->va_req_stage1_port_stats);
        combine_left(va_act_stage2_port_stats, ts->va_act_stage2_port_stats);
        combine_left(va_req_stage2_port_stats, ts->va_req_stage2_port_stats);
        combine_left(va_act_stage1_bridge_stats, ts->va_act_stage1_bridge_stats);
        combine_left(va_req_stage1_bridge_stats, ts->va_req_stage1_bridge_stats);
        combine_left(va_act_stage2_bridge_stats, ts->va_act_stage2_bridge_stats);
        combine_left(va_req_stage2_bridge_stats, ts->va_req_stage2_bridge_stats);
        combine_left(sw_act_stage1_port_stats, ts->sw_act_stage1_port_stats);
        combine_left(sw_req_stage1_port_stats, ts->sw_req_stage1_port_stats);
        combine_left(sw_act_stage2_port_stats, ts->sw_act_stage2_port_stats);
        combine_left(sw_req_stage2_port_stats, ts->sw_req_stage2_port_stats);
        combine_left(sw_act_stage1_bridge_stats, ts->sw_act_stage1_bridge_stats);
        combine_left(sw_req_stage1_bridge_stats, ts->sw_req_stage1_bridge_stats);
        combine_left(sw_act_stage2_bridge_stats, ts->sw_act_stage2_bridge_stats);
        combine_left(sw_req_stage2_bridge_stats, ts->sw_req_stage2_bridge_stats);
        combine_left(cxn_xmit_stats, ts->cxn_xmit_stats);
        combine_left(cxn_demand_stats, ts->cxn_demand_stats);
        combine_left(cxn_bw_stats, ts->cxn_bw_stats);
        combine_left(link_switches, ts->link_switches);
        combine_left(reorder_buffers, ts->reorder_buffers);
        combine_left(flow_reorder_stats, ts->flow_reorder_stats);
        combine_left(vqwr_flits, ts->vqwr_flits);
        combine_left(total_vqwr_flits, ts->total_vqwr_flits); 
        combine_left(vqrd_flits, ts->vqrd_flits);
        combine_left(total_vqrd_flits, ts->total_vqrd_flits); 
        combine_left(vqwr_bridge, ts-> vqwr_bridge);
        combine_left(vqrd_bridge, ts-> vqrd_bridge);
        combine_left(vqwr_port, ts-> vqwr_port);
        combine_left(vqrd_port, ts-> vqrd_port);
    }


    set<flow_id> flow_ids;
    for (flit_counter_t::const_iterator i = offered_flits.begin(); i
            != offered_flits.end(); ++i) {
        flow_ids.insert(i->first);
    }
    for (flit_counter_t::const_iterator i = sent_flits.begin(); i
            != sent_flits.end(); ++i) {
        flow_ids.insert(i->first);
    }
    for (flit_counter_t::const_iterator i = received_flits.begin(); i
            != received_flits.end(); ++i) {
        flow_ids.insert(i->first);
    }
    out << "flit counts:" << endl;
    for (set<flow_id>::const_iterator i = flow_ids.begin(); i != flow_ids.end(); ++i) {
        const flow_id &f = *i;
        uint64_t offered, sent, received;
        flit_counter_t::const_iterator fci;
        if ((fci = offered_flits.find(f)) != offered_flits.end()) {
            offered = fci->second;
        } else {
            offered = 0;
        }
        if ((fci = sent_flits.find(f)) != sent_flits.end()) {
            sent = fci->second;
        } else {
            sent = 0;
        }
        if ((fci = received_flits.find(f)) != received_flits.end()) {
            received = fci->second;
        } else {
            received = 0;
        }
        out << "    flow " << f << ": offered " << dec << offered << ", sent "
            << sent << ", received " << received << " (" << sent - received
            << " in flight)" << endl;
    }
    out << "    all flows counts: " << "offered " << dec << total_offered_flits
        << ", sent " << total_sent_flits << ", received "
        << total_received_flits << " ("
        << total_sent_flits - total_received_flits << " in flight)"
        << endl << endl;

    out << "in-network sent flit latencies (mean +/- s.d., [min..max] "
            << "in # cycles):" << endl;
    for (set<flow_id>::const_iterator i = flow_ids.begin(); i != flow_ids.end(); ++i) {
        const flow_id &f = *i;
        if (received_flits.find(f) != received_flits.end()) {
            assert(flow_flit_lat_stats.find(f) != flow_flit_lat_stats.end());
            assert(received_flits[f] > 0);
            running_stats &inc_stats = flow_flit_lat_stats[f];
            out << "    flow " << f << ": " << dec << inc_stats.get_mean()
                << " +/- " << inc_stats.get_std_dev() << ", range ["
                << inc_stats.get_min() << ".." << inc_stats.get_max()
                << "]" << endl;
        }
    }
    out << "    all flows in-network flit latency: " << dec
        << total_flit_lat_stats.get_mean() << " +/- "
        << total_flit_lat_stats.get_std_dev() << endl << endl;

    out << "in-network sent flit collections in #cycles):" << endl;
    for (flit_stats_t::const_iterator i = packet_flit_lat_stats.begin();
         i != packet_flit_lat_stats.end(); ++i) {
         int time = i->first;
         uint64_t counts = i->second;
         out << "    flits received between " << time << "0 and " << (time+1) << "0 :" 
             << dec << counts << endl;
    }
    out << endl;

    out << "link switch counts:" << endl;
    uint64_t total_switches = 0;
    typedef link_switch_counter_t link_switch_counter_t;
    for (link_switch_counter_t::const_iterator i = link_switches.begin(); i
            != link_switches.end(); ++i) {
        total_switches += i->second;
        out << "    link " << i->first << ": bandwidths changed " << dec
            << i->second << " time" << (i->second == 1 ? "" : "s") << endl;
    }
    out << "    all links: bandwidths changed " << dec << total_switches
        << " time" << (total_switches == 1 ? "" : "s") << endl << endl;

    out << "xbar transmission statistics (mean +/- s.d.):" << endl;
    for (node_stats_t::const_iterator nsi = xbar_xmit_stats.begin(); nsi
            != xbar_xmit_stats.end(); ++nsi) {
        const running_stats &xmit = xbar_xmit_stats[nsi->first];
        if (xmit.get_mean() == 0)
            continue; // no activity, don't report
        const running_stats &dem = xbar_demand_stats[nsi->first];
        const running_stats &bw = xbar_bw_stats[nsi->first];
        out << "    xbar " << nsi->first << ":" << fixed << setprecision(2)
                << " flits " << xmit.get_mean() << " +/- "
                << xmit.get_std_dev() << setprecision(0) << " demand " << 100
                * dem.get_mean() << "% +/- " << 100 * dem.get_std_dev() << "%"
                << " bw " << 100 * bw.get_mean() << "% +/- " << 100
                * bw.get_std_dev() << "%" << endl;
    }
    out << endl;
    out << "link transmission statistics (mean +/- s.d.):" << endl;
    for (cxn_stats_t::const_iterator csi = cxn_xmit_stats.begin(); csi
            != cxn_xmit_stats.end(); ++csi) {
        node_id src, dst;
        tie(src, dst) = csi->first;
        const running_stats &xmit = cxn_xmit_stats[csi->first];
        if (xmit.get_mean() == 0)
            continue; // no activity, don't report
        const running_stats &dem = cxn_demand_stats[csi->first];
        const running_stats &bw = cxn_bw_stats[csi->first];
        out << "    link " << src << "->" << dst << ":" << fixed
            << setprecision(2) << " flits " << xmit.get_mean() << " +/- "
            << xmit.get_std_dev() << setprecision(0)
            << " demand " << 100 * dem.get_mean()
            << "% +/- " << 100 * dem.get_std_dev() << "%"
            << " bw " << 100 * bw.get_mean()
            << "% +/- " << 100 * bw.get_std_dev() << "%" << endl;
    }

   out << endl;

    set<node_id> vq_node_ids;
    for (vq_node_counter_t::const_iterator i = vqwr_bridge.begin();
         i != vqwr_bridge.end(); ++i) {
         vq_node_ids.insert(i->first);
    }
    for (vq_node_counter_t::const_iterator i = vqrd_bridge.begin();
         i != vqrd_bridge.end(); ++i) {
         vq_node_ids.insert(i->first);
    }
    for (vq_node_counter_t::const_iterator i = vqwr_port.begin();
         i != vqwr_port.end(); ++i) {
         vq_node_ids.insert(i->first);
    }
    for (vq_node_counter_t::const_iterator i = vqrd_port.begin();
         i != vqrd_port.end(); ++i) {
         vq_node_ids.insert(i->first);
    }

    out << "Virtual queues operation in nodes counts:" <<endl;
    for(set<node_id>::const_iterator i = vq_node_ids.begin();
        i != vq_node_ids.end(); ++i) {
        const node_id &n = *i;
        uint64_t stats_time = s.get_stats_time(i->get_numeric_id());
        uint64_t wrote_bridge, read_bridge, wrote_port, read_port;
        vq_node_counter_t::const_iterator ni;
        if ((ni = vqwr_bridge.find(n))!= vqwr_bridge.end()){
            wrote_bridge = ni -> second;
        } else {
            wrote_bridge = 0;
        }
        if ((ni = vqrd_bridge.find(n))!= vqrd_bridge.end()){
            read_bridge = ni -> second;
        } else {
            read_bridge = 0;
        }
        if ((ni = vqwr_port.find(n))!= vqwr_port.end()){
            wrote_port = ni -> second;
        } else {
            wrote_port = 0;
        }
        if ((ni = vqrd_port.find(n))!= vqrd_port.end()){
            read_port = ni -> second;
        } else {
            read_port = 0;
        }
        double bridge_write_flits = static_cast<double> (wrote_bridge) 
                                    / static_cast<double> (stats_time);
        double bridge_read_flits  = static_cast<double> (read_bridge) 
                                    / static_cast<double> (stats_time);
        double port_write_flits = static_cast<double> (wrote_port) 
                                  / static_cast<double> (stats_time);
        double port_read_flits  = static_cast<double> (read_port) 
                                  / static_cast<double> (stats_time);

        out << "    vq in node " << n 
            << " total write " << fixed << setprecision(2) 
            << bridge_write_flits + port_write_flits
            << " bridge "  << setprecision(2) << bridge_write_flits
            << " port "  << setprecision(2) << port_write_flits
            << ", total read " << setprecision(2) 
            << bridge_read_flits + port_read_flits
            << " bridge " << setprecision(2) << bridge_read_flits
            << " port " << setprecision(2) << port_read_flits 
            << " stats_time " << setprecision(2) << stats_time << endl;
    }
            
    out << " all virtual queues operation counts: "
        << " wrote " << dec << total_vqwr_flits
        << ", read " << total_vqrd_flits 
        << endl <<endl;
  
   out << "Virtual alloctor operation statistics (mean +/- s.d.):" << endl;
   for (node_stats_t::const_iterator nsi = va_act_stage1_port_stats.begin();
        nsi != va_act_stage1_port_stats.end(); ++nsi) {
        const running_stats &va_act_port_stage1 = va_act_stage1_port_stats[nsi->first];
        if(va_act_port_stage1.get_mean() == 0) continue;
        const running_stats &va_req_port_stage1 = va_req_stage1_port_stats[nsi->first];
        const running_stats &va_act_port_stage2 = va_act_stage2_port_stats[nsi->first];
        const running_stats &va_req_port_stage2 = va_req_stage2_port_stats[nsi->first];
        const running_stats &va_act_bridge_stage1 = va_act_stage1_bridge_stats[nsi->first];
        const running_stats &va_req_bridge_stage1 = va_req_stage1_bridge_stats[nsi->first];
        const running_stats &va_act_bridge_stage2 = va_act_stage2_bridge_stats[nsi->first];
        const running_stats &va_req_bridge_stage2 = va_req_stage2_bridge_stats[nsi->first];
        out << "    VA in node " << nsi->first << ":" << fixed << setprecision(2)
            << " port S1 : act " << va_act_port_stage1.get_mean() << " +/- " 
            << va_act_port_stage1.get_std_dev() << setprecision(2) << " reqs "
            << va_req_port_stage1.get_mean() << " +/- "<< va_req_port_stage1.get_std_dev() 
            << " port S2 : act " << va_act_port_stage2.get_mean() << " +/- " 
            << va_act_port_stage2.get_std_dev() << setprecision(2) << " reqs " 
            << va_req_port_stage2.get_mean() << " +/- "<< va_req_port_stage2.get_std_dev() << endl
            << "    bridge S1 : act " << va_act_bridge_stage1.get_mean() << " +/- " 
            << va_act_bridge_stage1.get_std_dev() << setprecision(2) << " reqs "
            << va_req_bridge_stage1.get_mean() << " +/- "<< va_req_bridge_stage1.get_std_dev() 
            << " bridge S2 : act " << va_act_bridge_stage2.get_mean() << " +/- " 
            << va_act_bridge_stage2.get_std_dev() << setprecision(2) << " reqs " 
            << va_req_bridge_stage2.get_mean() << " +/- "<< va_req_bridge_stage2.get_std_dev() 
            << endl;
   }
   out << endl;      

   out << "Switch alloctor operation statistics (mean +/- s.d.):" << endl;
   for (node_stats_t::const_iterator nsi = sw_act_stage1_port_stats.begin();
        nsi != sw_act_stage1_port_stats.end(); ++nsi) {
        const running_stats &sw_act_port_stage1 = sw_act_stage1_port_stats[nsi->first];
        if(sw_act_port_stage1.get_mean() == 0) continue;
        const running_stats &sw_req_port_stage1 = sw_req_stage1_port_stats[nsi->first];
        const running_stats &sw_act_port_stage2 = sw_act_stage2_port_stats[nsi->first];
        const running_stats &sw_req_port_stage2 = sw_req_stage2_port_stats[nsi->first];
        const running_stats &sw_act_bridge_stage1 = sw_act_stage1_bridge_stats[nsi->first];
        const running_stats &sw_req_bridge_stage1 = sw_req_stage1_bridge_stats[nsi->first];
        const running_stats &sw_act_bridge_stage2 = sw_act_stage2_bridge_stats[nsi->first];
        const running_stats &sw_req_bridge_stage2 = sw_req_stage2_bridge_stats[nsi->first];
        out << "    SW in node " << nsi->first << ":" << fixed << setprecision(2)
            << " port S1 : act " << sw_act_port_stage1.get_mean() << " +/- " 
            << sw_act_port_stage1.get_std_dev() << setprecision(2) << " reqs "
            << sw_req_port_stage1.get_mean() << " +/- "<< sw_req_port_stage1.get_std_dev() 
            << " port S2 : act " << sw_act_port_stage2.get_mean() << " +/- " 
            << sw_act_port_stage2.get_std_dev() << setprecision(2) << " reqs " 
            << sw_req_port_stage2.get_mean() << " +/- "<< sw_req_port_stage2.get_std_dev() << endl
            << "    bridge S1 : act " << sw_act_bridge_stage1.get_mean() << " +/- " 
            << sw_act_bridge_stage1.get_std_dev() << setprecision(2) << " reqs "
            << sw_req_bridge_stage1.get_mean() << " +/- "<< sw_req_bridge_stage1.get_std_dev() 
            << " bridge S2 : act " << sw_act_bridge_stage2.get_mean() << " +/- " 
            << sw_act_bridge_stage2.get_std_dev() << setprecision(2) << " reqs " 
            << sw_req_bridge_stage2.get_mean() << " +/- "<< sw_req_bridge_stage2.get_std_dev() 
            << endl;
   }
   out << endl; 

   out << "Router Power using Orion2.0: " << endl;
   double total_router_power = 0;   
   for (vq_node_counter_t::const_iterator ni = vqwr_port.begin();
        ni != vqwr_port.end(); ni++ ) {
        router_power_d router_power;
        uint32_t node_index = ni->first.get_numeric_id();
        uint64_t stats_time = s.get_stats_time(node_index);
        
        power_statistic_d power_stats;
        system_parameter_d system_parameter;

        power_stats.bridge_vq_w = static_cast<double> (vqwr_bridge[ni->first]) 
                                / static_cast<double> (stats_time);
        power_stats.bridge_vq_r = static_cast<double> (vqrd_bridge[ni->first]) 
                                / static_cast<double> (stats_time);
        power_stats.port_vq_w = static_cast<double> (vqwr_port[ni->first]) 
                                / static_cast<double> (stats_time);
        power_stats.port_vq_r = static_cast<double> (vqrd_port[ni->first]) 
                                / static_cast<double> (stats_time);

        power_stats.xbar_trans = xbar_xmit_stats[ni->first].get_mean();
        power_stats.va_act_stage1_port = va_act_stage1_port_stats[ni->first].get_mean();
        power_stats.va_req_stage1_port = va_req_stage1_port_stats[ni->first].get_mean();
        power_stats.va_act_stage2_port = va_act_stage2_port_stats[ni->first].get_mean();
        power_stats.va_req_stage2_port = va_req_stage2_port_stats[ni->first].get_mean();
        power_stats.va_act_stage1_bridge = va_act_stage1_bridge_stats[ni->first].get_mean();
        power_stats.va_req_stage1_bridge = va_req_stage1_bridge_stats[ni->first].get_mean();
        power_stats.va_act_stage2_bridge = va_act_stage2_bridge_stats[ni->first].get_mean();
        power_stats.va_req_stage2_bridge = va_req_stage2_bridge_stats[ni->first].get_mean();
       
        power_stats.sw_act_stage1_port = sw_act_stage1_port_stats[ni->first].get_mean();
        power_stats.sw_req_stage1_port = sw_req_stage1_port_stats[ni->first].get_mean();
        power_stats.sw_act_stage2_port = sw_act_stage2_port_stats[ni->first].get_mean();
        power_stats.sw_req_stage2_port = sw_req_stage2_port_stats[ni->first].get_mean();
        power_stats.sw_act_stage1_bridge = sw_act_stage1_bridge_stats[ni->first].get_mean();
        power_stats.sw_req_stage1_bridge = sw_req_stage1_bridge_stats[ni->first].get_mean();
        power_stats.sw_act_stage2_bridge = sw_act_stage2_bridge_stats[ni->first].get_mean();
        power_stats.sw_req_stage2_bridge = sw_req_stage2_bridge_stats[ni->first].get_mean();

        uint64_t total_num_input = 0,total_num_output =0,
                 total_num_virtual_queue =0,total_virtual_queue_depths =0,
                 total_bw2xbar =0,total_bandwidth =0;
                 
        shared_ptr<tile_statistics> tile_stats = s.get_tile_stats(ni->first.get_numeric_id()); 
        
        cxn_id cid = make_tuple(ni->first,ni->first);
        system_parameter.bridge_in = tile_stats->num_ingresses[cid];
        system_parameter.bridge_out = tile_stats->num_egresses[cid];
        system_parameter.vq_per_bridge    = tile_stats->num_virtual_queues[cid];
        system_parameter.flits_per_bridge_vq = tile_stats->virtual_queue_depths[cid];
        system_parameter.bridge_bw2xbar    = tile_stats->ingress_bw2xbar[cid];
        system_parameter.bridge_bandwidth  = tile_stats->egress_bandwidth[cid];


        for(cxn_counter_t::iterator it = tile_stats->num_ingresses.begin(); 
                                    it != tile_stats->num_ingresses.end(); it ++)
        total_num_input += (*it).second;
        system_parameter.input_ports = total_num_input - system_parameter.bridge_in;

        for(cxn_counter_t::iterator it = tile_stats->num_egresses.begin(); 
                                    it != tile_stats->num_egresses.end(); it ++)
        total_num_output += (*it).second;
        system_parameter.output_ports = total_num_output - system_parameter.bridge_out;

        for(cxn_counter_t::iterator it = tile_stats->num_virtual_queues.begin(); 
                                    it != tile_stats->num_virtual_queues.end(); it ++)
        total_num_virtual_queue += (*it).second;
        system_parameter.vq_per_port = (total_num_virtual_queue - system_parameter.vq_per_bridge) / 
                                        system_parameter.input_ports;

        for(cxn_counter_t::iterator it = tile_stats->virtual_queue_depths.begin(); 
                                    it != tile_stats->virtual_queue_depths.end(); it ++)
        total_virtual_queue_depths += (*it).second;
        system_parameter.flits_per_port_vq = (total_virtual_queue_depths - 
                         system_parameter.flits_per_bridge_vq) / system_parameter.input_ports;
                            
        for(cxn_counter_t::iterator it = tile_stats->ingress_bw2xbar.begin(); 
                                    it != tile_stats->ingress_bw2xbar.end(); it ++)
        total_bw2xbar += (*it).second;
        system_parameter.port_bw2xbar = (total_bw2xbar - system_parameter.bridge_bw2xbar) / 
                                         system_parameter.input_ports;
        
        for(cxn_counter_t::iterator it = tile_stats->egress_bandwidth.begin(); 
                                    it != tile_stats->egress_bandwidth.end(); it ++)
        total_bandwidth += (*it).second;
        system_parameter.port_bandwidth = (total_bandwidth - system_parameter.bridge_bandwidth) / 
                                           system_parameter.output_ports;

        router_power = DarOrn_power(node_index, 
                                    system_parameter,
                                    power_stats);

        total_router_power += router_power.total;
        out << "router " << ni->first 
            << " : power: " << setprecision(5) << router_power.total 
            << " : dyn: " << setprecision(5) << router_power.dynamic_p
            << " : static: " << setprecision(5) << router_power.static_p << endl << endl;  
    }

    out << "Link Power using Orion2.0: " << endl;
    float total_link_power = 0;
    link_power_d link_power;
    for (cxn_stats_t::const_iterator csi = cxn_xmit_stats.begin();
         csi != cxn_xmit_stats.end(); ++csi) {
        node_id src, dst; tie(src,dst) = csi->first;
        cxn_id cid = make_tuple(src,dst);
        const running_stats &xmit = cxn_xmit_stats[csi->first];
        if (xmit.get_mean() == 0) continue; // no activity, don't report
        uint64_t bandwidth = s.get_tile_stats(src.get_numeric_id())->
                             egress_bandwidth[cid];
        link_power = Link_power(xmit.get_mean(), bandwidth);        
        total_link_power += link_power.total;
        out << "    link " << src << "->" << dst  
            << " bandwidth: " << bandwidth
            << " power: " << setprecision(5) << link_power.total
            << " dyn: "   << setprecision(5) << link_power.dynamic_p
            << " static: " << setprecision(5) << link_power.static_p << endl;
    }
   out << endl;
   
   out << "total power : " << (total_router_power + total_link_power)
       << " routers power : " << total_router_power 
       << " links power : " << total_link_power << endl;
    /*
    bool have_out_of_order = false;
    out << endl;
    out << "out-of-order packet counts and reorder buffer sizes "
        << "(in # flits):" << endl;
    for (flow_stats_t::iterator fsi = flow_reorder_stats.begin();
         fsi != flow_reorder_stats.end(); ++fsi) {
        const flow_id &fid = fsi->first;
        assert(reorder_buffers.find(fid) != reorder_buffers.end());
        const reorder_buffer &rob = reorder_buffers[fid];
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
    */
    return out;
}
