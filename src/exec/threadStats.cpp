// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "threadStats.hpp"

threadStatsPerThread::threadStatsPerThread(uint32_t id, const uint64_t &t) :
    m_id(id), system_time(t), m_spawned(false), m_completed(false), m_spawned_time(0), m_completion_time(0),
    m_mig_status(NOT_MIGRATING), m_mig_penalty_to_append(0)
{ }

threadStatsPerThread::~threadStatsPerThread() {}

threadStats::threadStats(const uint64_t &t) : aux_statistics(t) {}

threadStats::~threadStats() {}

void threadStatsPerThread::did_begin_migration() { 
    m_mig_status = MIGRATING; 
    m_mig_start_time = system_time;
}

void threadStatsPerThread::did_begin_eviction() { 
    if (m_mig_status == MIGRATING) {
        m_mig_penalty_to_append += system_time - m_mig_start_time;
    }
    m_mig_status = BEING_EVICTED; 
    m_mig_start_time = system_time; 
}

void threadStatsPerThread::did_arrive_destination() { 
    if (m_mig_status == MIGRATING) {
        m_migration_penalties.add(system_time - m_mig_start_time, 1); 
    } else if (m_mig_status == BEING_EVICTED) {
        m_eviction_penalties.add(system_time - m_mig_start_time, 1); 
    }
    m_mig_status = NOT_MIGRATING;
}

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
    uint64_t total_reads = 0;
    uint64_t total_writes = 0;
    uint64_t total_mem = 0;
    double total_read_latency = 0.0;
    double total_write_latency = 0.0;
    uint64_t total_migrations = 0;
    uint64_t total_evictions = 0;
    double total_mig_penalties = 0.0;
    double total_evt_penalties = 0.0;

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
        uint64_t migrations = st->m_migration_penalties.sample_count();
        uint64_t evictions = st->m_eviction_penalties.sample_count();
        double mig_penalties = st->m_migration_penalties.get_mean() * migrations;
        double evt_penalties = st->m_eviction_penalties.get_mean() * evictions;
        total_migrations += migrations;
        total_evictions += evictions;
        total_mig_penalties += mig_penalties + st->m_mig_penalty_to_append;
        total_evt_penalties += evt_penalties;

        char str[1024];
        if (st->check_if_completed()) {
            sprintf(str, "[Thread %4d completed] cycles: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                    "migration-rate%%: %.4f total migrations: %ld migrations: %ld evictions: %ld "
                    "total migration latency: %.4f migration latency: %.4f eviction latency: %.4f",
                    id, cycles, reads, writes, aml, amrl, amwl, 
                    (mem > 0)? migrations*100.0/mem : 0, 
                    migrations + evictions, migrations, evictions, 
                    mig_penalties + evt_penalties, mig_penalties, evt_penalties);
        } else if (st->check_if_spawned()) {
            sprintf(str, "[Thread %4d running] cycles: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                    "migration-rate%%: %.4f total migrations: %ld migrations: %ld evictions: %ld "
                    "total migration latency: %.4f migration latency: %.4f eviction latency: %.4f",
                    id, cycles, reads, writes, aml, amrl, amwl, 
                    (mem > 0)? migrations*100.0/mem : 0, 
                    migrations + evictions, migrations, evictions, 
                    mig_penalties + evt_penalties, mig_penalties, evt_penalties);
        }

        out << str << endl;

    }
    out << endl;
    char str[1024];
    if (completed) {
        sprintf(str, "[Summary: Thread completed] PCT: %ld reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                "migration-rate%%: %.4f total migrations: %ld migrations: %ld evictions: %ld "
                "total migration latency: %.4f migration latency: %.4f eviction latency: %.4f",
                parallel_completion_time, total_reads, total_writes,
                (total_read_latency + total_write_latency)/total_mem, 
                total_read_latency/total_reads,
                total_write_latency/total_writes,
                (total_mem > 0)? total_migrations*100.0/total_mem : 0, 
                total_migrations + total_evictions, total_migrations, total_evictions,
                total_mig_penalties + total_evt_penalties, total_mig_penalties, total_evt_penalties);
    } else {
        sprintf(str, "[Summary: Thread running] PCT: N/A reads: %ld writes: %ld AML: %.4f AMRL: %.4f AMWL: %.4f "
                "migration-rate%%: %.4f total migrations: %ld migrations: %ld evictions: %ld "
                "total migration latency: %.4f migration latency: %.4f eviction latency: %.4f",
                total_reads, total_writes, 
                (total_read_latency + total_write_latency)/total_mem, 
                total_read_latency/total_reads,
                total_write_latency/total_writes,
                (total_mem > 0)? total_migrations*100.0/total_mem : 0, 
                total_migrations + total_evictions, total_migrations, total_evictions,
                total_mig_penalties + total_evt_penalties, total_mig_penalties, total_evt_penalties);
    }
    out << str << endl;

    out << endl;
}
