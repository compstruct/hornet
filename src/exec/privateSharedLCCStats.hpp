// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_LCC_STATS_HPP__
#define __PRIVATE_SHARED_LCC_STATS_HPP__

#include "memStats.hpp"
#include "memory_types.hpp"

class privateSharedLCCStatsPerTile : public memStatsPerTile {
public:
    privateSharedLCCStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~privateSharedLCCStatsPerTile();

    inline void record_ideal_timestamp() { m_record_ideal_timestamp_delta = true; }

    void did_read_l1(bool hit);
    void did_read_l2(bool hit);
    void did_write_l2(bool hit, uint64_t blocked_cycles);
    void could_not_evict_l2();
    void record_ideal_timestamp_delta(maddr_t maddr, uint64_t delta);

    void did_read_cat(bool hit);

    friend class privateSharedLCCStats;

private:

    running_stats m_l1_read_hits;
    running_stats m_l2_read_hits;
    running_stats m_l2_write_hits;

    running_stats m_write_block_cycles;
    uint64_t m_evict_block_cycles;

    running_stats m_cat_hits;

    bool m_record_ideal_timestamp_delta;
    map<maddr_t, running_stats> m_ideal_delta;
    map<maddr_t, vector<uint64_t> > m_ideal_delta_samples;

};

class privateSharedLCCStats : public memStats {
public:
    privateSharedLCCStats(const uint64_t &system_time);
    virtual ~privateSharedLCCStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
