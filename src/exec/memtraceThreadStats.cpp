// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceThreadStats.hpp"

memtraceThreadStatsPerThread::memtraceThreadStatsPerThread(uint32_t id, const uint64_t &t) :
    threadStatsPerThread(id, t) { }

memtraceThreadStatsPerThread::~memtraceThreadStatsPerThread() {}

memtraceThreadStats::memtraceThreadStats(const uint64_t &t) : threadStats(t) {}

memtraceThreadStats::~memtraceThreadStats() {}

void memtraceThreadStats::print_stats(ostream &out) {

    threadStats::print_stats(out);

    /* add memtraceThread-specific statistics */

}
