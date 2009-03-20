// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include <map>
#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "cstdint.hpp"
#include "flow_id.hpp"

using namespace std;
using namespace boost::posix_time;

class statistics {
public:
    statistics(const uint64_t &system_time, const uint64_t &start_time) throw();
    void start_sim() throw();
    void end_sim() throw();
    void send_flit(const flow_id &) throw();
    void receive_flit(const flow_id &) throw();
    friend ostream &operator<<(ostream &, statistics &);
private:
    const uint64_t &system_time;
    const uint64_t start_time;
    ptime sim_start_time;
    ptime sim_end_time;
    typedef map<flow_id, uint64_t> flit_counter_t;
    flit_counter_t sent_flits;
    flit_counter_t received_flits;
};

ostream &operator<<(ostream &, const statistics &);

#endif // __STATISTICS_HPP__

