// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include <map>
#include <iostream>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "cstdint.hpp"
#include "flow_id.hpp"
#include "egress_id.hpp"

using namespace std;
using namespace boost;
using namespace boost::posix_time;

class statistics {
public:
    statistics(const uint64_t &system_time, const uint64_t &start_time) throw();
    void start_sim() throw();
    void end_sim() throw();
    void send_flit(const flow_id &) throw();
    void receive_flit(const flow_id &) throw();
    void register_links(const egress_id &src, const egress_id &dst,
                        unsigned num_links) throw();
    void switch_links(const egress_id &src, const egress_id &dst,
                      unsigned min_link, unsigned num_links) throw();
    friend ostream &operator<<(ostream &, statistics &);
private:
    const uint64_t &system_time;
    const uint64_t start_time;
    ptime sim_start_time;
    ptime sim_end_time;
    typedef map<flow_id, uint64_t> flit_counter_t;
    typedef tuple<egress_id, egress_id, unsigned> link_id;
    typedef map<link_id, uint64_t> link_switch_counter_t;
    flit_counter_t sent_flits;
    flit_counter_t received_flits;
    link_switch_counter_t link_switches;
};

ostream &operator<<(ostream &, const statistics &);

#endif // __STATISTICS_HPP__
