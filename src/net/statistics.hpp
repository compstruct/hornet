// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include "cstdint.hpp"
#include <iostream>
#include <set>
#include <map>
#include <queue>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/thread.hpp>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "flit.hpp"
#include "virtual_queue_id.hpp"
#include "egress_id.hpp"

using namespace std;
using namespace boost;

class flow_rename_table {
public:
    explicit flow_rename_table() throw();
    void add_flow_rename(const flow_id &from, const flow_id &to) throw(err);
    flow_id operator[](flow_id f) const throw();
private:
    typedef map<flow_id, flow_id> table_t;
    table_t table;
};

class running_stats {
public:
    explicit running_stats() throw();
    void add(double sample, double weight) throw();
    void reset() throw();
    double get_min() const throw();
    double get_max() const throw();
    double get_mean() const throw();
    double get_std_dev() const throw();
    void combine_with(const running_stats &other_stats) throw();
private:
    double minimum;
    double maximum;
    double mean;
    double var_numer;
    double weight_sum;
};

class reorder_buffer {
public:
    explicit reorder_buffer() throw();
    void receive_packet(const head_flit &flt) throw();
    uint32_t get_buffer_length() const throw();
    uint32_t get_received_count() const throw();
    uint32_t get_out_of_order_count() const throw();
private:
    map<flow_id, packet_id> next_packet_ids;
    map<flow_packet_id, uint32_t> buffered_packet_lengths;
    uint32_t buffer_length;
    uint32_t received_count;
    uint32_t out_of_order_count;
};

class system_statistics;

class tile_statistics {
public:
    tile_statistics(const uint64_t &system_time, const uint64_t &start_time,
                    shared_ptr<flow_rename_table> flow_renames) throw();
    bool is_started() throw();
    void reset() throw();
    void start_sim() throw();
    void end_sim() throw();
    void send_flit(const flow_id &, const flit &) throw();
    void receive_flit(const flow_id &, const flit &) throw();
    void offer_packet(const flow_id &flow, const packet_id &pkt,
                      uint32_t len) throw();
    void send_packet(const flow_id &flow, const packet_id &pkt,
                     uint32_t len) throw();
    void receive_packet(const head_flit &flt) throw();
    void switch_links(const egress_id &src, const egress_id &dst,
                      unsigned min_link, unsigned num_links) throw();
    void xbar(node_id id, int transmitted_flits, int requested_flits,
              double req_frac, double bw_frac) throw();
    void cxn_xmit(node_id src, node_id dst, unsigned used,
                  double req_frac, double bw_frac) throw();
    uint64_t get_received_packet_count() const throw();
public:
    friend ostream &operator<<(ostream &, const system_statistics &);
private:
    const uint64_t &system_time;
    const uint64_t start_time;
    shared_ptr<flow_rename_table> flow_renames;
    typedef map<flow_id, uint64_t> flit_counter_t;
    typedef tuple<egress_id, egress_id, unsigned> sub_link_id;
    typedef map<sub_link_id, uint64_t> link_switch_counter_t;
    typedef map<node_id, running_stats> node_stats_t;
    typedef map<flow_id, running_stats> flow_stats_t;
    typedef tuple<node_id, node_id> cxn_id;
    typedef map<cxn_id, running_stats> cxn_stats_t;
    flit_counter_t offered_flits;
    uint64_t total_offered_flits;
    flit_counter_t sent_flits;
    uint64_t total_sent_flits;
    flit_counter_t received_flits;
    uint64_t total_received_flits;
    uint64_t total_offered_packets;
    uint64_t total_sent_packets;
    uint64_t total_received_packets;
    flow_stats_t flow_flit_lat_stats;
    running_stats total_flit_lat_stats;
    node_stats_t xbar_xmit_stats;
    node_stats_t xbar_demand_stats;
    node_stats_t xbar_bw_stats;
    cxn_stats_t cxn_xmit_stats;
    cxn_stats_t cxn_demand_stats;
    cxn_stats_t cxn_bw_stats;
    link_switch_counter_t link_switches;
    map<flow_id, reorder_buffer> reorder_buffers;
    map<flow_id, uint64_t> last_received_times;
    flow_stats_t flow_reorder_stats;
private:
    typedef struct {
        char v_offered;
        char v_sent;
        char v_received;
    } vcd_flow_hooks_t;
    typedef map<flow_id, vcd_flow_hooks_t> vcd_flows_t;
    vcd_flows_t vcd_flows;
};

class system_statistics {
public:
    explicit system_statistics() throw();
    uint64_t get_received_packet_count() const throw();
    void add(uint32_t id, shared_ptr<tile_statistics> new_stats) throw();
    void reset() throw();
    void start_sim() throw();
    void end_sim() throw();
public:
    friend ostream &operator<<(ostream &, const system_statistics &);
private:
    typedef map<uint32_t, shared_ptr<tile_statistics> > tile_stats_t;
    tile_stats_t tile_stats;
};

ostream &operator<<(ostream &, const system_statistics &);

#endif // __STATISTICS_HPP__
