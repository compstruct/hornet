// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __THREAD_STATS_HPP__
#define __THREAD_STATS_HPP__

#include "statistics.hpp"
#include <cstdio>

class threadStatsPerThread {
public:
    threadStatsPerThread(uint32_t id, const uint64_t &system_time);
    virtual ~threadStatsPerThread();

    inline bool check_if_spawned() { return m_spawned; }
    inline bool check_if_completed() { return m_completed; }
    inline void did_spawn() { m_spawned = true; m_spawned_time = system_time; }
    inline void did_complete() { m_completed = true; m_completion_time = system_time; }
    inline void did_finish_read(uint64_t latency) { m_read_latencies.add(latency, 1); }
    inline void did_finish_write(uint64_t latency) { m_write_latencies.add(latency, 1); }

    void did_begin_migration();
    void did_begin_eviction();
    void did_arrive_destination();

    friend class threadStats;

private:

    typedef enum {
        NOT_MIGRATING = 0,
        MIGRATING,
        BEING_EVICTED
    } migrationStatus_t;

    uint32_t m_id;
    const uint64_t &system_time;

    bool m_spawned;
    bool m_completed;

    uint64_t m_spawned_time;
    uint64_t m_completion_time;

    migrationStatus_t m_mig_status;
    uint64_t m_mig_start_time;
    uint64_t m_mig_penalty_to_append;

    running_stats m_read_latencies;
    running_stats m_write_latencies;

    running_stats m_migration_penalties;
    running_stats m_eviction_penalties;

};

class threadStats : public aux_statistics {
public:
    threadStats(const uint64_t &system_time);
    virtual ~threadStats();

    void add_per_thread_stats(shared_ptr<threadStatsPerThread> stats);

protected:
    virtual void print_stats(ostream &out);

    typedef map<uint32_t/*thread id*/, shared_ptr<threadStatsPerThread> > perThreadStats_t;

    perThreadStats_t m_per_thread_stats;

};


#endif
