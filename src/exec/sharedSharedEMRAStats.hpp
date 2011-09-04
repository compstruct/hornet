// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SHARED_SHARED_EMRA_STATS_HPP__
#define __SHARED_SHARED_EMRA_STATS_HPP__

#include "memStats.hpp"
#include "memory_types.hpp"

class sharedSharedEMRAStatsPerMemInstr {
public:
    sharedSharedEMRAStatsPerMemInstr();
    ~sharedSharedEMRAStatsPerMemInstr();

    uint64_t total_cost();
    void add(const sharedSharedEMRAStatsPerMemInstr& other);

    shared_ptr<sharedSharedEMRAStatsPerMemInstr> get_tentative_data(int index);
    inline void clear_tentative_data() { m_tentative_data.clear(); }
    void discard_tentative_data(int index);
    void commit_tentative_data(int index);
    void commit_max_tentative_data();
    void commit_min_tentative_data();

    void apply_mig_latency(const uint64_t cur_time);
    inline void discard_mig_latency() { m_did_migrate = false; m_mig_depart_time = 0; }

    inline void did_core_miss() { m_did_core_miss = true; }
    inline void did_migrate(uint64_t depart_time) { m_did_migrate = true; m_mig_depart_time = depart_time; }

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

    friend class sharedSharedEMRAStatsPerTile;
    friend class sharedSharedEMRAStats;

private:
    bool add_new_tentative_data(int index);

    bool m_did_core_miss;
    bool m_did_migrate;
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

    map<int, shared_ptr<sharedSharedEMRAStatsPerMemInstr> > m_tentative_data;
    
};

class sharedSharedEMRAStatsPerTile : public memStatsPerTile {
public:
    sharedSharedEMRAStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~sharedSharedEMRAStatsPerTile();

    inline void new_read_instr_at_L1() { ++m_num_l1_read_instr; }
    inline void new_write_instr_at_L1() { ++m_num_l1_write_instr; }
    inline void new_read_instr_at_L2() { ++m_num_l2_read_instr; }
    inline void new_write_instr_at_L2() { ++m_num_l2_write_instr; }

    inline void hit_for_read_instr_at_L1() { ++m_num_hits_for_l1_read_instr; }
    inline void hit_for_write_instr_at_L1() { ++m_num_hits_for_l1_write_instr; }
    inline void hit_for_read_instr_at_L2() { ++m_num_hits_for_l2_read_instr; }
    inline void hit_for_write_instr_at_L2() { ++m_num_hits_for_l2_write_instr; }

    inline void core_miss_for_read_instr_at_L1() { ++m_num_core_misses_for_l1_read_instr; }
    inline void core_miss_for_write_instr_at_L1() { ++m_num_core_misses_for_l1_write_instr; }

    inline void true_miss_for_read_instr_at_L1() { ++m_num_true_misses_for_l1_read_instr; }
    inline void true_miss_for_write_instr_at_L1() { ++m_num_true_misses_for_l1_write_instr; }
    inline void true_miss_for_read_instr_at_L2() { ++m_num_true_misses_for_l2_read_instr; }
    inline void true_miss_for_write_instr_at_L2() { ++m_num_true_misses_for_l2_write_instr; }

    inline void add_cat_action() { ++m_num_cat_action; }
    inline void add_l1_action() { ++m_num_l1_action; }
    inline void add_l2_action() { ++m_num_l2_action; }
    inline void add_dram_action() { ++m_num_dram_action; }

    void add(const sharedSharedEMRAStatsPerTile& other);
    void commit_per_mem_instr_stats(const sharedSharedEMRAStatsPerMemInstr& data);
    inline void commit_per_mem_instr_stats(shared_ptr<sharedSharedEMRAStatsPerMemInstr> data) { commit_per_mem_instr_stats(*data); }

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

    uint64_t m_num_core_hit_instr;
    uint64_t m_num_core_miss_instr;

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
