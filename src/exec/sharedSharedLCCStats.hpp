// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SHARED_SHARED_LCC_STATS_HPP__
#define __SHARED_SHARED_LCC_STATS_HPP__

#include "memStats.hpp"
#include "memory_types.hpp"

class sharedSharedLCCStatsPerMemInstr {
public:
    sharedSharedLCCStatsPerMemInstr(bool is_read);
    ~sharedSharedLCCStatsPerMemInstr();

    uint64_t total_cost();
    void add(const sharedSharedLCCStatsPerMemInstr& other);

    std::shared_ptr<sharedSharedLCCStatsPerMemInstr> get_tentative_data(int index);
    int get_max_tentative_data_index();
    inline void clear_tentative_data() { m_tentative_data.clear(); }
    void discard_tentative_data(int index);
    void commit_tentative_data(int index);
    void commit_max_tentative_data();
    void commit_min_tentative_data();

    inline void migration_started(uint64_t depart_time) { m_in_migration = true; m_mig_depart_time = depart_time; }
    void migration_finished(uint64_t arrival_time, bool write_stats);
    inline bool is_in_migration() { return m_in_migration; }

    inline void set_serialization_begin_time_at_current_core(uint64_t begin_time) { m_serialization_begin_time = begin_time; }
    inline uint64_t serialization_begin_time_at_current_core() { return m_serialization_begin_time; }

    inline void core_missed() { m_did_core_miss = true; }

    inline void add_mem_srz(uint64_t amnt) { m_mem_srz += amnt; }
    inline void add_cat_srz(uint64_t amnt) { m_cat_srz += amnt; }
    inline void add_cat_ops(uint64_t amnt) { m_cat_ops += amnt; }
    inline void add_l1_srz(uint64_t amnt) { m_l1_srz += amnt; }
    inline void add_l1_ops(uint64_t amnt) { m_l1_ops += amnt; }
    inline void add_l1_blk_by_checkin(uint64_t amnt) { m_l1_blk_by_checkin += amnt; }
    inline void add_l1_blk_by_ts(uint64_t amnt) { m_l1_blk_by_ts += amnt; }
    inline void add_remote_req_nas(uint64_t amnt) { m_remote_req_nas += amnt; }
    inline void add_remote_rep_nas(uint64_t amnt) { m_remote_rep_nas += amnt; }
    inline void add_remote_checkin_nas(uint64_t amnt) { m_remote_checkin_nas += amnt; }
    inline void add_l2_srz(uint64_t amnt) { m_l2_srz += amnt; }
    inline void add_l2_ops(uint64_t amnt) { m_l2_ops += amnt; }
    inline void add_l2_blk_by_checkin(uint64_t amnt) { m_l2_blk_by_checkin += amnt; }
    inline void add_l2_blk_by_ts(uint64_t amnt) { m_l2_blk_by_ts += amnt; }
    inline void add_dramctrl_req_nas(uint64_t amnt) { m_dramctrl_req_nas += amnt; }
    inline void add_dramctrl_rep_nas(uint64_t amnt) { m_dramctrl_rep_nas += amnt; }
    inline void add_dram_ops(uint64_t amnt) { m_dram_ops += amnt; }
    inline void add_bypass(uint64_t amnt) { m_bypass += amnt; }

    /* L1 cost categories */
    inline void add_local_l1_cost_for_hit(uint64_t amnt) { m_local_l1_cost_for_hit += amnt; }
    inline void add_local_l1_cost_for_miss(uint64_t amnt) { m_local_l1_cost_for_miss += amnt; }
    inline void add_local_l1_cost_for_update(uint64_t amnt) { m_local_l1_cost_for_update += amnt; }
    inline void add_remote_l1_cost_for_hit(uint64_t amnt) { m_remote_l1_cost_for_hit += amnt; }
    inline void add_remote_l1_cost_for_miss(uint64_t amnt) { m_remote_l1_cost_for_miss += amnt; }
    inline void add_remote_l1_cost_for_update(uint64_t amnt) { m_remote_l1_cost_for_update += amnt; }
    inline void add_local_l1_blk_by_checkin(uint64_t amnt) { m_local_l1_blk_by_checkin += amnt; }
    inline void add_local_l1_blk_by_ts(uint64_t amnt) { m_local_l1_blk_by_ts += amnt; }
    inline void add_local_bypass(uint64_t amnt) { m_local_bypass += amnt; }
    inline void add_remote_l1_blk_by_checkin(uint64_t amnt) { m_remote_l1_blk_by_checkin += amnt; }
    inline void add_remote_l1_blk_by_ts(uint64_t amnt) { m_remote_l1_blk_by_ts += amnt; }
    inline void add_remote_bypass(uint64_t amnt) { m_remote_bypass += amnt; }

    /* L2 cost categories */
    inline void add_local_l2_cost_for_hit(uint64_t amnt) { m_local_l2_cost_for_hit += amnt; }
    inline void add_local_l2_cost_for_miss(uint64_t amnt) { m_local_l2_cost_for_miss += amnt; }
    inline void add_local_l2_cost_for_update(uint64_t amnt) { m_local_l2_cost_for_update += amnt; }
    inline void add_local_l2_cost_for_writeback(uint64_t amnt) { m_local_l2_cost_for_writeback += amnt; }
    inline void add_local_l2_blk_by_checkin(uint64_t amnt) { m_local_l2_blk_by_checkin += amnt; }
    inline void add_local_l2_blk_by_ts(uint64_t amnt) { m_local_l2_blk_by_ts += amnt; }
    inline void add_remote_l2_cost_for_hit(uint64_t amnt) { m_remote_l2_cost_for_hit += amnt; }
    inline void add_remote_l2_cost_for_miss(uint64_t amnt) { m_remote_l2_cost_for_miss += amnt; }
    inline void add_remote_l2_cost_for_update(uint64_t amnt) { m_remote_l2_cost_for_update += amnt; }
    inline void add_remote_l2_cost_for_writeback(uint64_t amnt) { m_remote_l2_cost_for_writeback += amnt; }
    inline void add_remote_l2_blk_by_checkin(uint64_t amnt) { m_remote_l2_blk_by_checkin += amnt; }
    inline void add_remote_l2_blk_by_ts(uint64_t amnt) { m_remote_l2_blk_by_ts += amnt; }

    friend class sharedSharedLCCStatsPerTile;
    friend class sharedSharedLCCStats;

private:

    bool add_new_tentative_data(int index);

    bool m_is_read;
    bool m_did_core_miss;
    uint64_t m_serialization_begin_time;
    bool m_in_migration;
    uint64_t m_mig_depart_time;

    uint64_t m_mem_srz;
    uint64_t m_l1_srz;
    uint64_t m_l1_ops;
    uint64_t m_l1_blk_by_checkin;
    uint64_t m_l1_blk_by_ts;
    uint64_t m_cat_srz;
    uint64_t m_cat_ops;
    uint64_t m_remote_req_nas;
    uint64_t m_remote_rep_nas;
    uint64_t m_remote_checkin_nas;
    uint64_t m_l2_srz;
    uint64_t m_l2_ops;
    uint64_t m_l2_blk_by_checkin;
    uint64_t m_l2_blk_by_ts;
    uint64_t m_dramctrl_req_nas;
    uint64_t m_dramctrl_rep_nas;
    uint64_t m_dram_ops;
    uint64_t m_bypass;
    uint64_t m_mig;

    uint64_t m_local_l1_cost_for_hit;
    uint64_t m_local_l1_cost_for_miss;
    uint64_t m_local_l1_cost_for_update;
    uint64_t m_local_l1_blk_by_checkin;
    uint64_t m_local_l1_blk_by_ts;
    uint64_t m_local_bypass;
    uint64_t m_remote_l1_cost_for_hit;
    uint64_t m_remote_l1_cost_for_miss;
    uint64_t m_remote_l1_cost_for_update;
    uint64_t m_remote_l1_blk_by_checkin;
    uint64_t m_remote_l1_blk_by_ts;
    uint64_t m_remote_bypass;

    uint64_t m_local_l2_cost_for_hit;
    uint64_t m_local_l2_cost_for_miss;
    uint64_t m_local_l2_cost_for_update;
    uint64_t m_local_l2_cost_for_writeback;
    uint64_t m_local_l2_blk_by_checkin;
    uint64_t m_local_l2_blk_by_ts;
    uint64_t m_remote_l2_cost_for_hit;
    uint64_t m_remote_l2_cost_for_miss;
    uint64_t m_remote_l2_cost_for_update;
    uint64_t m_remote_l2_cost_for_writeback;
    uint64_t m_remote_l2_blk_by_checkin;
    uint64_t m_remote_l2_blk_by_ts;

    map<int, std::shared_ptr<sharedSharedLCCStatsPerMemInstr> > m_tentative_data;
    
};

class sharedSharedLCCStatsPerTile : public memStatsPerTile {
public:
    sharedSharedLCCStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~sharedSharedLCCStatsPerTile();

    /* L1 */
    inline void new_read_instr_at_l1() { ++m_num_l1_read_instr; }
    inline void new_write_instr_at_l1() { ++m_num_l1_write_instr; }
    /* L1 home data */
    inline void hit_for_read_instr_at_home_l1() { ++m_num_hits_for_read_instr_at_home_l1; }
    inline void hit_for_write_instr_at_home_l1() { ++m_num_hits_for_write_instr_at_home_l1; }
    inline void true_miss_for_read_instr_at_home_l1() { ++m_num_true_misses_for_read_instr_at_home_l1; }
    inline void true_miss_for_write_instr_at_home_l1() { ++m_num_true_misses_for_write_instr_at_home_l1; }
    inline void ts_blocked_miss_for_write_instr_at_home_l1() { ++m_num_ts_blocked_miss_for_write_instr_at_home_l1; }
    inline void checkin_blocked_miss_for_read_instr_at_home_l1() { ++m_num_checkin_blocked_miss_for_read_instr_at_home_l1; }
    inline void checkin_blocked_miss_for_write_instr_at_home_l1() { ++m_num_checkin_blocked_miss_for_write_instr_at_home_l1; }
    /* L1 away data */
    inline void hit_for_read_instr_at_away_l1() { ++m_num_hits_for_read_instr_at_away_l1; }
    inline void hit_for_write_instr_at_away_l1() { ++m_num_hits_for_write_instr_at_away_l1; }
    inline void true_miss_for_read_instr_at_away_l1() { ++m_num_true_misses_for_read_instr_at_away_l1; }
    inline void true_miss_for_write_instr_at_away_l1() { ++m_num_true_misses_for_write_instr_at_away_l1; }
    inline void ts_expired_miss_for_read_instr_at_away_l1() { ++m_num_ts_expired_miss_for_read_instr_at_away_l1; }
    inline void permission_miss_ts_expired_for_write_instr_at_away_l1() { 
        ++m_num_permission_miss_ts_expired_for_write_instr_at_away_l1; 
    }
    inline void permission_miss_ts_unexpired_for_write_instr_at_away_l1() { 
        ++m_num_permission_miss_ts_unexpired_for_write_instr_at_away_l1; 
    }
    /* 1st-2nd categories */
    inline void hit_for_read_instr_at_local_l1() { ++m_num_hits_for_read_instr_at_local_l1; }
    inline void hit_for_write_instr_at_local_l1() { ++m_num_hits_for_write_instr_at_local_l1; }
    inline void true_miss_for_read_instr_at_local_l1() { ++m_num_true_misses_for_read_instr_at_local_l1; }
    inline void true_miss_for_write_instr_at_local_l1() { ++m_num_true_misses_for_write_instr_at_local_l1; }
    inline void ts_blocked_miss_for_write_instr_at_local_l1() { ++m_num_ts_blocked_miss_for_write_instr_at_local_l1; }
    inline void checkin_blocked_miss_for_read_instr_at_local_l1() { ++m_num_checkin_blocked_miss_for_read_instr_at_local_l1; }
    inline void checkin_blocked_miss_for_write_instr_at_local_l1() { ++m_num_checkin_blocked_miss_for_write_instr_at_local_l1; }
    inline void ts_expired_miss_for_read_instr_at_local_l1() { ++m_num_ts_expired_miss_for_read_instr_at_local_l1; }
    inline void permission_miss_ts_expired_for_write_instr_at_local_l1() { 
        ++m_num_permission_miss_ts_expired_for_write_instr_at_local_l1; 
    }
    inline void permission_miss_ts_unexpired_for_write_instr_at_local_l1() { 
        ++m_num_permission_miss_ts_unexpired_for_write_instr_at_local_l1; 
    }
    inline void hit_for_read_instr_at_remote_l1() { ++m_num_hits_for_read_instr_at_remote_l1; }
    inline void hit_for_write_instr_at_remote_l1() { ++m_num_hits_for_write_instr_at_remote_l1; }
    inline void true_miss_for_read_instr_at_remote_l1() { ++m_num_true_misses_for_read_instr_at_remote_l1; }
    inline void true_miss_for_write_instr_at_remote_l1() { ++m_num_true_misses_for_write_instr_at_remote_l1; }
    inline void ts_blocked_miss_for_write_instr_at_remote_l1() { ++m_num_ts_blocked_miss_for_write_instr_at_remote_l1; }
    inline void checkin_blocked_miss_for_read_instr_at_remote_l1() { ++m_num_checkin_blocked_miss_for_read_instr_at_remote_l1; }
    inline void checkin_blocked_miss_for_write_instr_at_remote_l1() { ++m_num_checkin_blocked_miss_for_write_instr_at_remote_l1; }
    inline void hit_for_read_instr_at_local_l2() { ++m_num_hits_for_local_l2_read_instr; }
    inline void hit_for_write_instr_at_local_l2() { ++m_num_hits_for_local_l2_write_instr; }
    inline void true_miss_for_read_instr_at_local_l2() { ++m_num_true_misses_for_read_instr_at_local_l2; }
    inline void true_miss_for_write_instr_at_local_l2() { ++m_num_true_misses_for_write_instr_at_local_l2; }
    inline void ts_blocked_miss_for_write_instr_at_local_l2() { ++m_num_ts_blocked_miss_for_write_instr_at_local_l2; }
    inline void checkin_blocked_miss_for_read_instr_at_local_l2() { ++m_num_checkin_blocked_miss_for_read_instr_at_local_l2; }
    inline void checkin_blocked_miss_for_write_instr_at_local_l2() { ++m_num_checkin_blocked_miss_for_write_instr_at_local_l2; }
    inline void hit_for_read_instr_at_remote_l2() { ++m_num_hits_for_remote_l2_read_instr; }
    inline void hit_for_write_instr_at_remote_l2() { ++m_num_hits_for_remote_l2_write_instr; }
    inline void true_miss_for_read_instr_at_remote_l2() { ++m_num_true_misses_for_read_instr_at_remote_l2; }
    inline void true_miss_for_write_instr_at_remote_l2() { ++m_num_true_misses_for_write_instr_at_remote_l2; }
    inline void ts_blocked_miss_for_write_instr_at_remote_l2() { ++m_num_ts_blocked_miss_for_write_instr_at_remote_l2; }
    inline void checkin_blocked_miss_for_read_instr_at_remote_l2() { ++m_num_checkin_blocked_miss_for_read_instr_at_remote_l2; }
    inline void checkin_blocked_miss_for_write_instr_at_remote_l2() { ++m_num_checkin_blocked_miss_for_write_instr_at_remote_l2; }

    /* L2 */
    inline void new_read_instr_at_l2() { ++m_num_l2_read_instr; }
    inline void new_write_instr_at_l2() { ++m_num_l2_write_instr; }
    inline void hit_for_read_instr_at_l2() { ++m_num_hits_for_l2_read_instr; }
    inline void hit_for_write_instr_at_l2() { ++m_num_hits_for_l2_write_instr; }
    inline void true_miss_for_read_instr_at_l2() { ++m_num_true_misses_for_read_instr_at_l2; }
    inline void true_miss_for_write_instr_at_l2() { ++m_num_true_misses_for_write_instr_at_l2; }
    inline void ts_blocked_miss_for_write_instr_at_l2() { ++m_num_ts_blocked_miss_for_write_instr_at_l2; }
    inline void checkin_blocked_miss_for_read_instr_at_l2() { ++m_num_checkin_blocked_miss_for_read_instr_at_l2; }
    inline void checkin_blocked_miss_for_write_instr_at_l2() { ++m_num_checkin_blocked_miss_for_write_instr_at_l2; }

    /* Bypass Hits */
    inline void bypass_for_read_instr(bool local) { 
        ++m_num_bypass_for_read_instr; 
        if (local) {
            ++m_num_bypass_for_local_read_instr;
        } else {
            ++m_num_bypass_for_remote_read_instr;
        }
    }

    /* others */
    inline void remote_checkin_uncacheable_sent() { ++m_remote_checkin_uncacheable_sent; }
    inline void remote_checkin_expired_sent() { ++m_remote_checkin_expired_sent; }
    inline void remote_checkin_evicted_sent() { ++m_remote_checkin_evicted_sent; }
    inline void checkin_hit_at_l1() { ++m_checkin_hit_at_l1; }
    inline void checkin_hit_at_l2() { ++m_checkin_hit_at_l2; }

    /* evictions */
    inline void evict_at_l1() { ++m_num_evict_at_l1; }
    inline void evict_at_l2() { ++m_num_evict_at_l2; }
    inline void writeback_at_l1() { ++m_num_writeback_at_l1; }
    inline void writeback_at_l2() { ++m_num_writeback_at_l2; }

    /* hardware */
    inline void add_cat_action() { ++m_num_cat_action; }
    inline void add_l1_action() { ++m_num_l1_action; }
    inline void add_l2_action() { ++m_num_l2_action; }
    inline void add_dram_action() { ++m_num_dram_action; }

    void add(const sharedSharedLCCStatsPerTile& other);
    void commit_per_mem_instr_stats(const sharedSharedLCCStatsPerMemInstr& data);
    inline void commit_per_mem_instr_stats(std::shared_ptr<sharedSharedLCCStatsPerMemInstr> data) { commit_per_mem_instr_stats(*data); }

    friend class sharedSharedLCCStats;

private:
    uint64_t m_num_l1_read_instr; 
    uint64_t m_num_l1_write_instr; 
    uint64_t m_num_hits_for_read_instr_at_home_l1; 
    uint64_t m_num_hits_for_write_instr_at_home_l1; 
    uint64_t m_num_true_misses_for_read_instr_at_home_l1; 
    uint64_t m_num_true_misses_for_write_instr_at_home_l1; 
    uint64_t m_num_ts_blocked_miss_for_write_instr_at_home_l1; 
    uint64_t m_num_checkin_blocked_miss_for_read_instr_at_home_l1; 
    uint64_t m_num_checkin_blocked_miss_for_write_instr_at_home_l1; 
    uint64_t m_num_hits_for_read_instr_at_away_l1; 
    uint64_t m_num_hits_for_write_instr_at_away_l1; 
    uint64_t m_num_true_misses_for_read_instr_at_away_l1; 
    uint64_t m_num_true_misses_for_write_instr_at_away_l1; 
    uint64_t m_num_ts_expired_miss_for_read_instr_at_away_l1; 
    uint64_t m_num_permission_miss_ts_expired_for_write_instr_at_away_l1; 
    uint64_t m_num_permission_miss_ts_unexpired_for_write_instr_at_away_l1; 

    uint64_t m_num_hits_for_read_instr_at_local_l1; 
    uint64_t m_num_hits_for_write_instr_at_local_l1; 
    uint64_t m_num_true_misses_for_read_instr_at_local_l1; 
    uint64_t m_num_true_misses_for_write_instr_at_local_l1; 
    uint64_t m_num_ts_blocked_miss_for_write_instr_at_local_l1; 
    uint64_t m_num_checkin_blocked_miss_for_read_instr_at_local_l1; 
    uint64_t m_num_checkin_blocked_miss_for_write_instr_at_local_l1; 
    uint64_t m_num_ts_expired_miss_for_read_instr_at_local_l1; 
    uint64_t m_num_permission_miss_ts_expired_for_write_instr_at_local_l1; 
    uint64_t m_num_permission_miss_ts_unexpired_for_write_instr_at_local_l1; 
    uint64_t m_num_hits_for_read_instr_at_remote_l1; 
    uint64_t m_num_hits_for_write_instr_at_remote_l1; 
    uint64_t m_num_true_misses_for_read_instr_at_remote_l1; 
    uint64_t m_num_true_misses_for_write_instr_at_remote_l1; 
    uint64_t m_num_ts_blocked_miss_for_write_instr_at_remote_l1; 
    uint64_t m_num_checkin_blocked_miss_for_read_instr_at_remote_l1; 
    uint64_t m_num_checkin_blocked_miss_for_write_instr_at_remote_l1; 
    uint64_t m_num_hits_for_local_l2_read_instr;
    uint64_t m_num_hits_for_local_l2_write_instr;
    uint64_t m_num_true_misses_for_read_instr_at_local_l2;
    uint64_t m_num_true_misses_for_write_instr_at_local_l2;
    uint64_t m_num_ts_blocked_miss_for_write_instr_at_local_l2;
    uint64_t m_num_checkin_blocked_miss_for_read_instr_at_local_l2;
    uint64_t m_num_checkin_blocked_miss_for_write_instr_at_local_l2;
    uint64_t m_num_hits_for_remote_l2_read_instr;
    uint64_t m_num_hits_for_remote_l2_write_instr;
    uint64_t m_num_true_misses_for_read_instr_at_remote_l2;
    uint64_t m_num_true_misses_for_write_instr_at_remote_l2;
    uint64_t m_num_ts_blocked_miss_for_write_instr_at_remote_l2;
    uint64_t m_num_checkin_blocked_miss_for_read_instr_at_remote_l2;
    uint64_t m_num_checkin_blocked_miss_for_write_instr_at_remote_l2;

    uint64_t m_num_l2_read_instr; 
    uint64_t m_num_l2_write_instr; 
    uint64_t m_num_hits_for_l2_read_instr; 
    uint64_t m_num_hits_for_l2_write_instr; 
    uint64_t m_num_true_misses_for_read_instr_at_l2; 
    uint64_t m_num_true_misses_for_write_instr_at_l2; 
    uint64_t m_num_ts_blocked_miss_for_write_instr_at_l2; 
    uint64_t m_num_checkin_blocked_miss_for_read_instr_at_l2; 
    uint64_t m_num_checkin_blocked_miss_for_write_instr_at_l2; 

    uint64_t m_num_bypass_for_read_instr; 
    uint64_t m_num_bypass_for_local_read_instr; 
    uint64_t m_num_bypass_for_remote_read_instr; 

    uint64_t m_num_core_hit_read_instr;
    uint64_t m_num_core_hit_write_instr;
    uint64_t m_num_core_miss_read_instr;
    uint64_t m_num_core_miss_write_instr;

    uint64_t m_remote_checkin_uncacheable_sent; 
    uint64_t m_remote_checkin_expired_sent; 
    uint64_t m_remote_checkin_evicted_sent; 
    uint64_t m_checkin_hit_at_l1; 
    uint64_t m_checkin_hit_at_l2; 

    uint64_t m_num_evict_at_l1; 
    uint64_t m_num_evict_at_l2; 
    uint64_t m_num_writeback_at_l1; 
    uint64_t m_num_writeback_at_l2; 

    uint64_t m_num_cat_action; 
    uint64_t m_num_l1_action; 
    uint64_t m_num_l2_action; 
    uint64_t m_num_dram_action; 

    sharedSharedLCCStatsPerMemInstr m_total_per_mem_instr_info;

};

class sharedSharedLCCStats : public memStats {
public:
    sharedSharedLCCStats(const uint64_t &system_time);
    virtual ~sharedSharedLCCStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
