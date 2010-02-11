// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include <iostream>
#include <set>
#include <map>
#include <queue>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "cstdint.hpp"
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "flit.hpp"
#include "virtual_queue_id.hpp"
#include "egress_id.hpp"
#include "vcd.hpp"
#include "logger.hpp"

using namespace std;
using namespace boost;
using namespace boost::posix_time;

class running_stats {
public:
    explicit running_stats() throw();
    void add(double sample, double weight) throw();
    double get_min() const throw();
    double get_max() const throw();
    double get_mean() const throw();
    double get_std_dev() const throw();
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
    void send_packet(const head_flit &flt) throw();
    void receive_packet(const head_flit &flt) throw();
    uint32_t get_buffer_length() const throw();
    uint32_t get_received_count() const throw();
    uint32_t get_out_of_order_count() const throw();
private:
    queue<flit_id> sent_packets; // head flit UIDs
    map<flit_id, unsigned> buffered_packets; // with length
    uint32_t buffer_length;
    uint32_t received_count;
    uint32_t out_of_order_count;
};

class statistics {
public:
    statistics(const uint64_t &system_time, const uint64_t &start_time,
               logger &log, shared_ptr<vcd_writer> vcd) throw();
    void start_sim() throw();
    void end_sim() throw();
    void send_flit(const flow_id &, const flit &) throw();
    void receive_flit(const flow_id &, const flit &) throw();
    void offer_packet(const flow_id &fid, const uint32_t num_data_flits,
                      const packet_id &pid) throw();
    void send_packet(const flow_id &fid, const head_flit &flt) throw();
    void receive_packet(const flow_id &fid, const head_flit &flt) throw();
    void complete_packet(const flow_id &fid, const uint32_t num_data_flits,
                         const packet_id &pid) throw();
    void register_links(const egress_id &src, const egress_id &dst,
                        unsigned num_links) throw();
    void register_flow(const flow_id &id) throw(err);
    void register_flow_rename(const flow_id &from,
                              const flow_id &to) throw(err);
    void register_node(const node_id &id) throw(err);
    void register_queue(const virtual_queue_node_id &id) throw(err);
    void switch_links(const egress_id &src, const egress_id &dst,
                      unsigned min_link, unsigned num_links) throw();
    void xbar(node_id id, int transmitted_flits, int requested_flits,
              double req_frac, double bw_frac) throw();
    void cxn_xmit(node_id src, node_id dst, unsigned used,
                  double req_frac, double bw_frac) throw();
    void virtual_queue(const virtual_queue_node_id &id, size_t size) throw();
    uint64_t get_received_packet_count() const throw();
    friend ostream &operator<<(ostream &, statistics &);
private:
    flow_id get_original_flow(flow_id f) const throw();
private:
    const uint64_t &system_time;
    const uint64_t start_time;
    ptime sim_start_time;
    ptime sim_end_time;
    typedef map<flow_id, flow_id> flow_renames_t;
    typedef map<flow_id, uint64_t> flit_counter_t;
    typedef tuple<egress_id, egress_id, unsigned> sub_link_id;
    typedef map<sub_link_id, uint64_t> link_switch_counter_t;
    typedef map<flit_id, uint64_t> flit_timestamp_t;
    typedef map<packet_id, uint64_t> packet_timestamp_t;
    typedef map<packet_id, flow_id> packet_flows_t;
    typedef map<node_id, running_stats> node_stats_t;
    typedef map<flow_id, running_stats> flow_stats_t;
    typedef tuple<node_id, node_id> cxn_id;
    typedef map<cxn_id, running_stats> cxn_stats_t;
    flow_renames_t original_flows;
    bool have_first_flit_id;
    flit_id first_flit_id; // first flit sent after stats start
    flit_counter_t offered_flits;
    uint64_t total_offered_flits;
    flit_counter_t sent_flits;
    uint64_t total_sent_flits;
    flit_counter_t received_flits;
    uint64_t total_received_flits;
    uint64_t total_offered_packets;
    uint64_t total_sent_packets;
    uint64_t total_received_packets;
    flit_timestamp_t flit_departures;
    flow_stats_t flow_flit_lat_stats;
    running_stats total_flit_lat_stats;
    packet_flows_t packet_flows;
    packet_timestamp_t packet_offers;
    packet_timestamp_t packet_sends;
    flow_stats_t flow_packet_lat_stats;
    running_stats total_packet_lat_stats;
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
    logger &log;
    shared_ptr<vcd_writer> vcd;
private:
    typedef struct {
        char v_offered;
        char v_sent;
        char v_received;
    } vcd_flow_hooks_t;
    typedef map<flow_id, vcd_flow_hooks_t> vcd_flows_t;
    vcd_flows_t vcd_flows;
    typedef struct {
        char v_xbar_demand;
        char v_xbar_use;
    } vcd_node_hooks_t;
    typedef map<node_id, vcd_node_hooks_t> vcd_nodes_t;
    vcd_nodes_t vcd_nodes;
    typedef struct {
        char v_size;
    } vcd_vq_hooks_t;
    typedef map<virtual_queue_node_id, vcd_vq_hooks_t> vcd_vqs_t;
    vcd_vqs_t vcd_vqs;
};

ostream &operator<<(ostream &, const statistics &);

#endif // __STATISTICS_HPP__
