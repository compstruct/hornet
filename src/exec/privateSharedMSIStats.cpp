// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "privateSharedMSIStats.hpp"

privateSharedMSIStatsPerTile::privateSharedMSIStatsPerTile(uint32_t id, const uint64_t &t) :
    memStatsPerTile(id, t) { }

privateSharedMSIStatsPerTile::~privateSharedMSIStatsPerTile() {}

void privateSharedMSIStatsPerTile::did_read_l1(bool hit) {
    m_l1_read_hits.add(hit? 1:0, 1);
}

void privateSharedMSIStatsPerTile::did_read_l2(bool hit) {
    m_l2_read_hits.add(hit? 1:0, 1);
}

void privateSharedMSIStatsPerTile::did_write_l1(bool hit) {
    m_l1_write_hits.add(hit? 1:0, 1);
}

void privateSharedMSIStatsPerTile::did_write_l2(bool hit) {
    m_l2_write_hits.add(hit? 1:0, 1);
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
    uint64_t total_l2_reads = 0;
    uint64_t total_l2_writes = 0;
    uint64_t total_l2 = 0;
    uint64_t total_cat_lookups = 0;
    uint64_t total_invs = 0;

    double total_l1_read_hits = 0.0;
    double total_l1_write_hits = 0.0;
    double total_l1_hits = 0.0;
    double total_l2_read_hits = 0.0;
    double total_l2_write_hits = 0.0;
    double total_l2_hits = 0.0;
    double total_inv_caches = 0.0;
    double total_inv_penalty = 0.0;
    double total_cat_hits = 0.0;

    perTileStats_t::iterator it;
    for (it = m_per_tile_stats.begin(); it != m_per_tile_stats.end(); ++it) {
        uint32_t id = it->first;
        shared_ptr<privateSharedMSIStatsPerTile> st = static_pointer_cast<privateSharedMSIStatsPerTile>(it->second);

        uint64_t l1_reads = st->m_l1_read_hits.sample_count();
        uint64_t l1_writes = st->m_l1_write_hits.sample_count();
        uint64_t l1 = l1_reads + l1_writes;
        uint64_t l2_reads = st->m_l2_read_hits.sample_count();
        uint64_t l2_writes = st->m_l2_write_hits.sample_count();
        uint64_t l2 = l2_reads + l2_writes;
        total_l1_reads += l1_reads;
        total_l1_writes += l1_writes;
        total_l1 += l1;
        total_l2_reads += l2_reads;
        total_l2_writes += l2_writes;
        total_l2 += l2;

        double l1_read_rate = st->m_l1_read_hits.get_mean();
        double l1_read_hits = l1_read_rate * l1_reads;
        double l1_write_rate = st->m_l1_write_hits.get_mean();
        double l1_write_hits = l1_write_rate * l1_writes;
        double l1_hits = l1_read_hits + l1_write_hits;
        double l1_rate = l1_hits / l1;
        double l2_read_rate = st->m_l2_read_hits.get_mean();
        double l2_read_hits = l2_read_rate * l2_reads;
        double l2_write_rate = st->m_l2_write_hits.get_mean();
        double l2_write_hits = l2_write_rate * l2_writes;
        double l2_hits = l2_read_hits + l2_write_hits;
        double l2_rate = l2_hits / l2;
        total_l1_read_hits += l1_read_hits;
        total_l1_write_hits += l1_write_hits;
        total_l1_hits += l1_hits;
        total_l2_read_hits += l2_read_hits;
        total_l2_write_hits += l2_write_hits;
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

        char str[1024];
        sprintf(str, "[Private-shared-MSI %4d ] L1hit%%: %.2f read%%: %.2f write%%: %.2f "
                     "L2hit%%: %.2f read%%: %.2f write%%: %.2f "
                     "avg-inv%%: %.2f avg-inv-caches: %.2f avg-inv-penalty: %.2f",
                     id, 100.0*l1_rate, 100.0*l1_read_rate, 100.0*l1_write_rate, 
                     100.0*l2_rate, 100.0*l2_read_rate, 100.0*l2_write_rate, 
                     invs*100.0/l2_writes, inv_caches/l2_writes, inv_penalty/l2_writes);

        out << str << endl;

    }

    out << endl;

    char str[1024];
    sprintf(str, "[Summary: Private-shared-MSI ] L1hit%%: %.2f read%%: %.2f write%%: %.2f "
            "L2hit%%: %.2f read%%: %.2f write%%: %.2f "
            "avg-inv%%: %.2f avg-inv-caches: %.2f avg-inv-penalty: %.2f",
            100.0*total_l1_hits/total_l1,
            100.0*total_l1_read_hits/total_l1_reads,
            100.0*total_l1_write_hits/total_l1_writes,
            100.0*total_l2_hits/total_l2,
            100.0*total_l2_read_hits/total_l2_reads,
            100.0*total_l2_write_hits/total_l2_writes,
            total_invs*100.0/total_l2_writes,
            total_inv_caches/total_l2_writes, 
            total_inv_penalty/total_l2_writes);

    out << str << endl;

    out << endl;

}
