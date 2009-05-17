// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include <map>
#include <map>
#include <iostream>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "cstdint.hpp"
#include "error.hpp"
#include "flow_id.hpp"
#include "flit.hpp"
#include "egress_id.hpp"

using namespace std;
using namespace boost;
using namespace boost::posix_time;

class statistics {
public:
    statistics(const uint64_t &system_time, const uint64_t &start_time) throw();
    void start_sim() throw();
    void end_sim() throw();
    void send_flit(const flow_id &, const flit &) throw();
    void receive_flit(const flow_id &, const flit &) throw();
    void register_links(const egress_id &src, const egress_id &dst,
                        unsigned num_links) throw();
    void register_flow_rename(const flow_id &from,
                              const flow_id &to) throw(err);
    void switch_links(const egress_id &src, const egress_id &dst,
                      unsigned min_link, unsigned num_links) throw();
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
    typedef tuple<egress_id, egress_id, unsigned> link_id;
    typedef map<link_id, uint64_t> link_switch_counter_t;
    typedef map<uint64_t, uint64_t> flit_timestamp_t;
    typedef map<flow_id, tuple<double, double> > flit_inc_stats_t;
    flow_renames_t original_flows;
    flit_counter_t sent_flits;
    uint64_t total_sent_flits;
    flit_counter_t received_flits;
    uint64_t total_received_flits;
    flit_timestamp_t flit_departures;
    flit_inc_stats_t flit_inc_stats;
    tuple<double,double> total_inc_stats;
    link_switch_counter_t link_switches;
};

ostream &operator<<(ostream &, const statistics &);

#endif // __STATISTICS_HPP__
