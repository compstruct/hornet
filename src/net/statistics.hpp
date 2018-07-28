// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include "cstdint.hpp"
#include <iostream>
#include <set>
#include <map>
#include <queue>
#include <boost/thread.hpp>
#include "error.hpp"
#include "flow_id.hpp"
#include "node_id.hpp"
#include "flit.hpp"
#include "virtual_queue_id.hpp"
#include "egress_id.hpp"
#include "DarOrn_power.hpp"
#include "ingress_id.hpp"

extern "C" {
#include "SIM_router.h"
}

using namespace std;
using namespace boost;

class flow_rename_table {
public:
    explicit flow_rename_table();
    void add_flow_rename(const flow_id &from, const flow_id &to);
    flow_id operator[](flow_id f) const;
private:
    typedef map<flow_id, flow_id> table_t;
    table_t table;
};

class running_stats {
public:
    explicit running_stats();
    void add(double sample, double weight);
    void reset();
    double get_min() const;
    double get_max() const;
    double get_mean() const;
    double get_std_dev() const;
    void combine_with(const running_stats &other_stats);
    inline uint64_t sample_count()  {return num_samples;}
private:
    double minimum;
    double maximum;
    double mean;
    double var_numer;
    double weight_sum;
    uint64_t num_samples;
};

class reorder_buffer {
public:
    explicit reorder_buffer();
    void receive_packet(const head_flit &flt);
    uint32_t get_buffer_length() const;
    uint32_t get_received_count() const;
    uint32_t get_out_of_order_count() const;
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
                    std::shared_ptr<flow_rename_table> flow_renames);
    bool is_started();
    void reset();
    void start_sim();
    void end_sim();
    void send_flit(const flow_id &, const flit &);
    void receive_flit(const flow_id &, const flit &);
    void offer_packet(const flow_id &flow, const packet_id &pkt,
                      uint32_t len);
    void send_packet(const flow_id &flow, const packet_id &pkt,
                     uint32_t len);
    void receive_packet(const head_flit &flt);
    void switch_links(const egress_id &src, const egress_id &dst,
                      unsigned min_link, unsigned num_links);
    void xbar(node_id id, int transmitted_flits, int requested_flits,
              double req_frac, double bw_frac);
    void cxn_xmit(node_id src, node_id dst, unsigned used,
                  double req_frac, double bw_frac);
    void add_ingress(node_id src, node_id dst, uint64_t num_vqs,
                     uint64_t bw2xbar,
                     uint64_t flits_per_vq);
    void add_egress(node_id src, node_id dst, uint64_t bandwidth);
    void va_alloc(node_id id, int va_act_stage1_port, double va_req_stage1_port,
                              int va_act_stage2_port, double va_req_stage2_port,
                              int va_act_stage1_bridge, double va_req_stage1_bridge,
                              int va_act_stage2_bridge, double va_req_stage2_bridge);
    void sw_alloc(node_id id, int sw_act_stage1_port, double sw_req_stage1_port,
                              int sw_act_stage2_port, double sw_req_stage2_port,
                              int sw_act_stage1_bridge, double sw_req_stage1_bridge,
                              int sw_act_stage2_bridge, double sw_req_stage2_bridge);
    uint64_t get_received_packet_count() const;
    void vq_wr(const virtual_queue_node_id &vq_id, const ingress_id &ig_id);
    void vq_rd(const virtual_queue_node_id &vq_id, const ingress_id &ig_id);
    inline uint64_t get_system_time() const;
    inline uint64_t get_start_time() const;

public:
    friend ostream &operator<<(ostream &, const system_statistics &);
private:
    const uint64_t &system_time;
    const uint64_t &start_time;
    std::shared_ptr<flow_rename_table> flow_renames;
    typedef map<flow_id, uint64_t> flit_counter_t;
    typedef map<virtual_queue_node_id, uint64_t> vq_counter_t;
    typedef map<node_id, uint64_t> vq_node_counter_t;
    typedef std::tuple<egress_id, egress_id, unsigned> sub_link_id;
    typedef map<sub_link_id, uint64_t> link_switch_counter_t;
    typedef map<node_id, uint64_t> node_counter_t;
    typedef map<node_id, running_stats> node_stats_t;
    typedef map<flow_id, running_stats> flow_stats_t;
    typedef map<int,uint64_t> flit_stats_t;
    typedef std::tuple<node_id, node_id> cxn_id;
    typedef map<cxn_id, running_stats> cxn_stats_t;
    typedef map<cxn_id, uint64_t> cxn_counter_t;
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
    map<flow_id, uint64_t> last_received_times;
    flow_stats_t flow_reorder_stats;
    node_counter_t flits_per_queue;
    cxn_counter_t num_ingresses;
    cxn_counter_t num_egresses;
    cxn_counter_t num_virtual_queues;
    cxn_counter_t virtual_queue_depths;
    cxn_counter_t ingress_bw2xbar;
    cxn_counter_t egress_bandwidth;
    vq_counter_t vqwr_flits; 
    uint64_t total_vqwr_flits; 
    vq_node_counter_t vqwr_bridge;
    vq_node_counter_t vqwr_port;
    vq_counter_t vqrd_flits; 
    uint64_t total_vqrd_flits; 
    vq_node_counter_t vqrd_bridge;
    vq_node_counter_t vqrd_port;

private:
    typedef struct {
        char v_offered;
        char v_sent;
        char v_received;
    } vcd_flow_hooks_t;
    typedef map<flow_id, vcd_flow_hooks_t> vcd_flows_t;
    vcd_flows_t vcd_flows;
};


class aux_statistics {
public:
    aux_statistics(const uint64_t &system_time);
    virtual ~aux_statistics();

    friend ostream &operator<<(ostream &, const system_statistics &);
#ifdef PROGRESSIVE_STATISTICS_REPORT
    friend class system_statistics;
#endif

protected:
    virtual void print_stats(ostream &out) = 0;

    const uint64_t &system_time;
};


class system_statistics {
public:
    explicit system_statistics();
    uint64_t get_received_packet_count() const;
    void add(uint32_t id, std::shared_ptr<tile_statistics> new_stats);
    void reset();
    void start_sim();
    void end_sim();
    inline uint64_t  get_stats_time(uint32_t index) const;
    std::shared_ptr<tile_statistics> get_tile_stats(uint32_t index) const;
    inline void add_aux_statistics(std::shared_ptr<aux_statistics> stats) { aux_stats.push_back(stats); }
public:
    friend ostream &operator<<(ostream &, const system_statistics &);

#ifdef PROGRESSIVE_STATISTICS_REPORT
    void print_exec_statistics(ostream &);
#endif

private:
    typedef map<uint32_t, std::shared_ptr<tile_statistics> > tile_stats_t;
    typedef vector<std::shared_ptr<aux_statistics> > aux_stats_t;

    tile_stats_t tile_stats;
    aux_stats_t aux_stats;
};

inline uint64_t tile_statistics::get_system_time() const {
       return system_time; }

inline uint64_t tile_statistics::get_start_time() const {
       return start_time; }

inline uint64_t system_statistics::get_stats_time(uint32_t index) const  {
    tile_stats_t::const_iterator ti = tile_stats.find(index);
    assert(ti != tile_stats.end());
    const std::shared_ptr<tile_statistics> ts = ti->second;
    return ts->get_system_time() - ts->get_start_time();
}

inline std::shared_ptr<tile_statistics> system_statistics::get_tile_stats(uint32_t index) 
   const {
      tile_stats_t::const_iterator ti = tile_stats.find(index);
      assert(ti != tile_stats.end());
      const std::shared_ptr<tile_statistics> ts = ti->second;
    return ts;
}    

ostream &operator<<(ostream &, const system_statistics &);
#endif // __STATISTICS_HPP__
