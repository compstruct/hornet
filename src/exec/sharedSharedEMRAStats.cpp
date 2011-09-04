// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "sharedSharedEMRAStats.hpp"

sharedSharedEMRAStatsPerMemInstr::sharedSharedEMRAStatsPerMemInstr() :
    m_did_core_miss(false),
    m_did_migrate(false),
    m_mig_depart_time(0),
    m_mem_srz(0),
    m_l1_srz(0),
    m_l1_ops(0),
    m_cat_srz(0),
    m_cat_ops(0),
    m_ra_req_nas(0),
    m_ra_rep_nas(0),
    m_l2_srz(0),
    m_l2_ops(0),
    m_dramctrl_req_nas(0),
    m_dramctrl_rep_nas(0),
    m_dram_ops(0),
    m_mig(0)
{}

sharedSharedEMRAStatsPerMemInstr::~sharedSharedEMRAStatsPerMemInstr() {}

void sharedSharedEMRAStatsPerMemInstr::add(const sharedSharedEMRAStatsPerMemInstr& other) {
    m_mem_srz += other.m_mem_srz;
    m_l1_srz += other.m_l1_srz;
    m_l1_ops += other.m_l1_ops;
    m_cat_srz += other.m_cat_srz;
    m_cat_ops += other.m_cat_ops;
    m_ra_req_nas += other.m_ra_req_nas;
    m_ra_rep_nas += other.m_ra_rep_nas;
    m_l2_srz += other.m_l2_srz;
    m_l2_ops += other.m_l2_ops;
    m_dramctrl_req_nas += other.m_dramctrl_req_nas;
    m_dramctrl_rep_nas += other.m_dramctrl_rep_nas;
    m_dram_ops += other.m_dram_ops;
    m_mig += other.m_mig;
}

uint64_t sharedSharedEMRAStatsPerMemInstr::total_cost() { 
    return m_mem_srz + m_l1_srz + m_l1_ops + m_cat_srz + m_cat_ops + m_ra_req_nas + m_ra_rep_nas + m_l2_srz
           + m_l2_ops + m_dramctrl_req_nas + m_dramctrl_rep_nas + m_dram_ops + m_mig;
}

bool sharedSharedEMRAStatsPerMemInstr::add_new_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        return false;
    }
    shared_ptr<sharedSharedEMRAStatsPerMemInstr> new_tentative_set(new sharedSharedEMRAStatsPerMemInstr());
    m_tentative_data[index] = new_tentative_set;
    return true;
}

shared_ptr<sharedSharedEMRAStatsPerMemInstr> sharedSharedEMRAStatsPerMemInstr::get_tentative_data(int index) {
    if (!m_tentative_data.count(index)) {
        add_new_tentative_data(index);
    }
    return m_tentative_data[index];
}

void sharedSharedEMRAStatsPerMemInstr::discard_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        m_tentative_data.erase(index);
    } 
}

void sharedSharedEMRAStatsPerMemInstr::commit_tentative_data(int index) {
    if (m_tentative_data.count(index)) {
        add(*m_tentative_data[index]);
    }
    m_tentative_data.clear();
}

void sharedSharedEMRAStatsPerMemInstr::commit_max_tentative_data() {
    uint64_t value = 0;
    map<int, shared_ptr<sharedSharedEMRAStatsPerMemInstr> >::iterator it;
    shared_ptr<sharedSharedEMRAStatsPerMemInstr> max = shared_ptr<sharedSharedEMRAStatsPerMemInstr>();
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
        shared_ptr<sharedSharedEMRAStatsPerMemInstr> cur = it->second;
        if (cur->total_cost() > value) {
            value = cur->total_cost();
            max = cur;
        }
    }

    if (max) {
        add(*max);
        m_tentative_data.clear();
    }
}

void sharedSharedEMRAStatsPerMemInstr::commit_min_tentative_data() {
    uint64_t value = UINT64_MAX;
    map<int, shared_ptr<sharedSharedEMRAStatsPerMemInstr> >::iterator it;
    shared_ptr<sharedSharedEMRAStatsPerMemInstr> min = shared_ptr<sharedSharedEMRAStatsPerMemInstr>();
    for (it = m_tentative_data.begin(); it != m_tentative_data.end(); ++it) {
        shared_ptr<sharedSharedEMRAStatsPerMemInstr> cur = it->second;
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

void sharedSharedEMRAStatsPerMemInstr::apply_mig_latency(const uint64_t cur_time) {
    if (m_did_migrate) {
        m_mig += cur_time - m_mig_depart_time;
        m_did_migrate = false;
        m_mig_depart_time = 0;
    }
}

sharedSharedEMRAStatsPerTile::sharedSharedEMRAStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t),
    m_num_l1_read_instr(0), 
    m_num_l1_write_instr(0), 
    m_num_l2_read_instr(0), 
    m_num_l2_write_instr(0), 
    m_num_hits_for_l1_read_instr(0), 
    m_num_hits_for_l1_write_instr(0), 
    m_num_hits_for_l2_read_instr(0), 
    m_num_hits_for_l2_write_instr(0), 
    m_num_core_misses_for_l1_read_instr(0), 
    m_num_core_misses_for_l1_write_instr(0), 
    m_num_true_misses_for_l1_read_instr(0), 
    m_num_true_misses_for_l1_write_instr(0), 
    m_num_true_misses_for_l2_read_instr(0), 
    m_num_true_misses_for_l2_write_instr(0), 
    m_num_core_hit_instr(0),
    m_num_core_miss_instr(0),
    m_num_cat_action(0),
    m_num_l1_action(0),
    m_num_l2_action(0),
    m_num_dram_action(0)
{}

sharedSharedEMRAStatsPerTile::~sharedSharedEMRAStatsPerTile() {}

void sharedSharedEMRAStatsPerTile::add(const sharedSharedEMRAStatsPerTile& other) {
    m_num_l1_read_instr += other.m_num_l1_read_instr; 
    m_num_l1_write_instr += other.m_num_l1_write_instr; 
    m_num_l2_read_instr += other.m_num_l2_read_instr; 
    m_num_l2_write_instr += other.m_num_l2_write_instr; 
    m_num_hits_for_l1_read_instr += other.m_num_hits_for_l1_read_instr; 
    m_num_hits_for_l1_write_instr += other.m_num_hits_for_l1_write_instr; 
    m_num_hits_for_l2_read_instr += other.m_num_hits_for_l2_read_instr; 
    m_num_hits_for_l2_write_instr += other.m_num_hits_for_l2_write_instr; 
    m_num_core_misses_for_l1_read_instr += other.m_num_core_misses_for_l1_read_instr; 
    m_num_core_misses_for_l1_write_instr += other.m_num_core_misses_for_l1_write_instr; 
    m_num_true_misses_for_l1_read_instr += other.m_num_true_misses_for_l1_read_instr; 
    m_num_true_misses_for_l1_write_instr += other.m_num_true_misses_for_l1_write_instr; 
    m_num_true_misses_for_l2_read_instr += other.m_num_true_misses_for_l2_read_instr; 
    m_num_true_misses_for_l2_write_instr += other.m_num_true_misses_for_l2_write_instr; 
    m_num_core_hit_instr += other.m_num_core_hit_instr;
    m_num_core_miss_instr += other.m_num_core_miss_instr;
    m_num_cat_action += other.m_num_cat_action;
    m_num_l1_action += other.m_num_l1_action;
    m_num_l2_action += other.m_num_l2_action;
    m_num_dram_action += other.m_num_dram_action;

    m_total_per_mem_instr_info.add(other.m_total_per_mem_instr_info);
}

void sharedSharedEMRAStatsPerTile::commit_per_mem_instr_stats(const sharedSharedEMRAStatsPerMemInstr& data) {

    if (data.m_did_core_miss) {
        ++m_num_core_miss_instr;
    } else {
        ++m_num_core_hit_instr;
    }

    m_total_per_mem_instr_info.add(data);

}

sharedSharedEMRAStats::sharedSharedEMRAStats(const uint64_t &t) : memStats(t) {}

sharedSharedEMRAStats::~sharedSharedEMRAStats() {}

void sharedSharedEMRAStats::print_stats(ostream &out) {

    char str[1024];
    sharedSharedEMRAStatsPerTile total_tile_info(0, system_time);

    memStats::print_stats(out);

    /* add sharedSharedEMRA-specific statistics */

    out << endl;
    
    out << "Shared-L1 Shared-L2 EMRA Statistics" << endl;
    out << "-----------------------------------" << endl;
    out << endl;

    perTileStats_t::iterator it;
    uint32_t num_tiles = 0;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        ++num_tiles;
        uint32_t id = it->first;
        shared_ptr<sharedSharedEMRAStatsPerTile> st = static_pointer_cast<sharedSharedEMRAStatsPerTile>(it->second);

        sprintf(str, "[S1S2EMRA:Core %d ] %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                id,
                st->m_num_l1_read_instr, 
                st->m_num_l1_write_instr, 
                st->m_num_l2_read_instr, 
                st->m_num_l2_write_instr, 
                st->m_num_hits_for_l1_read_instr, 
                st->m_num_hits_for_l1_write_instr, 
                st->m_num_hits_for_l2_read_instr, 
                st->m_num_hits_for_l2_write_instr, 
                st->m_num_core_misses_for_l1_read_instr, 
                st->m_num_core_misses_for_l1_write_instr, 
                st->m_num_true_misses_for_l1_read_instr, 
                st->m_num_true_misses_for_l1_write_instr, 
                st->m_num_true_misses_for_l2_read_instr, 
                st->m_num_true_misses_for_l2_write_instr, 
                st->m_num_core_hit_instr,
                st->m_num_core_miss_instr,
                st->m_num_cat_action,
                st->m_num_l1_action,
                st->m_num_l2_action,
                st->m_num_dram_action);

        out << str << endl;

        total_tile_info.add(*st);

    }

    out << endl;

    sprintf(str, "[S1S2EMRA:Summary A ] for each unique [instruction, core, cache level] pair\n"
                 "    total-L1-read-instr-at-each-core %ld\n"
                 "    total-L1-write-instr-at-each-core %ld\n"
                 "    total-L2-read-instr-at-each-core %ld\n"
                 "    total-L2-write-instr-at-each-core %ld\n"
                 "    total-L1-read-instr-hits-at-each-core %ld\n"
                 "    total-L1-write-instr-hits-at-each-core %ld\n"
                 "    total-L2-read-instr-hits-at-each-core %ld\n"
                 "    total-L2-write-instr-hits-at-each-core %ld\n"
                 "    total-L1-read-instr-core-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-core-misses-at-each-core %ld\n"
                 "    total-L1-read-instr-true-misses-at-each-core %ld\n"
                 "    total-L1-write-instr-true-misses-at-each-core %ld\n"
                 "    total-L2-read-instr-true-misses-at-each-core %ld\n"
                 "    total-L2-write-instr-true-misses-at-each-core %ld\n"
                 "[S1S2EMRA:Summary B ] for each instruction\n"
                 "    total-core-hits-instr %ld\n"
                 "    total-core-missed-instr %ld\n"
                 "[S1S2EMRA:Summary C ] at each core\n"
                 "    total-CAT-actions-at-each-core %ld\n"
                 "    total-L1-actions-at-each-core %ld\n"
                 "    total-L2-actions-at-each-core %ld\n"
                 "    total-offchip-DRAM-actions-at-each-core %ld\n",
            total_tile_info.m_num_l1_read_instr, 
            total_tile_info.m_num_l1_write_instr, 
            total_tile_info.m_num_l2_read_instr, 
            total_tile_info.m_num_l2_write_instr, 
            total_tile_info.m_num_hits_for_l1_read_instr, 
            total_tile_info.m_num_hits_for_l1_write_instr, 
            total_tile_info.m_num_hits_for_l2_read_instr, 
            total_tile_info.m_num_hits_for_l2_write_instr, 
            total_tile_info.m_num_core_misses_for_l1_read_instr, 
            total_tile_info.m_num_core_misses_for_l1_write_instr, 
            total_tile_info.m_num_true_misses_for_l1_read_instr, 
            total_tile_info.m_num_true_misses_for_l1_write_instr, 
            total_tile_info.m_num_true_misses_for_l2_read_instr, 
            total_tile_info.m_num_true_misses_for_l2_write_instr, 
            total_tile_info.m_num_core_hit_instr,
            total_tile_info.m_num_core_miss_instr,
            total_tile_info.m_num_cat_action,
            total_tile_info.m_num_l1_action,
            total_tile_info.m_num_l2_action,
            total_tile_info.m_num_dram_action);

    out << str << endl;

    out << endl;

    sprintf(str, "[S1S2EMRA Latency Breakdown ]\n"
            "    total-memory-serialization %ld\n"
            "    total-outstanding-CAT-serialization %ld\n"
            "    total-outstanding-CAT-operation %ld\n"
            "    total-outstanding-L1-serialization %ld\n"
            "    total-outstanding-L1-operation %ld\n"
            "    total-RA-request-network-and-serialization %ld\n"
            "    total-RA-reply-network-and-serialization %ld\n"
            "    total-outstanding-L2-serialization %ld\n"
            "    total-outstanding-L2-operation %ld\n"
            "    total-outstanding-DRAMctrl-request-network-and-serialization %ld\n"
            "    total-outstanding-DRAMctrl-reply-network-and-serialization %ld\n"
            "    total-outstanding-DRAM-operation %ld\n"
            "    total-migration_latency %ld\n",
            total_tile_info.m_total_per_mem_instr_info.m_mem_srz,
            total_tile_info.m_total_per_mem_instr_info.m_cat_srz,
            total_tile_info.m_total_per_mem_instr_info.m_cat_ops,
            total_tile_info.m_total_per_mem_instr_info.m_l1_srz,
            total_tile_info.m_total_per_mem_instr_info.m_l1_ops,
            total_tile_info.m_total_per_mem_instr_info.m_ra_req_nas,
            total_tile_info.m_total_per_mem_instr_info.m_ra_rep_nas,
            total_tile_info.m_total_per_mem_instr_info.m_l2_srz,
            total_tile_info.m_total_per_mem_instr_info.m_l2_ops,
            total_tile_info.m_total_per_mem_instr_info.m_dramctrl_req_nas,
            total_tile_info.m_total_per_mem_instr_info.m_dramctrl_rep_nas,
            total_tile_info.m_total_per_mem_instr_info.m_dram_ops,
            total_tile_info.m_total_per_mem_instr_info.m_mig);

    out << str << endl;

    out << endl;

}
