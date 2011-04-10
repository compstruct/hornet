// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_MSI_STATS_HPP__
#define __PRIVATE_SHARED_MSI_STATS_HPP__

#include "memStats.hpp"

class privateSharedMSIStatsPerTile : public memStatsPerTile {
public:
    privateSharedMSIStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~privateSharedMSIStatsPerTile();

    void did_read_l1(bool hit);
    void did_write_l1(bool hit);
    void did_read_l2(bool hit);
    void did_write_l2(bool hit);

    /* only for read sharer invalidation */
    void did_invalidate_sharers(uint32_t num_sharers, uint64_t latency);
    void did_read_cat(bool hit);

    friend class privateSharedMSIStats;

private:

    running_stats m_l1_read_hits;
    running_stats m_l1_write_hits;
    running_stats m_l2_read_hits;
    running_stats m_l2_write_hits;

    running_stats m_invalidated_sharers;
    running_stats m_invalidate_penalties;

    running_stats m_directory_hits;

};

class privateSharedMSIStats : public memStats {
public:
    privateSharedMSIStats(const uint64_t &system_time);
    virtual ~privateSharedMSIStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
