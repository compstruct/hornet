// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMTRACE_THREAD_HPP__
#define __MEMTRACE_THREAD_HPP__

#include <memory>
#include "memory.hpp"
#include "logger.hpp"
#include "memtraceThreadStats.hpp"

class memtraceThread {
public:
    typedef enum {
        INST_NONE = 0,  /* thread is finished */
        INST_MEMORY,
        INST_OTHER
    } inst_type_t;

public:
    memtraceThread(uint32_t id, const uint64_t &system_time, logger &l);
    ~memtraceThread();

    inline uint32_t get_id() { return m_id; }

    bool finished();

    /* move to the next instruction */
    void fetch();

    /* decrease alu_time */
    void execute();

    /* restart current instruction */
    void reset_current_instruction();

    inline bool current_instruction_done() { return remaining_alu_cycle() == 0; }

    /* read from current instruction */
    inst_type_t type();
    uint32_t remaining_alu_cycle();
    inline uint64_t first_memory_issued_time() { return m_cur.first_memory_issued_time; }

    /* read from current instruction - valid for INST_MEMORY only */
    bool is_read();
    maddr_t maddr();
    uint32_t word_count();

    /* for now, one native core per thread */
    inline int native_core() { return m_native_core; }
    inline void set_native_core(int core) { m_native_core = core; }

    /* add instructions */
    void add_mem_inst(uint32_t alu_cost, bool write, maddr_t maddr, uint32_t word_count);
    void add_non_mem_inst(uint32_t repeats);

    /* set stats */
    inline void set_per_thread_stats(std::shared_ptr<memtraceThreadStatsPerThread> stats) { m_stats = stats; }
    inline bool stats_enabled() { return (m_stats != std::shared_ptr<memtraceThreadStatsPerThread>()); }
    inline std::shared_ptr<memtraceThreadStatsPerThread> stats() { return m_stats; }

    inline std::shared_ptr<std::shared_ptr<void> > per_mem_instr_runtime_info() { return m_cur.per_mem_instr_runtime_info; }

private:
    typedef struct {
        uint32_t repeats = 0;
        uint32_t alu_cost = 0;
        uint32_t remaining_alu_cost = 0;
        inst_type_t type = INST_NONE;
        bool is_read = false;
        maddr_t maddr = { 0, 0 };
        uint32_t word_count = 0;
        /* stats */
        bool first_memory_issued = false;
        uint64_t first_memory_issued_time = 0;
        std::shared_ptr<std::shared_ptr<void> > per_mem_instr_runtime_info = std::shared_ptr<std::shared_ptr<void> >();
    } inst_t;

    uint32_t m_id;
    const uint64_t &system_time;
    logger &log;
    std::shared_ptr<memtraceThreadStatsPerThread> m_stats;

    inst_t m_cur;
    vector<inst_t> m_insts;

    int m_native_core;

};

class memtraceThreadPool {
public:
    memtraceThreadPool();
    ~memtraceThreadPool();

    void add_thread(std::shared_ptr<memtraceThread> p);
    std::shared_ptr<memtraceThread> find(uint32_t id);
    std::shared_ptr<memtraceThread> thread_at(uint32_t n);
    unsigned int size();

    bool empty();

private:
    /* memtraceThreadPool class has the following restrictions for the performance reason */
    /* 1. no thread is added to the pool during simulation */
    /* 2. no thread is removed from the pool during simulation */
    map<uint32_t, std::shared_ptr<memtraceThread> >m_threads;
    mutable boost::recursive_mutex memtraceThreadPool_mutex;
};

#endif
