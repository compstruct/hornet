// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEM_STATS_HPP__
#define __MEM_STATS_HPP__

#include "statistics.hpp"

class memStatsPerTile {
public:
    memStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~memStatsPerTile();

    inline void did_finish_read(uint64_t latency) { m_read_latencies.add(latency, 1); }
    inline void did_finish_write(uint64_t latency) { m_write_latencies.add(latency, 1); }

    friend class memStats;

private:
    uint32_t m_id;
    const uint64_t &system_time;

    running_stats m_read_latencies;
    running_stats m_write_latencies;

};

class memStats : public aux_statistics {
public:
    memStats(const uint64_t &system_time);
    virtual ~memStats();

    void add_per_tile_stats(shared_ptr<memStatsPerTile> stats);

protected:
    virtual void print_stats(ostream &out);

    typedef map<uint32_t/*tile id*/, shared_ptr<memStatsPerTile> > perTileStats_t;

    perTileStats_t m_per_tile_stats;

};


#endif
