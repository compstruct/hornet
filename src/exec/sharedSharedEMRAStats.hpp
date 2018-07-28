// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SHARED_SHARED_EMRA_STATS_HPP__
#define __SHARED_SHARED_EMRA_STATS_HPP__

#include "memStats.hpp"
#include "memory_types.hpp"

class sharedSharedEMRAStatsPerMemInstr {
public:
    sharedSharedEMRAStatsPerMemInstr(bool is_read);
    ~sharedSharedEMRAStatsPerMemInstr();

    uint64_t total_cost();
    void add(const sharedSharedEMRAStatsPerMemInstr& other);

    std::shared_ptr<sharedSharedEMRAStatsPerMemInstr> get_tentative_data(int index);
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
    inline void add_l1_srz(uint64_t amnt) { m_l1_srz += amnt; }
    inline void add_l1_ops(uint64_t amnt) { m_l1_ops += amnt; }
    inline void add_cat_srz(uint64_t amnt) { m_cat_srz += amnt; }
    inline void add_cat_ops(uint64_t amnt) { m_cat_ops += amnt; }
    inline void add_ra_req_nas(uint64_t amnt) { m_ra_req_nas += amnt; }
    inline void add_ra_rep_nas(uint64_t amnt) { m_ra_rep_nas += amnt; }
    inline void add_l2_srz(uint64_t amnt) { m_l2_srz += amnt; }
    inline void add_l2_ops(uint64_t amnt) { m_l2_ops += amnt; }
    inline void add_dramctrl_req_nas(uint64_t amnt) { m_dramctrl_req_nas += amnt; }
    inline void add_dramctrl_rep_nas(uint64_t amnt) { m_dramctrl_rep_nas += amnt; }
    inline void add_dram_ops(uint64_t amnt) { m_dram_ops += amnt; }

    /* L1 cost categories */
    inline void add_local_l1_cost_for_hit(uint64_t amnt) { m_local_l1_cost_for_hit += amnt; }
    inline void add_remote_l1_cost_for_hit(uint64_t amnt) { m_remote_l1_cost_for_hit += amnt; }
    inline void add_local_l1_cost_for_miss(uint64_t amnt) { m_local_l1_cost_for_miss += amnt; }
    inline void add_remote_l1_cost_for_miss(uint64_t amnt) { m_remote_l1_cost_for_miss += amnt; }
    inline void add_local_l1_cost_for_update(uint64_t amnt) { m_local_l1_cost_for_update += amnt; }
    inline void add_remote_l1_cost_for_update(uint64_t amnt) { m_remote_l1_cost_for_update += amnt; }

    /* L2 cost categories */
    inline void add_local_l2_cost_for_hit(uint64_t amnt) { m_local_l2_cost_for_hit += amnt; }
    inline void add_local_l2_cost_for_miss(uint64_t amnt) { m_local_l2_cost_for_miss += amnt; }
    inline void add_local_l2_cost_for_update(uint64_t amnt) { m_local_l2_cost_for_update += amnt; }
    inline void add_local_l2_cost_for_writeback(uint64_t amnt) { m_local_l2_cost_for_writeback += amnt; }
    inline void add_remote_l2_cost_for_hit(uint64_t amnt) { m_remote_l2_cost_for_hit += amnt; }
    inline void add_remote_l2_cost_for_miss(uint64_t amnt) { m_remote_l2_cost_for_miss += amnt; }
    inline void add_remote_l2_cost_for_update(uint64_t amnt) { m_remote_l2_cost_for_update += amnt; }
    inline void add_remote_l2_cost_for_writeback(uint64_t amnt) { m_remote_l2_cost_for_writeback += amnt; }

    friend class sharedSharedEMRAStatsPerTile;
    friend class sharedSharedEMRAStats;

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
    uint64_t m_cat_srz;
    uint64_t m_cat_ops;
    uint64_t m_ra_req_nas;
    uint64_t m_ra_rep_nas;
    uint64_t m_l2_srz;
    uint64_t m_l2_ops;
    uint64_t m_dramctrl_req_nas;
    uint64_t m_dramctrl_rep_nas;
    uint64_t m_dram_ops;
    uint64_t m_mig;

    uint64_t m_local_l1_cost_for_hit;
    uint64_t m_remote_l1_cost_for_hit;
    uint64_t m_local_l1_cost_for_miss;
    uint64_t m_remote_l1_cost_for_miss;
    uint64_t m_local_l1_cost_for_update;
    uint64_t m_remote_l1_cost_for_update;

    uint64_t m_local_l2_cost_for_hit;
    uint64_t m_local_l2_cost_for_miss;
    uint64_t m_local_l2_cost_for_update;
    uint64_t m_local_l2_cost_for_writeback;
    uint64_t m_remote_l2_cost_for_hit;
    uint64_t m_remote_l2_cost_for_miss;
    uint64_t m_remote_l2_cost_for_update;
    uint64_t m_remote_l2_cost_for_writeback;

    map<int, std::shared_ptr<sharedSharedEMRAStatsPerMemInstr> > m_tentative_data;
    
};

class sharedSharedEMRAStatsPerTile : public memStatsPerTile {
public:
    sharedSharedEMRAStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~sharedSharedEMRAStatsPerTile();

    inline void new_read_instr_at_l1() { ++m_num_l1_read_instr; }
    inline void new_write_instr_at_l1() { ++m_num_l1_write_instr; }
    inline void new_read_instr_at_l2() { ++m_num_l2_read_instr; }
    inline void new_write_instr_at_l2() { ++m_num_l2_write_instr; }

    inline void hit_for_read_instr_at_l1() { ++m_num_hits_for_l1_read_instr; }
    inline void hit_for_write_instr_at_l1() { ++m_num_hits_for_l1_write_instr; }
    inline void hit_for_read_instr_at_l2() { ++m_num_hits_for_l2_read_instr; }
    inline void hit_for_write_instr_at_l2() { ++m_num_hits_for_l2_write_instr; }

    inline void core_miss_for_read_instr_at_l1() { ++m_num_core_misses_for_l1_read_instr; }
    inline void core_miss_for_write_instr_at_l1() { ++m_num_core_misses_for_l1_write_instr; }

    inline void true_miss_for_read_instr_at_l1() { ++m_num_true_misses_for_l1_read_instr; }
    inline void true_miss_for_write_instr_at_l1() { ++m_num_true_misses_for_l1_write_instr; }
    inline void true_miss_for_read_instr_at_l2() { ++m_num_true_misses_for_l2_read_instr; }
    inline void true_miss_for_write_instr_at_l2() { ++m_num_true_misses_for_l2_write_instr; }

    inline void hit_for_read_instr_at_local_l1() { ++m_num_hits_for_local_l1_read_instr; }
    inline void hit_for_write_instr_at_local_l1() { ++m_num_hits_for_local_l1_write_instr; }
    inline void hit_for_read_instr_at_local_l2() { ++m_num_hits_for_local_l2_read_instr; }
    inline void hit_for_write_instr_at_local_l2() { ++m_num_hits_for_local_l2_write_instr; }
    inline void true_miss_for_read_instr_at_local_l1() { ++m_num_true_misses_for_local_l1_read_instr; }
    inline void true_miss_for_write_instr_at_local_l1() { ++m_num_true_misses_for_local_l1_write_instr; }
    inline void true_miss_for_read_instr_at_local_l2() { ++m_num_true_misses_for_local_l2_read_instr; }
    inline void true_miss_for_write_instr_at_local_l2() { ++m_num_true_misses_for_local_l2_write_instr; }
    inline void hit_for_read_instr_at_remote_l1() { ++m_num_hits_for_remote_l1_read_instr; }
    inline void hit_for_write_instr_at_remote_l1() { ++m_num_hits_for_remote_l1_write_instr; }
    inline void hit_for_read_instr_at_remote_l2() { ++m_num_hits_for_remote_l2_read_instr; }
    inline void hit_for_write_instr_at_remote_l2() { ++m_num_hits_for_remote_l2_write_instr; }
    inline void true_miss_for_read_instr_at_remote_l1() { ++m_num_true_misses_for_remote_l1_read_instr; }
    inline void true_miss_for_write_instr_at_remote_l1() { ++m_num_true_misses_for_remote_l1_write_instr; }
    inline void true_miss_for_read_instr_at_remote_l2() { ++m_num_true_misses_for_remote_l2_read_instr; }
    inline void true_miss_for_write_instr_at_remote_l2() { ++m_num_true_misses_for_remote_l2_write_instr; }

    inline void evict_at_l1() { ++m_num_evict_at_l1; }
    inline void evict_at_l2() { ++m_num_evict_at_l2; }
    inline void writeback_at_l1() { ++m_num_writeback_at_l1; }
    inline void writeback_at_l2() { ++m_num_writeback_at_l2; }

    inline void add_cat_action() { ++m_num_cat_action; }
    inline void add_l1_action() { ++m_num_l1_action; }
    inline void add_l2_action() { ++m_num_l2_action; }
    inline void add_dram_action() { ++m_num_dram_action; }

    void add(const sharedSharedEMRAStatsPerTile& other);
    void commit_per_mem_instr_stats(const sharedSharedEMRAStatsPerMemInstr& data);
    inline void commit_per_mem_instr_stats(std::shared_ptr<sharedSharedEMRAStatsPerMemInstr> data) { commit_per_mem_instr_stats(*data); }

    friend class sharedSharedEMRAStats;

private:
    uint64_t m_num_l1_read_instr; 
    uint64_t m_num_l1_write_instr; 
    uint64_t m_num_l2_read_instr; 
    uint64_t m_num_l2_write_instr; 
    uint64_t m_num_hits_for_l1_read_instr; 
    uint64_t m_num_hits_for_l1_write_instr; 
    uint64_t m_num_hits_for_l2_read_instr; 
    uint64_t m_num_hits_for_l2_write_instr; 
    uint64_t m_num_core_misses_for_l1_read_instr; 
    uint64_t m_num_core_misses_for_l1_write_instr; 
    uint64_t m_num_true_misses_for_l1_read_instr; 
    uint64_t m_num_true_misses_for_l1_write_instr; 
    uint64_t m_num_true_misses_for_l2_read_instr; 
    uint64_t m_num_true_misses_for_l2_write_instr; 

    uint64_t m_num_core_hit_read_instr;
    uint64_t m_num_core_hit_write_instr;
    uint64_t m_num_core_miss_read_instr;
    uint64_t m_num_core_miss_write_instr;

    uint64_t m_num_hits_for_local_l1_read_instr; 
    uint64_t m_num_hits_for_local_l1_write_instr; 
    uint64_t m_num_hits_for_local_l2_read_instr; 
    uint64_t m_num_hits_for_local_l2_write_instr; 
    uint64_t m_num_true_misses_for_local_l1_read_instr; 
    uint64_t m_num_true_misses_for_local_l1_write_instr; 
    uint64_t m_num_true_misses_for_local_l2_read_instr; 
    uint64_t m_num_true_misses_for_local_l2_write_instr; 
    uint64_t m_num_hits_for_remote_l1_read_instr; 
    uint64_t m_num_hits_for_remote_l1_write_instr; 
    uint64_t m_num_hits_for_remote_l2_read_instr; 
    uint64_t m_num_hits_for_remote_l2_write_instr; 
    uint64_t m_num_true_misses_for_remote_l1_read_instr; 
    uint64_t m_num_true_misses_for_remote_l1_write_instr; 
    uint64_t m_num_true_misses_for_remote_l2_read_instr; 
    uint64_t m_num_true_misses_for_remote_l2_write_instr; 

    uint64_t m_num_evict_at_l1;
    uint64_t m_num_evict_at_l2;
    uint64_t m_num_writeback_at_l1;
    uint64_t m_num_writeback_at_l2;

    uint64_t m_num_cat_action;
    uint64_t m_num_l1_action;
    uint64_t m_num_l2_action;
    uint64_t m_num_dram_action;

    sharedSharedEMRAStatsPerMemInstr m_total_per_mem_instr_info;

};

class sharedSharedEMRAStats : public memStats {
public:
    sharedSharedEMRAStats(const uint64_t &system_time);
    virtual ~sharedSharedEMRAStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
