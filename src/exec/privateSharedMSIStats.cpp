// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSIStats.hpp"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(true) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

privateSharedMSIStatsPerMemInstr::privateSharedMSIStatsPerMemInstr(bool is_read) :
    m_is_read(is_read),
    m_did_core_miss(false),
    m_in_migration(false),
    m_mig_depart_time(0),
    m_inv_begin_time(0),
    m_num_inv_for_coherence_targets(0),
    m_num_inv_for_evict_targets(0),
    m_did_reorder(false),
    m_reorder_begin_time(0),
    m_mem_srz(0),
    m_l1_srz(0),
    m_l1_ops(0),
    m_cat_srz(0),
    m_cat_ops(0),
    m_cache_req_nas(0),
    m_dir_rep_nas(0),
    m_cache_rep_srz_for_switch(0),
    m_cache_rep_srz_for_evict(0),
    m_l2_srz(0),
    m_l2_ops(0),
    m_empty_req_srz(0),
    m_dramctrl_req_nas(0),
    m_dramctrl_rep_nas(0),
    m_dram_ops(0),
    m_mig(0),
    m_inv_for_coherence(0),
    m_inv_for_evict(0),
    m_reorder(0),
    m_l1_cost_for_hit(0),
    m_l1_cost_for_true_miss(0),
    m_l1_cost_for_write_on_shared(0),
    m_l1_cost_for_feed(0),
    m_local_l2_cost_for_hit(0),
    m_local_l2_cost_for_true_miss(0),
    m_local_l2_cost_for_write_on_shared_miss(0),
    m_local_l2_cost_for_read_on_exclusive_miss(0),
    m_local_l2_cost_for_write_on_exclusive_miss(0),
    m_local_l2_cost_for_directory_update(0),
    m_local_l2_cost_for_writeback(0),
    m_local_l2_cost_for_evict(0),
    m_local_l2_cost_for_feed(0),
    m_local_l2_cost_for_feed_retry(0),
    m_remote_l2_cost_for_hit(0),
    m_remote_l2_cost_for_true_miss(0),
    m_remote_l2_cost_for_write_on_shared_miss(0),
    m_remote_l2_cost_for_read_on_exclusive_miss(0),
    m_remote_l2_cost_for_write_on_exclusive_miss(0),
    m_remote_l2_cost_for_directory_update(0),
    m_remote_l2_cost_for_writeback(0),
    m_remote_l2_cost_for_evict(0),
    m_remote_l2_cost_for_feed(0),
    m_remote_l2_cost_for_feed_retry(0),
    m_local_inv_for_coherence(0),
    m_local_empty_req_srz(0),
    m_local_inv_for_evict(0),
    m_local_reorder(0),
    m_remote_inv_for_coherence(0),
    m_remote_empty_req_srz(0),
    m_remote_inv_for_evict(0),
    m_remote_reorder(0)
{}

privateSharedMSIStatsPerMemInstr::~privateSharedMSIStatsPerMemInstr() {}

void privateSharedMSIStatsPerMemInstr::add(const privateSharedMSIStatsPerMemInstr& other) {
    m_num_inv_for_coherence_targets += other.m_num_inv_for_coherence_targets;
    m_num_inv_for_evict_targets += other.m_num_inv_for_evict_targets;
    m_mem_srz += other.m_mem_srz;
    m_l1_srz += other.m_l1_srz;
    m_l1_ops += other.m_l1_ops;
    m_cat_srz += other.m_cat_srz;
    m_cat_ops += other.m_cat_ops;
    m_cache_req_nas += other.m_cache_req_nas;
    m_dir_rep_nas += other.m_dir_rep_nas;
    m_cache_rep_srz_for_switch += other.m_cache_rep_srz_for_switch;
    m_cache_rep_srz_for_evict += other.m_cache_rep_srz_for_evict;
    m_l2_srz += other.m_l2_srz;
    m_l2_ops += other.m_l2_ops;
    m_empty_req_srz += other.m_empty_req_srz;
    m_dramctrl_req_nas += other.m_dramctrl_req_nas;
    m_dramctrl_rep_nas += other.m_dramctrl_rep_nas;
    m_dram_ops += other.m_dram_ops;
    m_mig += other.m_mig;
    m_inv_for_coherence += other.m_inv_for_coherence;
    m_inv_for_evict += other.m_inv_for_evict;
    m_reorder += other.m_reorder;
    m_l1_cost_for_hit += other.m_l1_cost_for_hit;
    m_l1_cost_for_true_miss += other.m_l1_cost_for_true_miss;
    m_l1_cost_for_write_on_shared += other.m_l1_cost_for_write_on_shared;
    m_l1_cost_for_feed += other.m_l1_cost_for_feed;
    m_local_l2_cost_for_hit += other.m_local_l2_cost_for_hit;
    m_local_l2_cost_for_true_miss += other.m_local_l2_cost_for_true_miss;
    m_local_l2_cost_for_write_on_shared_miss += other.m_local_l2_cost_for_write_on_shared_miss;
    m_local_l2_cost_for_read_on_exclusive_miss += other.m_local_l2_cost_for_read_on_exclusive_miss;
    m_local_l2_cost_for_write_on_exclusive_miss += other.m_local_l2_cost_for_write_on_exclusive_miss;
    m_local_l2_cost_for_directory_update += other.m_local_l2_cost_for_directory_update;
    m_local_l2_cost_for_writeback += other.m_local_l2_cost_for_writeback;
    m_local_l2_cost_for_evict += other.m_local_l2_cost_for_evict;
    m_local_l2_cost_for_feed += other.m_local_l2_cost_for_feed;
    m_local_l2_cost_for_feed_retry += other.m_local_l2_cost_for_feed_retry;
    m_remote_l2_cost_for_hit += other.m_remote_l2_cost_for_hit;
    m_remote_l2_cost_for_true_miss += other.m_remote_l2_cost_for_true_miss;
    m_remote_l2_cost_for_write_on_shared_miss += other.m_remote_l2_cost_for_write_on_shared_miss;
    m_remote_l2_cost_for_read_on_exclusive_miss += other.m_remote_l2_cost_for_read_on_exclusive_miss;
    m_remote_l2_cost_for_write_on_exclusive_miss += other.m_remote_l2_cost_for_write_on_exclusive_miss;
    m_remote_l2_cost_for_directory_update += other.m_remote_l2_cost_for_directory_update;
    m_remote_l2_cost_for_writeback += other.m_remote_l2_cost_for_writeback;
    m_remote_l2_cost_for_evict += other.m_remote_l2_cost_for_evict;
    m_remote_l2_cost_for_feed += other.m_remote_l2_cost_for_feed;
    m_remote_l2_cost_for_feed_retry += other.m_remote_l2_cost_for_feed_retry;
    m_local_inv_for_coherence += other.m_local_inv_for_coherence;
    m_local_empty_req_srz += other.m_local_empty_req_srz;
    m_local_inv_for_evict += other.m_local_inv_for_evict;
    m_local_reorder += other.m_local_reorder;
    m_remote_inv_for_coherence += other.m_remote_inv_for_coherence;
    m_remote_empty_req_srz += other.m_remote_empty_req_srz;
    m_remote_inv_for_evict += other.m_remote_inv_for_evict;
    m_remote_reorder += other.m_remote_reorder;
}

uint64_t privateSharedMSIStatsPerMemInstr::total_cost() {
    return m_mem_srz + m_l1_srz + m_l1_ops + m_cat_srz + m_cat_ops + m_cache_req_nas + m_dir_rep_nas + m_cache_rep_srz_for_switch + m_cache_rep_srz_for_evict
           + m_l2_srz + m_l2_ops + m_empty_req_srz + m_dramctrl_req_nas + m_dramctrl_rep_nas + m_mig + m_inv_for_coherence + m_inv_for_evict + m_reorder;
}

bool privateSharedMSIStatsPerMemInstr::add_new_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        return false;
    }
    std::shared_ptr<privateSharedMSIStatsPerMemInstr> new_tentative_set(new privateSharedMSIStatsPerMemInstr(m_is_read));
    m_tentative_data[index] = new_tentative_set;
    return true;
}

std::shared_ptr<privateSharedMSIStatsPerMemInstr> privateSharedMSIStatsPerMemInstr::get_tentative_data(int index) {
    if (!m_tentative_data.count(index)) {
        add_new_tentative_data(index);
    }
    return m_tentative_data[index];
}

int privateSharedMSIStatsPerMemInstr::get_max_tentative_data_index() {
    mh_assert(m_tentative_data.size());
    uint64_t value = 0;
    map<int, std::shared_ptr<privateSharedMSIStatsPerMemInstr> >::iterator it;
    int max = 0;
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
            std::shared_ptr<privateSharedMSIStatsPerMemInstr> cur = it->second;
        if (cur->total_cost() > value) {
            value = cur->total_cost();
            max = it->first;
        }
    }

    return max;
}

void privateSharedMSIStatsPerMemInstr::commit_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        add(*m_tentative_data[index]);
    }
    m_tentative_data.clear();
}

void privateSharedMSIStatsPerMemInstr::commit_max_tentative_data() {
    if (m_tentative_data.size() == 0) {
        return;
    }

    add(*m_tentative_data[get_max_tentative_data_index()]);
    m_tentative_data.clear();
}

void privateSharedMSIStatsPerMemInstr::commit_min_tentative_data() {
    uint64_t value = UINT64_MAX;
    map<int, std::shared_ptr<privateSharedMSIStatsPerMemInstr> >::iterator it;
    std::shared_ptr<privateSharedMSIStatsPerMemInstr> min = std::shared_ptr<privateSharedMSIStatsPerMemInstr>();
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
            std::shared_ptr<privateSharedMSIStatsPerMemInstr> cur = it->second;
        if (cur->total_cost() < value) {
            value = cur->total_cost();
            min = cur;
        }
    }
    if (min) {
        add(*min);
        m_tentative_data.clear();
    }
}

void privateSharedMSIStatsPerMemInstr::migration_finished(uint64_t arrival_time, bool write_stats) {
    if (m_in_migration) {
        if (write_stats) {
            m_mig += arrival_time - m_mig_depart_time;
        }
        m_in_migration = false;
    }
}

privateSharedMSIStatsPerTile::privateSharedMSIStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t), 
    m_num_l1_read_instr(0),
    m_num_l1_write_instr(0),
    m_num_l2_read_instr(0),
    m_num_l2_write_instr(0),
    m_num_hits_for_read_instr_at_l1(0),
    m_num_hits_for_write_instr_at_l1(0),
    m_num_hits_for_read_instr_at_l2(0),
    m_num_hits_for_write_instr_at_l2(0),
    m_num_true_misses_for_read_instr_at_l1(0),
    m_num_true_misses_for_write_instr_at_l1(0),
    m_num_true_misses_for_read_instr_at_l2(0),
    m_num_true_misses_for_write_instr_at_l2(0),
    m_num_write_on_shared_misses_for_write_instr_at_l1(0),
    m_num_write_on_shared_misses_for_write_instr_at_l2(0),
    m_num_read_on_exclusive_misses_for_read_instr_at_l2(0),
    m_num_write_on_exclusive_misses_for_write_instr_at_l2(0),
    m_num_hits_for_read_instr_at_local_l2(0), 
    m_num_hits_for_write_instr_at_local_l2(0), 
    m_num_true_misses_for_read_instr_at_local_l2(0), 
    m_num_true_misses_for_write_instr_at_local_l2(0), 
    m_num_write_on_shared_misses_for_write_instr_at_local_l2(0), 
    m_num_read_on_exclusive_misses_for_read_instr_at_local_l2(0), 
    m_num_write_on_exclusive_misses_for_write_instr_at_local_l2(0), 
    m_num_hits_for_read_instr_at_remote_l2(0), 
    m_num_hits_for_write_instr_at_remote_l2(0), 
    m_num_true_misses_for_read_instr_at_remote_l2(0), 
    m_num_true_misses_for_write_instr_at_remote_l2(0), 
    m_num_write_on_shared_misses_for_write_instr_at_remote_l2(0), 
    m_num_read_on_exclusive_misses_for_read_instr_at_remote_l2(0), 
    m_num_write_on_exclusive_misses_for_write_instr_at_remote_l2(0), 
    m_num_retry_for_update_at_l2(0),
    m_num_core_hit_read_instr(0),
    m_num_core_hit_write_instr(0),
    m_num_core_miss_read_instr(0),
    m_num_core_miss_write_instr(0),
    m_num_evict_at_l1(0),
    m_num_evict_at_l2(0),
    m_num_writeback_at_l1(0),
    m_num_writeback_at_l2(0),
    m_num_inv_for_coherence(0),
    m_num_inv_targets_for_coherence(0),
    m_num_inv_for_evict(0),
    m_num_inv_targets_for_evict(0),
    m_num_reorder(0),
    m_num_cat_action(0),
    m_num_l1_action(0),
    m_num_l2_action(0),
    m_num_dram_action(0),
    m_shreq_sent(0),
    m_exreq_sent(0),
    m_invrep_sent(0),
    m_flushrep_sent(0),
    m_wbrep_sent(0),
    m_shrep_sent(0),
    m_exrep_sent(0),
    m_invreq_sent(0),
    m_flushreq_sent(0),
    m_wbreq_sent(0),
    m_emptyreq_sent(0),
    m_shreq_sent_remote(0),
    m_exreq_sent_remote(0),
    m_invrep_sent_remote(0),
    m_flushrep_sent_remote(0),
    m_wbrep_sent_remote(0),
    m_shrep_sent_remote(0),
    m_exrep_sent_remote(0),
    m_invreq_sent_remote(0),
    m_flushreq_sent_remote(0),
    m_wbreq_sent_remote(0),
    m_total_per_mem_instr_info(false/*don't care*/)
{}

privateSharedMSIStatsPerTile::~privateSharedMSIStatsPerTile() {}

void privateSharedMSIStatsPerTile::add(const privateSharedMSIStatsPerTile& other) {
    m_num_l1_read_instr += other.m_num_l1_read_instr;
    m_num_l1_write_instr += other.m_num_l1_write_instr;
    m_num_l2_read_instr += other.m_num_l2_read_instr;
    m_num_l2_write_instr += other.m_num_l2_write_instr;
    m_num_hits_for_read_instr_at_l1 += other.m_num_hits_for_read_instr_at_l1;
    m_num_hits_for_write_instr_at_l1 += other.m_num_hits_for_write_instr_at_l1;
    m_num_hits_for_read_instr_at_l2 += other.m_num_hits_for_read_instr_at_l2;
    m_num_hits_for_write_instr_at_l2 += other.m_num_hits_for_write_instr_at_l2;
    m_num_true_misses_for_read_instr_at_l1 += other.m_num_true_misses_for_read_instr_at_l1;
    m_num_true_misses_for_write_instr_at_l1 += other.m_num_true_misses_for_write_instr_at_l1;
    m_num_true_misses_for_read_instr_at_l2 += other.m_num_true_misses_for_read_instr_at_l2;
    m_num_true_misses_for_write_instr_at_l2 += other.m_num_true_misses_for_write_instr_at_l2;
    m_num_write_on_shared_misses_for_write_instr_at_l1 += other.m_num_write_on_shared_misses_for_write_instr_at_l1;
    m_num_write_on_shared_misses_for_write_instr_at_l2 += other.m_num_write_on_shared_misses_for_write_instr_at_l2;
    m_num_read_on_exclusive_misses_for_read_instr_at_l2 += other.m_num_read_on_exclusive_misses_for_read_instr_at_l2;
    m_num_write_on_exclusive_misses_for_write_instr_at_l2 += other.m_num_write_on_exclusive_misses_for_write_instr_at_l2;
    m_num_hits_for_read_instr_at_local_l2 += other.m_num_hits_for_read_instr_at_local_l2; 
    m_num_hits_for_write_instr_at_local_l2 += other.m_num_hits_for_write_instr_at_local_l2; 
    m_num_true_misses_for_read_instr_at_local_l2 += other.m_num_true_misses_for_read_instr_at_local_l2; 
    m_num_true_misses_for_write_instr_at_local_l2 += other.m_num_true_misses_for_write_instr_at_local_l2; 
    m_num_write_on_shared_misses_for_write_instr_at_local_l2 += other.m_num_write_on_shared_misses_for_write_instr_at_local_l2; 
    m_num_read_on_exclusive_misses_for_read_instr_at_local_l2 += other.m_num_read_on_exclusive_misses_for_read_instr_at_local_l2; 
    m_num_write_on_exclusive_misses_for_write_instr_at_local_l2 += other.m_num_write_on_exclusive_misses_for_write_instr_at_local_l2; 
    m_num_hits_for_read_instr_at_remote_l2 += other.m_num_hits_for_read_instr_at_remote_l2; 
    m_num_hits_for_write_instr_at_remote_l2 += other.m_num_hits_for_write_instr_at_remote_l2; 
    m_num_true_misses_for_read_instr_at_remote_l2 += other.m_num_true_misses_for_read_instr_at_remote_l2; 
    m_num_true_misses_for_write_instr_at_remote_l2 += other.m_num_true_misses_for_write_instr_at_remote_l2; 
    m_num_write_on_shared_misses_for_write_instr_at_remote_l2 += other.m_num_write_on_shared_misses_for_write_instr_at_remote_l2; 
    m_num_read_on_exclusive_misses_for_read_instr_at_remote_l2 += other.m_num_read_on_exclusive_misses_for_read_instr_at_remote_l2; 
    m_num_write_on_exclusive_misses_for_write_instr_at_remote_l2 += other.m_num_write_on_exclusive_misses_for_write_instr_at_remote_l2; 
    m_num_retry_for_update_at_l2 += other.m_num_retry_for_update_at_l2;
    m_num_core_hit_read_instr += other.m_num_core_hit_read_instr;
    m_num_core_hit_write_instr += other.m_num_core_hit_write_instr;
    m_num_core_miss_read_instr += other.m_num_core_miss_read_instr;
    m_num_core_miss_write_instr += other.m_num_core_miss_write_instr;
    m_num_evict_at_l1 += other.m_num_evict_at_l1;
    m_num_evict_at_l2 += other.m_num_evict_at_l2;
    m_num_writeback_at_l1 += other.m_num_writeback_at_l1;
    m_num_writeback_at_l2 += other.m_num_writeback_at_l2;
    m_num_inv_for_coherence += other.m_num_inv_for_coherence;
    m_num_inv_targets_for_coherence += other.m_num_inv_targets_for_coherence;
    m_num_inv_for_evict += other.m_num_inv_for_evict;
    m_num_inv_targets_for_evict += other.m_num_inv_targets_for_evict;
    m_num_reorder += other.m_num_reorder;
    m_num_cat_action += other.m_num_cat_action;
    m_num_l1_action += other.m_num_l1_action;
    m_num_l2_action += other.m_num_l2_action;
    m_num_dram_action += other.m_num_dram_action;
    m_shreq_sent += other.m_shreq_sent;
    m_exreq_sent += other.m_exreq_sent;
    m_invrep_sent += other.m_invrep_sent;
    m_flushrep_sent += other.m_flushrep_sent;
    m_wbrep_sent += other.m_wbrep_sent;
    m_shrep_sent += other.m_shrep_sent;
    m_exrep_sent += other.m_exrep_sent;
    m_invreq_sent += other.m_invreq_sent;
    m_flushreq_sent += other.m_flushreq_sent;
    m_wbreq_sent += other.m_wbreq_sent;
    m_emptyreq_sent += other.m_emptyreq_sent;
    m_shreq_sent_remote += other.m_shreq_sent_remote;
    m_exreq_sent_remote += other.m_exreq_sent_remote;
    m_invrep_sent_remote += other.m_invrep_sent_remote;
    m_flushrep_sent_remote += other.m_flushrep_sent_remote;
    m_wbrep_sent_remote += other.m_wbrep_sent_remote;
    m_shrep_sent_remote += other.m_shrep_sent_remote;
    m_exrep_sent_remote += other.m_exrep_sent_remote;
    m_invreq_sent_remote += other.m_invreq_sent_remote;
    m_flushreq_sent_remote += other.m_flushreq_sent_remote;
    m_wbreq_sent_remote += other.m_wbreq_sent_remote;

    m_total_per_mem_instr_info.add(other.m_total_per_mem_instr_info);
}

void privateSharedMSIStatsPerTile::commit_per_mem_instr_stats(const privateSharedMSIStatsPerMemInstr& data) {
    if (data.m_did_core_miss) {
        if (data.m_is_read) {
            ++m_num_core_miss_read_instr;
        } else {
            ++m_num_core_miss_write_instr;
        }
    } else {
        if (data.m_is_read) {
            ++m_num_core_hit_read_instr;
        } else {
            ++m_num_core_hit_write_instr;
        }
    }
    if (data.m_num_inv_for_coherence_targets) {
        ++m_num_inv_for_coherence;
        m_num_inv_targets_for_coherence += data.m_num_inv_for_coherence_targets;
    }
    if (data.m_num_inv_for_evict_targets) {
        ++m_num_inv_for_evict;
        m_num_inv_targets_for_evict += data.m_num_inv_for_evict_targets;
    }
    if (data.m_did_reorder) {
        ++m_num_reorder;
    }
    m_total_per_mem_instr_info.add(data);
}

privateSharedMSIStats::privateSharedMSIStats(const uint64_t &t) : memStats(t) {}

privateSharedMSIStats::~privateSharedMSIStats() {}

void privateSharedMSIStats::print_stats(ostream &out) {
    
    char str[4096];
    privateSharedMSIStatsPerTile total_tile_info(0, system_time);

    memStats::print_stats(out);

    /* add privateSharedMSI-specific statistics */

    out << endl;
    
    out << "Private-L1 Shared-L2 MSI/MESI Statistics" << endl;
    out << "----------------------------------------" << endl;
    out << endl;

    perTileStats_t::iterator it;
    uint32_t num_tiles = 0;

    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        ++num_tiles;
        uint32_t id = it->first;
        std::shared_ptr<privateSharedMSIStatsPerTile> st = static_pointer_cast<privateSharedMSIStatsPerTile>(it->second);
        sprintf(str, "[P1S2MSIMESI:Core %d ] %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                     "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                     "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                     "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                     "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                     "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld "
                     "%ld %ld %ld %ld %ld %ld %ld %ld %ld",
                     id,
                     st->m_num_l1_read_instr,
                     st->m_num_l1_write_instr,
                     st->m_num_hits_for_read_instr_at_l1,
                     st->m_num_hits_for_write_instr_at_l1,
                     st->m_num_true_misses_for_read_instr_at_l1,
                     st->m_num_true_misses_for_write_instr_at_l1,
                     st->m_num_write_on_shared_misses_for_write_instr_at_l1,
                     st->m_num_l2_read_instr,
                     st->m_num_l2_write_instr,
                     st->m_num_hits_for_read_instr_at_l2,
                     st->m_num_hits_for_write_instr_at_l2,
                     st->m_num_true_misses_for_read_instr_at_l2,
                     st->m_num_true_misses_for_write_instr_at_l2,
                     st->m_num_write_on_shared_misses_for_write_instr_at_l2,
                     st->m_num_read_on_exclusive_misses_for_read_instr_at_l2,
                     st->m_num_write_on_exclusive_misses_for_write_instr_at_l2,
                     st->m_num_core_hit_read_instr,
                     st->m_num_core_hit_write_instr,
                     st->m_num_core_miss_read_instr,
                     st->m_num_core_miss_write_instr,
                     st->m_shreq_sent,
                     st->m_shreq_sent_remote,
                     st->m_exreq_sent,
                     st->m_exreq_sent_remote,
                     st->m_invrep_sent,
                     st->m_invrep_sent_remote,
                     st->m_flushrep_sent,
                     st->m_flushrep_sent_remote,
                     st->m_wbrep_sent,
                     st->m_wbrep_sent_remote,
                     st->m_shrep_sent,
                     st->m_shrep_sent_remote,
                     st->m_exrep_sent,
                     st->m_exrep_sent_remote,
                     st->m_invreq_sent,
                     st->m_invreq_sent_remote,
                     st->m_flushreq_sent,
                     st->m_flushreq_sent_remote,
                     st->m_wbreq_sent,
                     st->m_wbreq_sent_remote,
                     st->m_emptyreq_sent,
                     st->m_num_retry_for_update_at_l2,
                     st->m_num_evict_at_l1,
                     st->m_num_evict_at_l2,
                     st->m_num_writeback_at_l1,
                     st->m_num_writeback_at_l2,
                     st->m_num_inv_for_coherence,
                     st->m_num_inv_targets_for_coherence,
                     st->m_num_inv_for_evict,
                     st->m_num_inv_targets_for_evict,
                     st->m_num_reorder,
                     st->m_num_cat_action,
                     st->m_num_l1_action,
                     st->m_num_l2_action,
                     st->m_num_dram_action,
                     st->m_num_hits_for_read_instr_at_local_l2, 
                     st->m_num_hits_for_write_instr_at_local_l2, 
                     st->m_num_true_misses_for_read_instr_at_local_l2, 
                     st->m_num_true_misses_for_write_instr_at_local_l2, 
                     st->m_num_write_on_shared_misses_for_write_instr_at_local_l2, 
                     st->m_num_read_on_exclusive_misses_for_read_instr_at_local_l2, 
                     st->m_num_write_on_exclusive_misses_for_write_instr_at_local_l2, 
                     st->m_num_hits_for_read_instr_at_remote_l2, 
                     st->m_num_hits_for_write_instr_at_remote_l2, 
                     st->m_num_true_misses_for_read_instr_at_remote_l2, 
                     st->m_num_true_misses_for_write_instr_at_remote_l2, 
                     st->m_num_write_on_shared_misses_for_write_instr_at_remote_l2, 
                     st->m_num_read_on_exclusive_misses_for_read_instr_at_remote_l2, 
                     st->m_num_write_on_exclusive_misses_for_write_instr_at_remote_l2);

        out << str << endl;
        total_tile_info.add(*st);
    }

    out << endl;

    sprintf(str, "[P1S2MSIMESI:Summary A ] for each unique [instruction, core, cache level] pair\n"
                 "    -- L1 -- \n"
                 "    total-L1-read-instr-at-each-core %ld\n"
                 "    total-L1-write-instr-at-each-core %ld\n"
                 "    total-L1-read-instr-hits-at-each-core %ld\n"
                 "    total-L1-write-instr-hits-at-each-core %ld\n"
                 "    total-L1-read-instr-true-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-true-misses-at-each-core %ld\n"
                 "    total-L1-write-on-shared-misses-at-each-core %ld\n"
                 "    -- L2 -- \n"
                 "    total-L2-read-instr-at-each-core %ld\n"
                 "    total-L2-write-instr-at-each-core %ld\n"
                 "    total-L2-read-instr-hits-at-each-core %ld\n"
                 "    total-L2-write-instr-hits-at-each-core %ld\n"
                 "    total-L2-read-instr-true-misses-at-each-core %ld\n"
                 "    total-L2-write-instr-true-misses-at-each-core %ld\n"
                 "    total-L2-write-on-shared-misses-at-each-core %ld\n"
                 "    total-L2-read-on-exclusive-misses-at-each-core %ld\n"
                 "    total-L2-write-on-exclusive-misses-at-each-core %ld\n"
                 "[P1S2MSIMESI:Summary B ] for each instruction\n"
                 "    total-core-hit-read-instr %ld\n"
                 "    total-core-hit-write-instr %ld\n"
                 "    total-core-missed-read-instr %ld\n"
                 "    total-core-missed-write-instr %ld\n"
                 "[P1S2MSIMESI:Summary C ] at each core\n"
                 "    message-shReq-sent %ld (remote: %ld )\n"
                 "    message-exReq-sent %ld (remote: %ld )\n"
                 "    message-invRep-sent %ld (remote: %ld )\n"
                 "    message-flushRep-sent %ld (remote: %ld )\n"
                 "    message-wbRep-sent %ld (remote: %ld )\n"
                 "    message-shRep-sent %ld (remote: %ld )\n"
                 "    message-exRep-sent %ld (remote: %ld )\n"
                 "    message-invReq-sent %ld (remote: %ld )\n"
                 "    message-flushReq-sent %ld (remote: %ld )\n"
                 "    message-wbReq-sent %ld (remote: %ld )\n"
                 "    message-emptbReq-sent %ld\n"
                 "    total-retry-for-update-at-l2 %ld\n"
                 "    total-core-evict-L1 %ld\n"
                 "    total-core-evict-L2 %ld\n"
                 "    total-core-writeback-L1 %ld\n"
                 "    total-core-writeback-L2 %ld\n"
                 "    total-core-invalidation-for-coherence %ld\n"
                 "    total-core-invalidation-targets-for-coherence %ld\n"
                 "    total-core-invalidation-for-evict %ld\n"
                 "    total-core-invalidation-targets-for-evict %ld\n"
                 "    total-core-reordering %ld\n"
                 "    total-CAT-actions-at-each-core %ld\n"
                 "    total-L1-actions-at-each-core %ld\n"
                 "    total-L2-actions-at-each-core %ld\n"
                 "    total-offchip-DRAM-actions-at-each-core %ld\n"
                 "[P1S2MSIMESI:Summary D ] at each core\n"
                 "    -- Local L2 -- \n"
                 "    total-L2-read-instr-hits-at-local-core %ld\n"
                 "    total-L2-write-instr-hits-at-local-core %ld\n"
                 "    total-L2-read-instr-true-misses-at-local-core %ld\n"
                 "    total-L2-write-instr-true-misses-at-local-core %ld\n"
                 "    total-L2-write-on-shared-misses-at-local-core %ld\n"
                 "    total-L2-read-on-exclusive-misses-at-local-core %ld\n"
                 "    total-L2-write-on-exclusive-misses-at-local-core %ld\n"
                 "    -- Remote L2 -- \n"
                 "    total-L2-read-instr-hits-at-remote-core %ld\n"
                 "    total-L2-write-instr-hits-at-remote-core %ld\n"
                 "    total-L2-read-instr-true-misses-at-remote-core %ld\n"
                 "    total-L2-write-instr-true-misses-at-remote-core %ld\n"
                 "    total-L2-write-on-shared-misses-at-remote-core %ld\n"
                 "    total-L2-read-on-exclusive-misses-at-remote-core %ld\n"
                 "    total-L2-write-on-exclusive-misses-at-remote-core %ld\n",
            total_tile_info.m_num_l1_read_instr,
            total_tile_info.m_num_l1_write_instr,
            total_tile_info.m_num_hits_for_read_instr_at_l1,
            total_tile_info.m_num_hits_for_write_instr_at_l1,
            total_tile_info.m_num_true_misses_for_read_instr_at_l1,
            total_tile_info.m_num_true_misses_for_write_instr_at_l1,
            total_tile_info.m_num_write_on_shared_misses_for_write_instr_at_l1,
            total_tile_info.m_num_l2_read_instr,
            total_tile_info.m_num_l2_write_instr,
            total_tile_info.m_num_hits_for_read_instr_at_l2,
            total_tile_info.m_num_hits_for_write_instr_at_l2,
            total_tile_info.m_num_true_misses_for_read_instr_at_l2,
            total_tile_info.m_num_true_misses_for_write_instr_at_l2,
            total_tile_info.m_num_write_on_shared_misses_for_write_instr_at_l2,
            total_tile_info.m_num_read_on_exclusive_misses_for_read_instr_at_l2,
            total_tile_info.m_num_write_on_exclusive_misses_for_write_instr_at_l2,
            total_tile_info.m_num_core_hit_read_instr,
            total_tile_info.m_num_core_hit_write_instr,
            total_tile_info.m_num_core_miss_read_instr,
            total_tile_info.m_num_core_miss_write_instr,
            total_tile_info.m_shreq_sent,
            total_tile_info.m_shreq_sent_remote,
            total_tile_info.m_exreq_sent,
            total_tile_info.m_exreq_sent_remote,
            total_tile_info.m_invrep_sent,
            total_tile_info.m_invrep_sent_remote,
            total_tile_info.m_flushrep_sent,
            total_tile_info.m_flushrep_sent_remote,
            total_tile_info.m_wbrep_sent,
            total_tile_info.m_wbrep_sent_remote,
            total_tile_info.m_shrep_sent,
            total_tile_info.m_shrep_sent_remote,
            total_tile_info.m_exrep_sent,
            total_tile_info.m_exrep_sent_remote,
            total_tile_info.m_invreq_sent,
            total_tile_info.m_invreq_sent_remote,
            total_tile_info.m_flushreq_sent,
            total_tile_info.m_flushreq_sent_remote,
            total_tile_info.m_wbreq_sent,
            total_tile_info.m_wbreq_sent_remote,
            total_tile_info.m_emptyreq_sent,
            total_tile_info.m_num_retry_for_update_at_l2,
            total_tile_info.m_num_evict_at_l1,
            total_tile_info.m_num_evict_at_l2,
            total_tile_info.m_num_writeback_at_l1,
            total_tile_info.m_num_writeback_at_l2,
            total_tile_info.m_num_inv_for_coherence,
            total_tile_info.m_num_inv_targets_for_coherence,
            total_tile_info.m_num_inv_for_evict,
            total_tile_info.m_num_inv_targets_for_evict,
            total_tile_info.m_num_reorder,
            total_tile_info.m_num_cat_action,
            total_tile_info.m_num_l1_action,
            total_tile_info.m_num_l2_action,
            total_tile_info.m_num_dram_action,
            total_tile_info.m_num_hits_for_read_instr_at_local_l2, 
            total_tile_info.m_num_hits_for_write_instr_at_local_l2, 
            total_tile_info.m_num_true_misses_for_read_instr_at_local_l2, 
            total_tile_info.m_num_true_misses_for_write_instr_at_local_l2, 
            total_tile_info.m_num_write_on_shared_misses_for_write_instr_at_local_l2, 
            total_tile_info.m_num_read_on_exclusive_misses_for_read_instr_at_local_l2, 
            total_tile_info.m_num_write_on_exclusive_misses_for_write_instr_at_local_l2, 
            total_tile_info.m_num_hits_for_read_instr_at_remote_l2, 
            total_tile_info.m_num_hits_for_write_instr_at_remote_l2, 
            total_tile_info.m_num_true_misses_for_read_instr_at_remote_l2, 
            total_tile_info.m_num_true_misses_for_write_instr_at_remote_l2, 
            total_tile_info.m_num_write_on_shared_misses_for_write_instr_at_remote_l2, 
            total_tile_info.m_num_read_on_exclusive_misses_for_read_instr_at_remote_l2, 
            total_tile_info.m_num_write_on_exclusive_misses_for_write_instr_at_remote_l2);

    out << str << endl;

    sprintf(str, "[P1S2MSIMESI Latency Breakdown ]\n"
                 "    total-memory-serialization %ld\n"
                 "    total-outstanding-CAT-serialization %ld\n"
                 "    total-outstanding-CAT-operation %ld\n"
                 "    total-outstanding-L1-serialization %ld\n"
                 "    total-outstanding-L1-operation %ld\n"
                 "    total-cache-request-network-and-serialization %ld\n"
                 "    total-directory-reply-network-and-serialization %ld\n"
                 "    total-cache-reply-for-S-to-E-serialization %ld\n"
                 "    total-cache-reply-for-evict-serialization %ld\n"
                 "    total-outstanding-L2-serialization %ld\n"
                 "    total-outstanding-L2-operation %ld\n"
                 "    total-outstanding-invalidation-for-coherence %ld\n"
                 "    total-outstanding-empty-req-serialization %ld\n"
                 "    total-outstanding-invalidation-for-evict %ld\n"
                 "    total-outstanding-reorder-delay %ld\n"
                 "    total-outstanding-DRAMctrl-request-network-and-serialization %ld\n"
                 "    total-outstanding-DRAMctrl-reply-network-and-serialization %ld\n"
                 "    total-outstanding-DRAM-operation %ld\n"
                 "    total-migration_latency %ld\n"
                 "    \n"
                 "    (L1 serialization & operation categories)\n"
                 "        L1-cost-for-hits %ld \n"
                 "        L1-cost-for-true-miss %ld \n"
                 "        L1-cost-for-write-on-shared-miss %ld\n"
                 "        L1-cost-for-feed %ld\n"
                 "    (L2 serialization & operation categories)\n"
                 "        L2-cost-for-local-hits %ld\n"
                 "        L2-cost-for-local-true-misses %ld\n"
                 "        L2-cost-for-local-write-on-shared-misses %ld\n"
                 "        L2-cost-for-local-read-on-exclusive-misses %ld\n"
                 "        L2-cost-for-local-write-on-exclusive-misses %ld\n"
                 "        L2-cost-for-local-directory-update %ld\n"
                 "        L2-cost-for-local-writeback %ld\n"
                 "        L2-cost-for-local-evict %ld\n"
                 "        L2-cost-for-local-feed %ld\n"
                 "        L2-cost-for-local-feed-retry %ld\n"
                 "        local-outstanding-invalidation-for-coherence %ld\n"
                 "        local-outstanding-empty-req-serialization %ld\n"
                 "        local-outstanding-invalidation-for-evict %ld\n"
                 "        local-outstanding-reorder-delay %ld\n"
                 "        L2-cost-for-remote-hits %ld\n"
                 "        L2-cost-for-remote-true-misses %ld\n"
                 "        L2-cost-for-remote-write-on-shared-misses %ld\n"
                 "        L2-cost-for-remote-read-on-exclusive-misses %ld\n"
                 "        L2-cost-for-remote-write-on-exclusive-misses %ld\n"
                 "        L2-cost-for-remote-directory-update %ld\n"
                 "        L2-cost-for-remote-writeback %ld\n"
                 "        L2-cost-for-remote-evict %ld\n"
                 "        L2-cost-for-remote-feed %ld\n"
                 "        L2-cost-for-remote-feed-retry %ld\n"
                 "        remote-outstanding-invalidation-for-coherence %ld\n"
                 "        remote-outstanding-empty-req-serialization %ld\n"
                 "        remote-outstanding-invalidation-for-evict %ld\n"
                 "        remote-outstanding-reorder-delay %ld\n",
            total_tile_info.m_total_per_mem_instr_info.m_mem_srz,
            total_tile_info.m_total_per_mem_instr_info.m_cat_srz,
            total_tile_info.m_total_per_mem_instr_info.m_cat_ops,
            total_tile_info.m_total_per_mem_instr_info.m_l1_srz,
            total_tile_info.m_total_per_mem_instr_info.m_l1_ops,
            total_tile_info.m_total_per_mem_instr_info.m_cache_req_nas,
            total_tile_info.m_total_per_mem_instr_info.m_dir_rep_nas,
            total_tile_info.m_total_per_mem_instr_info.m_cache_rep_srz_for_switch,
            total_tile_info.m_total_per_mem_instr_info.m_cache_rep_srz_for_evict,
            total_tile_info.m_total_per_mem_instr_info.m_l2_srz,
            total_tile_info.m_total_per_mem_instr_info.m_l2_ops,
            total_tile_info.m_total_per_mem_instr_info.m_inv_for_coherence,
            total_tile_info.m_total_per_mem_instr_info.m_empty_req_srz,
            total_tile_info.m_total_per_mem_instr_info.m_inv_for_evict,
            total_tile_info.m_total_per_mem_instr_info.m_reorder,
            total_tile_info.m_total_per_mem_instr_info.m_dramctrl_req_nas,
            total_tile_info.m_total_per_mem_instr_info.m_dramctrl_rep_nas,
            total_tile_info.m_total_per_mem_instr_info.m_dram_ops,
            total_tile_info.m_total_per_mem_instr_info.m_mig,
            total_tile_info.m_total_per_mem_instr_info.m_l1_cost_for_hit,
            total_tile_info.m_total_per_mem_instr_info.m_l1_cost_for_true_miss,
            total_tile_info.m_total_per_mem_instr_info.m_l1_cost_for_write_on_shared,
            total_tile_info.m_total_per_mem_instr_info.m_l1_cost_for_feed,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_hit,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_true_miss,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_write_on_shared_miss,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_read_on_exclusive_miss,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_write_on_exclusive_miss,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_directory_update,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_writeback,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_evict,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_feed,
            total_tile_info.m_total_per_mem_instr_info.m_local_l2_cost_for_feed_retry,
            total_tile_info.m_total_per_mem_instr_info.m_local_inv_for_coherence,
            total_tile_info.m_total_per_mem_instr_info.m_local_empty_req_srz,
            total_tile_info.m_total_per_mem_instr_info.m_local_inv_for_evict,
            total_tile_info.m_total_per_mem_instr_info.m_local_reorder,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_hit,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_true_miss,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_write_on_shared_miss,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_read_on_exclusive_miss,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_write_on_exclusive_miss,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_directory_update,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_writeback,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_evict,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_feed,
            total_tile_info.m_total_per_mem_instr_info.m_remote_l2_cost_for_feed_retry,
            total_tile_info.m_total_per_mem_instr_info.m_remote_inv_for_coherence,
            total_tile_info.m_total_per_mem_instr_info.m_remote_empty_req_srz,
            total_tile_info.m_total_per_mem_instr_info.m_remote_inv_for_evict,
            total_tile_info.m_total_per_mem_instr_info.m_remote_reorder);

    out << str << endl;
    out << endl;
}
    

