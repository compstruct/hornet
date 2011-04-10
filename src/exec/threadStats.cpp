// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "threadStats.hpp"

threadStatsPerThread::threadStatsPerThread(uint32_t id, const uint64_t &t) :
    m_id(id), system_time(t), m_spawned(false), m_completed(false), m_spawned_time(0), m_completion_time(0) { }

threadStatsPerThread::~threadStatsPerThread() {}

threadStats::threadStats(const uint64_t &t) : aux_statistics(t) {}

threadStats::~threadStats() {}

void threadStats::add_per_thread_stats(shared_ptr<threadStatsPerThread> stats) {
    assert(m_per_thread_stats.count(stats->m_id) == 0);
    m_per_thread_stats[stats->m_id] = stats;
}

void threadStats::print_stats(ostream &out) {

    out << endl;

    out << "Thread Statistics" << endl;
    out << "-----------------" << endl;
    out << endl;

    bool completed = true;
    uint64_t parallel_completion_time = 0;
    double total_cycles = 0.0;
    uint64_t total_reads = 0;
    uint64_t total_writes = 0;
    uint64_t total_mem = 0;
    double total_read_latency = 0.0;
    double total_write_latency = 0.0;
    double total_latency = 0.0;
    uint64_t total_migrations = 0;
    uint64_t total_evictions = 0;
    double total_penalties = 0.0;

    perThreadStats_t::iterator it;
    for (it = m_per_thread_stats.begin(); it != m_per_thread_stats.end(); ++it) {
        uint32_t id = it->first;
        shared_ptr<threadStatsPerThread> st = it->second;
        uint64_t cycles;
        if (st->check_if_spawned())  {
            if (st->check_if_completed()) {
                cycles = st->m_completion_time - st->m_spawned_time;
            } else {
                completed = false;
                cycles = system_time - st->m_spawned_time;
            }   
        } else {
            cycles = 0;
        }
        if (completed && cycles > parallel_completion_time) {
            parallel_completion_time = cycles;
        }
        total_cycles += cycles;
        uint64_t reads = st->m_read_latencies.sample_count();
        uint64_t writes = st->m_write_latencies.sample_count();
        uint64_t mem = reads + writes;
        total_reads += reads;
        total_writes += writes;
        total_mem += mem;
        double amrl = st->m_read_latencies.get_mean();
        double amwl = st->m_write_latencies.get_mean();
        double aml = (amrl*reads + amwl*writes) / mem;
        total_read_latency += amrl * reads;
        total_write_latency += amwl * writes;
        total_latency += total_read_latency + total_write_latency;
        uint64_t migrations = st->m_migration_penalties.sample_count();
        uint64_t evictions = st->m_eviction_penalties.sample_count();
        double penalties = st->m_migration_penalties.get_mean() * migrations + st->m_eviction_penalties.get_mean() * evictions;
        total_migrations += migrations;
        total_evictions += evictions;
        total_penalties += penalties;

        char str[1024];
        if (st->check_if_completed()) {
            sprintf(str, "[Thread %4d completed] cycles: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                    "migration-rate%%: %.4f cycles-per-eviction: %.4f",
                    id, cycles, reads, writes, aml, amrl, amwl, 
                    (mem > 0)? migrations*100.0/mem : 0, 
                    (cycles-penalties)/evictions);
        } else if (st->check_if_spawned()) {
            sprintf(str, "[Thread %4d running] cycles: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                    "migration-rate%%: %.4f cycles-per-eviction: %.4f",
                    id, cycles, reads, writes, aml, amrl, amwl, (mem > 0)? migrations*100.0/mem : 0, 
                    (cycles-penalties)/evictions);
        }

        out << str << endl;

    }
    out << endl;
    char str[1024];
    if (completed) {
        sprintf(str, "[Summary: Thread completed] PCT: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                "migration-rate%%: %.4f cycles-per-eviction: %.4f",
                parallel_completion_time, total_reads, total_writes,
                total_latency/total_mem, 
                total_read_latency/total_reads,
                total_write_latency/total_writes,
                (total_mem > 0)? total_migrations*100.0/total_mem : 0, 
                (total_cycles-total_penalties)/total_evictions);
    } else {
        sprintf(str, "[Summary: Thread running] PCT: N/A reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                "migration-rate%%: %.4f cycles-per-eviction: %.4f",
                total_reads, total_writes, 
                total_latency/total_mem, 
                total_read_latency/total_reads,
                total_write_latency/total_writes,
                (total_mem > 0)? total_migrations*100.0/total_mem : 0, 
                (total_cycles-total_penalties)/total_evictions);
    }
    out << str << endl;

    out << endl;
}
