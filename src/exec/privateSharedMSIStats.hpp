// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __PRIVATE_SHARED_MSI_STATS_HPP__
#define __PRIVATE_SHARED_MSI_STATS_HPP__

#include "memStats.hpp"

class privateSharedMSIStatsPerMemInstr {
public:
    privateSharedMSIStatsPerMemInstr(bool is_read);
    ~privateSharedMSIStatsPerMemInstr();

    uint64_t total_cost();
    void add(const privateSharedMSIStatsPerMemInstr& other);

    shared_ptr<privateSharedMSIStatsPerMemInstr> get_tentative_data(int index);
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

    inline void begin_inv_for_coherence(uint64_t start_time, uint32_t num_targets) { 
        m_inv_begin_time = start_time; m_num_inv_for_coherence_targets += num_targets; 
    }
    inline void end_inv_for_coherence(uint64_t end_time, bool write_stats, bool local) { 
        if (write_stats) {
            m_inv_for_coherence += end_time - m_inv_begin_time; 
            if (local) {
                m_local_inv_for_coherence += end_time - m_inv_begin_time;
            } else {
                m_remote_inv_for_coherence += end_time - m_inv_begin_time;
            }
        }
    }

    inline void begin_inv_for_evict(uint64_t start_time, uint32_t num_targets) { 
        m_inv_begin_time = start_time; m_num_inv_for_evict_targets += num_targets; 
    }
    inline void end_inv_for_evict(uint64_t end_time, bool write_stats, bool local) { 
        if (write_stats) {
            m_inv_for_evict += end_time - m_inv_begin_time; 
            if (local) {
                m_local_inv_for_evict += end_time - m_inv_begin_time;
            } else {
                m_remote_inv_for_evict += end_time - m_inv_begin_time;
            }
        }
    }

    inline void begin_reorder(uint64_t start_time) { m_reorder_begin_time = start_time; }
    inline void end_reorder(uint64_t end_time, bool write_stats, bool local) { 
        m_did_reorder = true; 
        if (write_stats) {
            m_reorder += end_time - m_reorder_begin_time; 
            if (local) {
                m_local_reorder += end_time - m_reorder_begin_time;
            } else {
                m_remote_reorder += end_time - m_reorder_begin_time;
            }
        }
    }

    inline void add_mem_srz(uint64_t amnt) { m_mem_srz += amnt; }
    inline void add_l1_srz(uint64_t amnt) { m_l1_srz += amnt; }
    inline void add_l1_ops(uint64_t amnt) { m_l1_ops += amnt; }
    inline void add_cat_srz(uint64_t amnt) { m_cat_srz += amnt; }
    inline void add_cat_ops(uint64_t amnt) { m_cat_ops += amnt; }
    inline void add_cache_req_nas(uint64_t amnt) { m_cache_req_nas += amnt; }
    inline void add_dir_rep_nas(uint64_t amnt) { m_dir_rep_nas += amnt; }
    inline void add_cache_rep_srz_for_switch(uint64_t amnt) { m_cache_rep_srz_for_switch += amnt; }
    inline void add_cache_rep_srz_for_evict(uint64_t amnt) { m_cache_rep_srz_for_evict += amnt; }
    inline void add_l2_srz(uint64_t amnt) { m_l2_srz += amnt; }
    inline void add_l2_ops(uint64_t amnt) { m_l2_ops += amnt; }
    inline void add_empty_req_srz(uint64_t amnt, bool local) { 
        m_empty_req_srz += amnt; 
        if (local) {
            m_local_empty_req_srz += amnt;
        } else {
            m_remote_empty_req_srz += amnt;
        }
    }
    inline void add_dramctrl_req_nas(uint64_t amnt) { m_dramctrl_req_nas += amnt; }
    inline void add_dramctrl_rep_nas(uint64_t amnt) { m_dramctrl_rep_nas += amnt; }
    inline void add_dram_ops(uint64_t amnt) { m_dram_ops += amnt; }

    /* L1 cost categories */
    inline void add_l1_cost_for_hit(uint64_t amnt) { m_l1_cost_for_hit += amnt; }
    inline void add_l1_cost_for_true_miss(uint64_t amnt) { m_l1_cost_for_true_miss += amnt; }
    inline void add_l1_cost_for_write_on_shared_miss(uint64_t amnt) { m_l1_cost_for_write_on_shared += amnt; }
    inline void add_l1_cost_for_feed(uint64_t amnt) { m_l1_cost_for_feed += amnt; }

    /* L2 cost categories */
    inline void add_local_l2_cost_for_hit(uint64_t amnt) { m_local_l2_cost_for_hit += amnt; }
    inline void add_local_l2_cost_for_true_miss(uint64_t amnt) { m_local_l2_cost_for_true_miss += amnt; }
    inline void add_local_l2_cost_for_write_on_shared_miss(uint64_t amnt) { m_local_l2_cost_for_write_on_shared_miss += amnt; }
    inline void add_local_l2_cost_for_read_on_exclusive_miss(uint64_t amnt) { m_local_l2_cost_for_read_on_exclusive_miss += amnt; }
    inline void add_local_l2_cost_for_write_on_exclusive_miss(uint64_t amnt) { m_local_l2_cost_for_write_on_exclusive_miss += amnt; }
    inline void add_local_l2_cost_for_directory_update(uint64_t amnt) { m_local_l2_cost_for_directory_update += amnt; }
    inline void add_local_l2_cost_for_writeback(uint64_t amnt) { m_local_l2_cost_for_writeback += amnt; }
    inline void add_local_l2_cost_for_evict(uint64_t amnt) { m_local_l2_cost_for_evict += amnt; }
    inline void add_local_l2_cost_for_feed(uint64_t amnt) { m_local_l2_cost_for_feed += amnt; }
    inline void add_local_l2_cost_for_feed_retry(uint64_t amnt) { m_local_l2_cost_for_feed_retry += amnt; }

    inline void add_remote_l2_cost_for_hit(uint64_t amnt) { m_remote_l2_cost_for_hit += amnt; }
    inline void add_remote_l2_cost_for_true_miss(uint64_t amnt) { m_remote_l2_cost_for_true_miss += amnt; }
    inline void add_remote_l2_cost_for_write_on_shared_miss(uint64_t amnt) { m_remote_l2_cost_for_write_on_shared_miss += amnt; }
    inline void add_remote_l2_cost_for_read_on_exclusive_miss(uint64_t amnt) { m_remote_l2_cost_for_read_on_exclusive_miss += amnt; }
    inline void add_remote_l2_cost_for_write_on_exclusive_miss(uint64_t amnt) { m_remote_l2_cost_for_write_on_exclusive_miss += amnt; }
    inline void add_remote_l2_cost_for_directory_update(uint64_t amnt) { m_remote_l2_cost_for_directory_update += amnt; }
    inline void add_remote_l2_cost_for_writeback(uint64_t amnt) { m_remote_l2_cost_for_writeback += amnt; }
    inline void add_remote_l2_cost_for_evict(uint64_t amnt) { m_remote_l2_cost_for_evict += amnt; }
    inline void add_remote_l2_cost_for_feed(uint64_t amnt) { m_remote_l2_cost_for_feed += amnt; }
    inline void add_remote_l2_cost_for_feed_retry(uint64_t amnt) { m_remote_l2_cost_for_feed_retry += amnt; }

    friend class privateSharedMSIStatsPerTile;
    friend class privateSharedMSIStats;

private:
    bool add_new_tentative_data(int index);

    bool m_is_read;
    bool m_did_core_miss;
    uint64_t m_serialization_begin_time;
    bool m_in_migration;
    uint64_t m_mig_depart_time;
    uint64_t m_inv_begin_time;
    uint32_t m_num_inv_for_coherence_targets;
    uint32_t m_num_inv_for_evict_targets;
    bool m_did_reorder;
    uint64_t m_reorder_begin_time;

    uint64_t m_mem_srz;
    uint64_t m_l1_srz;
    uint64_t m_l1_ops;
    uint64_t m_cat_srz;
    uint64_t m_cat_ops;
    uint64_t m_cache_req_nas;
    uint64_t m_dir_rep_nas;
    uint64_t m_cache_rep_srz_for_switch;
    uint64_t m_cache_rep_srz_for_evict;
    uint64_t m_l2_srz;
    uint64_t m_l2_ops;
    uint64_t m_empty_req_srz;
    uint64_t m_dramctrl_req_nas;
    uint64_t m_dramctrl_rep_nas;
    uint64_t m_dram_ops;
    uint64_t m_mig;
    uint64_t m_inv_for_coherence;
    uint64_t m_inv_for_evict;
    uint64_t m_reorder;

    uint64_t m_l1_cost_for_hit;
    uint64_t m_l1_cost_for_true_miss;
    uint64_t m_l1_cost_for_write_on_shared;
    uint64_t m_l1_cost_for_feed;
    uint64_t m_local_l2_cost_for_hit;
    uint64_t m_local_l2_cost_for_true_miss;
    uint64_t m_local_l2_cost_for_write_on_shared_miss;
    uint64_t m_local_l2_cost_for_read_on_exclusive_miss;
    uint64_t m_local_l2_cost_for_write_on_exclusive_miss;
    uint64_t m_local_l2_cost_for_directory_update;
    uint64_t m_local_l2_cost_for_writeback;
    uint64_t m_local_l2_cost_for_evict;
    uint64_t m_local_l2_cost_for_feed;
    uint64_t m_local_l2_cost_for_feed_retry;
    uint64_t m_remote_l2_cost_for_hit;
    uint64_t m_remote_l2_cost_for_true_miss;
    uint64_t m_remote_l2_cost_for_write_on_shared_miss;
    uint64_t m_remote_l2_cost_for_read_on_exclusive_miss;
    uint64_t m_remote_l2_cost_for_write_on_exclusive_miss;
    uint64_t m_remote_l2_cost_for_directory_update;
    uint64_t m_remote_l2_cost_for_writeback;
    uint64_t m_remote_l2_cost_for_evict;
    uint64_t m_remote_l2_cost_for_feed;
    uint64_t m_remote_l2_cost_for_feed_retry;

    uint64_t m_local_inv_for_coherence;
    uint64_t m_local_empty_req_srz;
    uint64_t m_local_inv_for_evict;
    uint64_t m_local_reorder;
    uint64_t m_remote_inv_for_coherence;
    uint64_t m_remote_empty_req_srz;
    uint64_t m_remote_inv_for_evict;
    uint64_t m_remote_reorder;

    map<int, shared_ptr<privateSharedMSIStatsPerMemInstr> > m_tentative_data;

};

class privateSharedMSIStatsPerTile : public memStatsPerTile {
public:
    privateSharedMSIStatsPerTile(uint32_t id, const uint64_t &system_time);
    virtual ~privateSharedMSIStatsPerTile();

    inline void new_read_instr_at_l1() { ++m_num_l1_read_instr; }
    inline void new_write_instr_at_l1() { ++m_num_l1_write_instr; }
    inline void new_read_instr_at_l2() { ++m_num_l2_read_instr; }
    inline void new_write_instr_at_l2() { ++m_num_l2_write_instr; }
    
    inline void hit_for_read_instr_at_l1() { ++m_num_hits_for_read_instr_at_l1; }
    inline void hit_for_write_instr_at_l1() { ++m_num_hits_for_write_instr_at_l1; }
    inline void hit_for_read_instr_at_l2() { ++m_num_hits_for_read_instr_at_l2; }
    inline void hit_for_write_instr_at_l2() { ++m_num_hits_for_write_instr_at_l2; }

    inline void true_miss_for_read_instr_at_l1() { ++m_num_true_misses_for_read_instr_at_l1; }
    inline void true_miss_for_write_instr_at_l1() { ++m_num_true_misses_for_write_instr_at_l1; }
    inline void true_miss_for_read_instr_at_l2() { ++m_num_true_misses_for_read_instr_at_l2; }
    inline void true_miss_for_write_instr_at_l2() { ++m_num_true_misses_for_write_instr_at_l2; }

    inline void write_on_shared_miss_for_write_instr_at_l1() { ++m_num_write_on_shared_misses_for_write_instr_at_l1; }
    inline void write_on_shared_miss_for_write_instr_at_l2() { ++m_num_write_on_shared_misses_for_write_instr_at_l2; }
    inline void read_on_exclusive_miss_for_read_instr_at_l2() { ++m_num_read_on_exclusive_misses_for_read_instr_at_l2; }
    inline void write_on_exclusive_miss_for_write_instr_at_l2() { ++m_num_write_on_exclusive_misses_for_write_instr_at_l2; }

    inline void hit_for_read_instr_at_local_l2() { ++m_num_hits_for_read_instr_at_local_l2; }
    inline void hit_for_write_instr_at_local_l2() { ++m_num_hits_for_write_instr_at_local_l2; }
    inline void true_miss_for_read_instr_at_local_l2() { ++m_num_true_misses_for_read_instr_at_local_l2; }
    inline void true_miss_for_write_instr_at_local_l2() { ++m_num_true_misses_for_write_instr_at_local_l2; }
    inline void write_on_shared_miss_for_write_instr_at_local_l2() { ++m_num_write_on_shared_misses_for_write_instr_at_local_l2; }
    inline void read_on_exclusive_miss_for_read_instr_at_local_l2() { ++m_num_read_on_exclusive_misses_for_read_instr_at_local_l2; }
    inline void write_on_exclusive_miss_for_write_instr_at_local_l2() { ++m_num_write_on_exclusive_misses_for_write_instr_at_local_l2; }
    inline void hit_for_read_instr_at_remote_l2() { ++m_num_hits_for_read_instr_at_remote_l2; }
    inline void hit_for_write_instr_at_remote_l2() { ++m_num_hits_for_write_instr_at_remote_l2; }
    inline void true_miss_for_read_instr_at_remote_l2() { ++m_num_true_misses_for_read_instr_at_remote_l2; }
    inline void true_miss_for_write_instr_at_remote_l2() { ++m_num_true_misses_for_write_instr_at_remote_l2; }
    inline void write_on_shared_miss_for_write_instr_at_remote_l2() { ++m_num_write_on_shared_misses_for_write_instr_at_remote_l2; }
    inline void read_on_exclusive_miss_for_read_instr_at_remote_l2() { ++m_num_read_on_exclusive_misses_for_read_instr_at_remote_l2; }
    inline void write_on_exclusive_miss_for_write_instr_at_remote_l2() { ++m_num_write_on_exclusive_misses_for_write_instr_at_remote_l2; }

    inline void retry_for_update_at_l2() { ++m_num_retry_for_update_at_l2; }

    inline void evict_at_l1() { ++m_num_evict_at_l1; }
    inline void evict_at_l2() { ++m_num_evict_at_l2; }
    inline void writeback_at_l1() { ++m_num_writeback_at_l1; }
    inline void writeback_at_l2() { ++m_num_writeback_at_l2; }

    inline void add_cat_action() { ++m_num_cat_action; }
    inline void add_l1_action() { ++m_num_l1_action; }
    inline void add_l2_action() { ++m_num_l2_action; }
    inline void add_dram_action() { ++m_num_dram_action; }

    inline void shreq_sent(bool is_remote) { ++m_shreq_sent; if (is_remote) { ++m_shreq_sent_remote; } }
    inline void exreq_sent(bool is_remote) { ++m_exreq_sent; if (is_remote) { ++m_exreq_sent_remote; } }
    inline void invrep_sent(bool is_remote) { ++m_invrep_sent; if (is_remote) { ++m_invrep_sent_remote; } }
    inline void flushrep_sent(bool is_remote) { ++m_flushrep_sent; if (is_remote) { ++m_flushrep_sent_remote; } }
    inline void wbrep_sent(bool is_remote) { ++m_wbrep_sent; if (is_remote) { ++m_wbrep_sent_remote; } }
    inline void shrep_sent(bool is_remote) { ++m_shrep_sent; if (is_remote) { ++m_shrep_sent_remote; } }
    inline void exrep_sent(bool is_remote) { ++m_exrep_sent; if (is_remote) { ++m_exrep_sent_remote; } }
    inline void invreq_sent(bool is_remote) { ++m_invreq_sent; if (is_remote) { ++m_invreq_sent_remote; } }
    inline void flushreq_sent(bool is_remote) { ++m_flushreq_sent; if (is_remote) { ++m_flushreq_sent_remote; } }
    inline void wbreq_sent(bool is_remote) { ++m_wbreq_sent; if (is_remote) { ++m_wbreq_sent_remote; } }
    inline void empty_req_sent() { ++m_emptyreq_sent; }

    void add(const privateSharedMSIStatsPerTile& other);
    void commit_per_mem_instr_stats(const privateSharedMSIStatsPerMemInstr& data);
    inline void commit_per_mem_instr_stats(shared_ptr<privateSharedMSIStatsPerMemInstr> data) { commit_per_mem_instr_stats(*data); }

    friend class privateSharedMSIStats;

private:
    uint64_t m_num_l1_read_instr;
    uint64_t m_num_l1_write_instr;
    uint64_t m_num_l2_read_instr;
    uint64_t m_num_l2_write_instr;
    
    uint64_t m_num_hits_for_read_instr_at_l1;
    uint64_t m_num_hits_for_write_instr_at_l1;
    uint64_t m_num_hits_for_read_instr_at_l2;
    uint64_t m_num_hits_for_write_instr_at_l2;

    uint64_t m_num_true_misses_for_read_instr_at_l1;
    uint64_t m_num_true_misses_for_write_instr_at_l1;
    uint64_t m_num_true_misses_for_read_instr_at_l2;
    uint64_t m_num_true_misses_for_write_instr_at_l2;

    uint64_t m_num_write_on_shared_misses_for_write_instr_at_l1;
    uint64_t m_num_write_on_shared_misses_for_write_instr_at_l2;
    uint64_t m_num_read_on_exclusive_misses_for_read_instr_at_l2;
    uint64_t m_num_write_on_exclusive_misses_for_write_instr_at_l2;

    uint64_t m_num_hits_for_read_instr_at_local_l2; 
    uint64_t m_num_hits_for_write_instr_at_local_l2; 
    uint64_t m_num_true_misses_for_read_instr_at_local_l2; 
    uint64_t m_num_true_misses_for_write_instr_at_local_l2; 
    uint64_t m_num_write_on_shared_misses_for_write_instr_at_local_l2; 
    uint64_t m_num_read_on_exclusive_misses_for_read_instr_at_local_l2; 
    uint64_t m_num_write_on_exclusive_misses_for_write_instr_at_local_l2; 
    uint64_t m_num_hits_for_read_instr_at_remote_l2; 
    uint64_t m_num_hits_for_write_instr_at_remote_l2; 
    uint64_t m_num_true_misses_for_read_instr_at_remote_l2; 
    uint64_t m_num_true_misses_for_write_instr_at_remote_l2; 
    uint64_t m_num_write_on_shared_misses_for_write_instr_at_remote_l2; 
    uint64_t m_num_read_on_exclusive_misses_for_read_instr_at_remote_l2; 
    uint64_t m_num_write_on_exclusive_misses_for_write_instr_at_remote_l2; 

    uint64_t m_num_retry_for_update_at_l2;

    uint64_t m_num_core_hit_read_instr;
    uint64_t m_num_core_hit_write_instr;
    uint64_t m_num_core_miss_read_instr;
    uint64_t m_num_core_miss_write_instr;

    uint64_t m_num_evict_at_l1;
    uint64_t m_num_evict_at_l2;
    uint64_t m_num_writeback_at_l1;
    uint64_t m_num_writeback_at_l2;

    uint64_t m_num_inv_for_coherence;
    uint64_t m_num_inv_targets_for_coherence;
    uint64_t m_num_inv_for_evict;
    uint64_t m_num_inv_targets_for_evict;
    uint64_t m_num_reorder;

    uint64_t m_num_cat_action;
    uint64_t m_num_l1_action;
    uint64_t m_num_l2_action;
    uint64_t m_num_dram_action;
    
    uint64_t m_shreq_sent;
    uint64_t m_exreq_sent;
    uint64_t m_invrep_sent;
    uint64_t m_flushrep_sent;
    uint64_t m_wbrep_sent;
    uint64_t m_shrep_sent;
    uint64_t m_exrep_sent;
    uint64_t m_invreq_sent;
    uint64_t m_flushreq_sent;
    uint64_t m_wbreq_sent;
    uint64_t m_emptyreq_sent;
    uint64_t m_shreq_sent_remote;
    uint64_t m_exreq_sent_remote;
    uint64_t m_invrep_sent_remote;
    uint64_t m_flushrep_sent_remote;
    uint64_t m_wbrep_sent_remote;
    uint64_t m_shrep_sent_remote;
    uint64_t m_exrep_sent_remote;
    uint64_t m_invreq_sent_remote;
    uint64_t m_flushreq_sent_remote;
    uint64_t m_wbreq_sent_remote;

    privateSharedMSIStatsPerMemInstr m_total_per_mem_instr_info;
};

class privateSharedMSIStats : public memStats {
public:
    privateSharedMSIStats(const uint64_t &system_time);
    virtual ~privateSharedMSIStats();

protected:
    virtual void print_stats(ostream &out);

};

#endif
