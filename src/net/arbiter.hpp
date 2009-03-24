// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __ARBITER_HPP__
#define __ARBITER_HPP__

#include <queue>
#include "node.hpp"

typedef enum {
    AS_NONE = 0,
    AS_DUMB = 1,
    NUM_AS
} arbitration_t;

class arbiter {
public:
    arbiter(uint64_t &time, shared_ptr<node> src, shared_ptr<node> dst,
            arbitration_t scheme, unsigned min_bw, unsigned period,
            unsigned delay, logger &log) throw(err);
    void tick_positive_edge() throw(err);
    void tick_negative_edge() throw(err);
private:
    uint64_t &system_time;
    arbitration_t scheme;
    unsigned min_bw;    // guarantee minimum bandwidth in each direction
    unsigned period;    // run arbitration every sample_period ticks
    unsigned delay;     // delay between decision and enforcement
    uint64_t next_arb;  // next arbitration tick
    queue<tuple<uint64_t, unsigned, unsigned> > arb_queue;
    shared_ptr<egress> src_to_dst;
    shared_ptr<egress> dst_to_src;
    unsigned num_dst_queues;
    unsigned num_src_queues;
    unsigned last_queued_src_to_dst_bw;
    logger &log;
private:
    arbiter();                // not implemented
    arbiter(const arbiter &); // not implemented
};

#endif // __ARBITER_HPP__
