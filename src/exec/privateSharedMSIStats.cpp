// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSIStats.hpp"

privateSharedMSIStatsPerTile::privateSharedMSIStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t), 
    /* cost breakdown study */
    m_memory_subsystem_serialization_cost(0),
    m_cat_serialization_cost(0), m_cat_action_cost(0),
    m_l1_serialization_cost(0), m_l1_action_cost(0), m_l1_eviction_cost(0),
    m_l2_network_plus_serialization_cost(0), m_l2_action_cost(0), m_l2_invalidation_cost(0),
    m_dram_network_plus_serialization_cost(0), m_dram_offchip_network_plus_dram_action_cost(0),
    m_l1_action(0), m_l2_action(0),
    m_i_s(0), m_i_e(0), m_s_s(0), m_s_e(0), m_e_s(0), m_e_e(0), 
    m_shreq(0), m_exreq(0), m_invrep(0), m_flushrep(0), m_wbrep(0), m_shrep(0), m_exrep(0),
    m_invreq(0), m_invreq_replied(0), m_wbreq(0), m_wbreq_replied(0) { }

privateSharedMSIStatsPerTile::~privateSharedMSIStatsPerTile() {}

void privateSharedMSIStatsPerTile::did_read_l1(bool hit) {
    m_l1_read_hits.add(hit? 1:0, 1);
}

void privateSharedMSIStatsPerTile::did_access_l2(bool hit) {
    m_l2_hits.add(hit? 1:0, 1);
}

void privateSharedMSIStatsPerTile::did_write_l1(bool hit) {
    m_l1_write_hits.add(hit? 1:0, 1);
}

void privateSharedMSIStatsPerTile::did_invalidate_caches(uint32_t num_caches, uint64_t latency) {
    m_invalidated_caches.add(num_caches, 1);
    m_invalidate_penalties.add(latency, 1);
} 

void privateSharedMSIStatsPerTile::did_read_cat(bool hit) {
    m_directory_hits.add(hit? 1:0, 1);
}

privateSharedMSIStats::privateSharedMSIStats(const uint64_t &t) : memStats(t) {}

privateSharedMSIStats::~privateSharedMSIStats() {}

void privateSharedMSIStats::print_stats(ostream &out) {

    memStats::print_stats(out);

    /* add privateSharedMSI-specific statistics */

    out << endl;
    
    out << "Private-L1 Shared-L2 MCC Statistics" << endl;
    out << "-----------------------------------" << endl;
    out << endl;

    uint64_t total_l1_reads = 0;
    uint64_t total_l1_writes = 0;
    uint64_t total_l1 = 0;
    uint64_t total_l2 = 0;
    uint64_t total_cat_lookups = 0;
    uint64_t total_invs = 0;

    double total_l1_read_hits = 0.0;
    double total_l1_write_hits = 0.0;
    double total_l1_hits = 0.0;
    double total_l2_hits = 0.0;
    double total_inv_caches = 0.0;
    double total_inv_penalty = 0.0;
    double total_cat_hits = 0.0;

    /* cost breakdown study */
    uint64_t total_memory_subsystem_serialization_cost = 0;
    uint64_t total_cat_serialization_cost = 0;
    uint64_t total_cat_action_cost = 0;
    uint64_t total_l1_serialization_cost = 0;
    uint64_t total_l1_action_cost = 0;
    uint64_t total_l1_eviction_cost = 0;
    uint64_t total_l2_network_plus_serialization_cost = 0;
    uint64_t total_l2_action_cost = 0;
    uint64_t total_l2_invalidation_cost = 0;
    uint64_t total_dram_network_plus_serialization_cost = 0;
    uint64_t total_dram_offchip_network_plus_dram_action_cost = 0;
    uint64_t total_l1_action = 0;
    uint64_t total_l2_action = 0;

    uint64_t total_i_s = 0;
    uint64_t total_i_e = 0;
    uint64_t total_s_s = 0;
    uint64_t total_s_e = 0;
    uint64_t total_e_s = 0;
    uint64_t total_e_e = 0;
    
    uint64_t total_shreq = 0;
    uint64_t total_exreq = 0;
    uint64_t total_invrep = 0;
    uint64_t total_flushrep = 0;
    uint64_t total_wbrep = 0;
    uint64_t total_shrep = 0;
    uint64_t total_exrep = 0;
    uint64_t total_invreq = 0;
    uint64_t total_invreq_replied = 0;
    uint64_t total_flushreq = 0;
    uint64_t total_flushreq_replied = 0;
    uint64_t total_wbreq = 0;
    uint64_t total_wbreq_replied = 0;

    perTileStats_t::iterator it;
    uint32_t num_tiles = 0;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        ++num_tiles;
        uint32_t id = it->first;
        shared_ptr<privateSharedMSIStatsPerTile> st = static_pointer_cast<privateSharedMSIStatsPerTile>(it->second);

        uint64_t l1_reads = st->m_l1_read_hits.sample_count();
        uint64_t l1_writes = st->m_l1_write_hits.sample_count();
        uint64_t l1 = l1_reads + l1_writes;
        uint64_t l2 = st->m_l2_hits.sample_count();
        total_l1_reads += l1_reads;
        total_l1_writes += l1_writes;
        total_l1 += l1;
        total_l2 += l2;

        double l1_read_rate = st->m_l1_read_hits.get_mean();
        double l1_read_hits = l1_read_rate * l1_reads;
        double l1_write_rate = st->m_l1_write_hits.get_mean();
        double l1_write_hits = l1_write_rate * l1_writes;
        double l1_hits = l1_read_hits + l1_write_hits;
        double l1_rate = l1_hits / l1;
        double l2_rate = st->m_l2_hits.get_mean();
        double l2_hits = l2_rate * l2;
        total_l1_read_hits += l1_read_hits;
        total_l1_write_hits += l1_write_hits;
        total_l1_hits += l1_hits;
        total_l2_hits += l2_hits;

        uint64_t cat_lookups = st->m_directory_hits.sample_count();
        double cat_rate = st->m_directory_hits.get_mean();
        double cat_hits = cat_rate * cat_lookups;
        total_cat_lookups += cat_lookups;
        total_cat_hits += cat_hits;

        uint64_t invs = st->m_invalidated_caches.sample_count();
        double avg_inv_caches = st->m_invalidated_caches.get_mean();
        double inv_caches = avg_inv_caches * invs;
        double avg_inv_penalty = st->m_invalidate_penalties.get_mean();
        double inv_penalty = avg_inv_penalty * invs;
        total_invs += invs;
        total_inv_caches += inv_caches;
        total_inv_penalty += inv_penalty;

        /* cost breakdown study */
        total_memory_subsystem_serialization_cost += st->m_memory_subsystem_serialization_cost;
        total_cat_serialization_cost += st->m_cat_serialization_cost;
        total_cat_action_cost += st->m_cat_action_cost;
        total_l1_serialization_cost += st->m_l1_serialization_cost;
        total_l1_action_cost += st->m_l1_action_cost;
        total_l1_eviction_cost += st->m_l1_eviction_cost;
        total_l2_network_plus_serialization_cost += st->m_l2_network_plus_serialization_cost;
        total_l2_action_cost += st->m_l2_action_cost;
        total_l2_invalidation_cost += st->m_l2_invalidation_cost;
        total_dram_network_plus_serialization_cost += st->m_dram_network_plus_serialization_cost;
        total_dram_offchip_network_plus_dram_action_cost += st->m_dram_offchip_network_plus_dram_action_cost;
        total_l1_action += st->m_l1_action;
        total_l2_action += st->m_l2_action;

        total_i_s += st->m_i_s;
        total_i_e += st->m_i_e;
        total_s_s += st->m_s_s;
        total_s_e += st->m_s_e;
        total_e_s += st->m_e_s;
        total_e_e += st->m_e_e;

        total_shreq += st->m_shreq;
        total_exreq += st->m_exreq;
        total_invrep += st->m_invrep;
        total_flushrep += st->m_flushrep;
        total_wbrep += st->m_wbrep;
        total_shrep += st->m_shrep;
        total_exrep += st->m_exrep;
        total_invreq += st->m_invreq;
        total_invreq_replied += st->m_invreq_replied;
        total_flushreq += st->m_flushreq;
        total_flushreq_replied += st->m_flushreq_replied;
        total_wbreq += st->m_wbreq;
        total_wbreq_replied += st->m_wbreq_replied;

        char str[1024];
        sprintf(str, "[Private-shared-MSI/MESI %4d ] %ld reads %ld writes L1hit%%: %.2f read%%: %.2f write%%: %.2f "
                     "L2hit%%: %.2f invs: %ld #targets/inv: %.2f avg-inv-latency: %.2f CAThit%%: %.2f L1ops: %ld L2ops: %ld",
                     id, l1_reads, l1_writes, 100.0*l1_rate, 100.0*l1_read_rate, 100.0*l1_write_rate, 
                     100.0*l2_rate, invs, avg_inv_caches, avg_inv_penalty, 100.0*cat_rate, st->m_l1_action, st->m_l2_action);

        out << str << endl;

    }

    out << endl;

    char str[1024];
    sprintf(str, "[Summary: Private-shared-MSI/MESI ] %ld reads %ld writes L1hit%%: %.2f read%%: %.2f write%%: %.2f "
            "L2hit%%: %.2f invs: %ld total-inv-targets: %.0f total-inv-latency: %.0f CAThit%%: %.2f L1ops: %ld L2ops: %ld",
            total_l1_reads, total_l1_writes,
            100.0*total_l1_hits/total_l1,
            100.0*total_l1_read_hits/total_l1_reads,
            100.0*total_l1_write_hits/total_l1_writes,
            100.0*total_l2_hits/total_l2,
            /* these include invalidations for evictions */
            total_invs,
            total_inv_caches, 
            total_inv_penalty,
            100.0*total_cat_hits/total_cat_lookups, total_l1_action, total_l2_action);

    out << str << endl;

    /* cost breakdown study */
    sprintf(str, "[Latency Breakdown ] MEM-S: %ld L1-N&S: %ld L1: %ld L1-evict: %ld CAT-S: %ld CAT: %ld L2-N&S: %ld L2-Inv: %ld L2: %ld "
                 "DRAM-N&S: %ld DRAM-Offchip: %ld", total_memory_subsystem_serialization_cost,
                 total_l1_serialization_cost, total_l1_action_cost, total_l1_eviction_cost,
                 total_cat_serialization_cost, total_cat_action_cost,
                 total_l2_network_plus_serialization_cost, total_l2_invalidation_cost, total_l2_action_cost, 
                 total_dram_network_plus_serialization_cost, total_dram_offchip_network_plus_dram_action_cost);
    
    out << str << endl;

    sprintf(str, "[State Transitions on Requests ] I->S: %ld I->E: %ld S->S: %ld S->E: %ld E->S: %ld E->E: %ld",
                 total_i_s, total_i_e, total_s_s, total_s_e, total_e_s, total_e_e);

    out << str << endl;

    sprintf(str, "[Coherence Messages 1 ] shReq: %ld exReq: %ld invRep: %ld (on requests: %ld) flushRep: %ld (on requests: %ld) "
                 "wbRep: %ld (on requests %ld)",
                 total_shreq, total_exreq, total_invrep, total_invreq_replied, total_flushrep, total_flushreq_replied,
                 total_wbrep, total_wbreq_replied);

    out << str << endl;

    sprintf(str, "[Coherence Messages 2 ] shRep: %ld exRep: %ld invReq: %ld (replied: %ld) wbReq: %ld (replied: %ld) "
                 "flushReq: %ld (replied: %ld)",
                 total_shrep, total_exrep, total_invreq, total_invreq_replied, total_wbreq, total_wbreq_replied,
                 total_flushreq, total_flushreq_replied);

    out << str << endl;

    out << endl;

}
