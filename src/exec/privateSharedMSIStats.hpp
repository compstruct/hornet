// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_MSI_STATS_HPP__
#define __PRIVATE_SHARED_MSI_STATS_HPP__

#include "memStats.hpp"

class privateSharedMSIStatsPerTile : public memStatsPerTile {
public:
    privateSharedMSIStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~privateSharedMSIStatsPerTile();

    void did_read_cat(bool hit);
    void did_read_l1(bool hit);
    void did_write_l1(bool hit);
    void did_access_l2(bool hit);

    void did_invalidate_caches(uint32_t num_caches, uint64_t latency);

    /* cost breakdown study - outstanding costs only */
    /* cost means something that causes an increase in the latency for a memory request */
    /* if a latency of a certain action is hidden and not seen by any memory request, it's not a cost */
    /* all costs except for serialization costs are measured only on single memory request that directly decides actions */
    /* if an action (i.e. an invalidation) may affect other memory requests, those requests will see this effect only on */
    /* serialization costs */
    inline void add_memory_subsystem_serialization_cost(uint64_t cost) { m_memory_subsystem_serialization_cost += cost; }
    inline void add_cat_serialization_cost(uint64_t cost) { m_cat_serialization_cost += cost; }
    inline void add_cat_action_cost(uint64_t cost) { m_cat_action_cost += cost; }
    inline void add_l1_serialization_cost(uint64_t cost) { m_l1_serialization_cost += cost; }
    inline void add_l1_action_cost(uint64_t cost) { m_l1_action_cost += cost; }
    inline void add_l1_eviction_cost(uint64_t cost) { m_l1_eviction_cost += cost; }
    inline void add_l2_network_plus_serialization_cost(uint64_t cost) { m_l2_network_plus_serialization_cost += cost; }
    inline void add_l2_action_cost(uint64_t cost) { m_l2_action_cost += cost; }
    inline void add_l2_invalidation_cost(uint64_t cost) { m_l2_invalidation_cost += cost; }
    inline void add_l2_eviction_cost(uint64_t cost) { m_l2_eviction_cost += cost; }
    inline void add_dram_network_plus_serialization_cost(uint64_t cost) { m_dram_network_plus_serialization_cost += cost; }
    inline void add_dram_offchip_network_plus_dram_action_cost(uint64_t cost) { m_dram_offchip_network_plus_dram_action_cost += cost; }
    inline void add_l1_action() { ++m_l1_action; }
    inline void add_l2_action() { ++m_l2_action; }
    
    inline void add_i_s() { ++m_i_s; }
    inline void add_i_m() { ++m_i_m; }
    inline void add_s_s() { ++m_s_s; }
    inline void add_s_m() { ++m_s_m; }
    inline void add_s_m_invs() { ++m_s_m_invs; }
    inline void add_m_s() { ++m_m_s; }
    inline void add_m_s_invs() { ++m_m_s_invs; }
    inline void add_m_m() { ++m_m_m; }
    inline void add_s_i() { ++m_s_i; }
    inline void add_s_i_invs() { ++m_s_i_invs; }
    inline void add_m_i() { ++m_m_i; }
    inline void add_m_i_invs() { ++m_m_i_invs; }
    
    inline void add_shreq() { ++m_shreq; }
    inline void add_exreq() { ++m_exreq; }
    inline void add_invrep() { ++m_invrep; }
    inline void add_invrep_requested() { ++m_invrep_requested; }
    inline void add_flushrep() { ++m_flushrep; }
    inline void add_flushrep_requested() { ++m_flushrep_requested; }
    inline void add_wbrep() { ++m_wbrep; }
    inline void add_wbrep_requested() { ++m_wbrep_requested; }
    inline void add_shrep() { ++m_shrep; }
    inline void add_exrep() { ++m_exrep; }
    inline void add_invreq() { ++m_invreq; }
    inline void add_invreq_replied() { ++m_invreq_replied; }
    inline void add_flushreq() { ++m_flushreq; }
    inline void add_flushreq_replied() { ++m_flushreq_replied; }
    inline void add_wbreq() { ++m_wbreq; }
    inline void add_wbreq_replied() { ++m_wbreq_replied; }

    friend class privateSharedMSIStats;

private:

    running_stats m_l1_read_hits;
    running_stats m_l1_write_hits;
    running_stats m_l2_hits;

    running_stats m_invalidated_caches;
    running_stats m_invalidate_penalties;

    running_stats m_directory_hits;

    /* cost breakdown study */
    uint64_t m_memory_subsystem_serialization_cost;
    uint64_t m_cat_serialization_cost;
    uint64_t m_cat_action_cost;
    uint64_t m_l1_serialization_cost;
    uint64_t m_l1_action_cost;
    uint64_t m_l1_eviction_cost;
    uint64_t m_l2_network_plus_serialization_cost;
    uint64_t m_l2_action_cost;
    uint64_t m_l2_invalidation_cost;
    uint64_t m_l2_eviction_cost;
    uint64_t m_dram_network_plus_serialization_cost;
    uint64_t m_dram_offchip_network_plus_dram_action_cost;
    uint64_t m_l1_action;
    uint64_t m_l2_action;
    
    uint64_t m_i_s;
    uint64_t m_i_m;
    uint64_t m_s_s;
    uint64_t m_s_m;
    uint64_t m_s_m_invs;
    uint64_t m_m_s;
    uint64_t m_m_s_invs;
    uint64_t m_m_m;
    uint64_t m_s_i;
    uint64_t m_s_i_invs;
    uint64_t m_m_i;
    uint64_t m_m_i_invs;
    
    uint64_t m_shreq;
    uint64_t m_exreq;
    uint64_t m_invrep;
    uint64_t m_invrep_requested;
    uint64_t m_flushrep;
    uint64_t m_flushrep_requested;
    uint64_t m_wbrep;
    uint64_t m_wbrep_requested;
    uint64_t m_shrep;
    uint64_t m_exrep;
    uint64_t m_invreq;
    uint64_t m_invreq_replied;
    uint64_t m_flushreq;
    uint64_t m_flushreq_replied;
    uint64_t m_wbreq;
    uint64_t m_wbreq_replied;

};

class privateSharedMSIStats : public memStats {
public:
    privateSharedMSIStats(const uint64_t &system_time);
    virtual ~privateSharedMSIStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
