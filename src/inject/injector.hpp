// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __INJECTOR_HPP__
#define __INJECTOR_HPP__

#include <vector>
#include <map>
#include <boost/shared_ptr.hpp>
#include <boost/tuple/tuple.hpp>
#include "cstdint.hpp"
#include "flow_id.hpp"
#include "logger.hpp"
#include "pe.hpp"

using namespace std;
using namespace boost;

class injector : public pe {
public:
    injector(const pe_id &id, uint64_t &system_time, logger &log) throw(err);
    virtual ~injector() throw();
    virtual void connect(shared_ptr<bridge> net_bridge) throw(err);
    void add_event(const uint64_t &time, const flow_id &flow,
                   uint32_t len, const uint64_t &period) throw(err);
    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
private:
    typedef uint64_t tick_t;
    typedef uint32_t len_t;
    typedef uint64_t period_t;
    typedef vector<tuple<tick_t, flow_id, len_t, period_t> > events_t;
    typedef map<flow_id, tuple<tick_t, len_t, period_t> > flows_t;

    tick_t &system_time;
    shared_ptr<bridge> net;
    events_t events;
    events_t::iterator next_event;
    flows_t flows;
    vector<flow_id> flow_ids;
    vector<uint32_t> queue_ids;
    logger &log;
};

#endif // __INJECTOR_HPP__