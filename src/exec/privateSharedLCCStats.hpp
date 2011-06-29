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

    void did_read_l1(bool hit);
    void did_read_l2(bool hit);
    void did_write_l2(bool hit);
    void did_read_cat(bool hit);

    /* cost breakdown study */
    inline void add_memory_subsystem_serialization_cost(uint64_t cost) { m_memory_subsystem_serialization_cost += cost; }
    inline void add_cat_serialization_cost(uint64_t cost) { m_cat_serialization_cost += cost; }
    inline void add_cat_action_cost(uint64_t cost) { m_cat_action_cost += cost; }
    inline void add_l1_serialization_cost(uint64_t cost) { m_l1_serialization_cost += cost; }
    inline void add_l1_action_cost(uint64_t cost) { m_l1_action_cost += cost; }
    inline void add_l2_network_plus_serialization_cost(uint64_t cost) { m_l2_network_plus_serialization_cost += cost; }
    inline void add_l2_action_cost(uint64_t cost) { m_l2_action_cost += cost; }
    inline void add_l2_write_block_cost(uint64_t cost) { m_l2_write_block_cost += cost; }
    inline void add_l2_eviction_cost(uint64_t cost) { m_l2_eviction_cost += cost; }
    inline void add_dram_network_plus_serialization_cost(uint64_t cost) { m_dram_network_plus_serialization_cost += cost; }
    inline void add_dram_offchip_network_plus_dram_action_cost(uint64_t cost) { m_dram_offchip_network_plus_dram_action_cost += cost; }
    inline void add_l1_action() { ++m_l1_action; }
    inline void add_l2_action() { ++m_l2_action; }

    friend class privateSharedLCCStats;

private:

    running_stats m_l1_read_hits;
    running_stats m_l2_read_hits;
    running_stats m_l2_write_hits;

    running_stats m_cat_hits;

    map<maddr_t, running_stats> m_ideal_delta;
    map<maddr_t, vector<uint64_t> > m_ideal_delta_samples;

    /* cost breakdown study */
    uint64_t m_memory_subsystem_serialization_cost;
    uint64_t m_cat_serialization_cost;
    uint64_t m_cat_action_cost;
    uint64_t m_l1_serialization_cost;
    uint64_t m_l1_action_cost;
    uint64_t m_l2_network_plus_serialization_cost;
    uint64_t m_l2_action_cost;
    uint64_t m_l2_write_block_cost;
    uint64_t m_l2_eviction_cost;
    uint64_t m_dram_network_plus_serialization_cost;
    uint64_t m_dram_offchip_network_plus_dram_action_cost;
    uint64_t m_l1_action;
    uint64_t m_l2_action;

};

class privateSharedLCCStats : public memStats {
public:
    privateSharedLCCStats(const uint64_t &system_time);
    virtual ~privateSharedLCCStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
