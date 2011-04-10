// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_MSI_STATS_HPP__
#define __PRIVATE_SHARED_MSI_STATS_HPP__

#include "memStats.hpp"

class privateSharedMSIStatsPerTile : public memStatsPerTile {
public:
    privateSharedMSIStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~privateSharedMSIStatsPerTile();

    /* add privateSharedMSI-specific statistics */

    friend class privateSharedMSIStats;
};

class privateSharedMSIStats : public memStats {
public:
    privateSharedMSIStats(const uint64_t &system_time);
    virtual ~privateSharedMSIStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
