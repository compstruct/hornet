// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMTRACE_THREAD_STATISTICS_HPP__
#define __MEMTRACE_THREAD_STATISTICS_HPP__

#include "threadStats.hpp"

class memtraceThreadStatsPerThread : public threadStatsPerThread {
public:
    memtraceThreadStatsPerThread(uint32_t id, const uint64_t &system_time);
    virtual ~memtraceThreadStatsPerThread();

    /* add memtraceThread-specific statistics */

    friend class memtraceThreadStats;
};

class memtraceThreadStats : public threadStats {
public:
    memtraceThreadStats(const uint64_t &system_time);
    virtual ~memtraceThreadStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
