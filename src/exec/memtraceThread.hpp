// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMTRACE_THREAD_HPP__
#define __MEMTRACE_THREAD_HPP__

#include <boost/shared_ptr.hpp>
#include "memory.hpp"
#include "logger.hpp"

typedef uint32_t mth_id_t;

class memtraceThread {
public:
    typedef enum {
        INST_NONE = 0,  /* thread is finished */
        INST_MEMORY,
        INST_OTHER
    } inst_type_t;

public:
    memtraceThread(mth_id_t id, const uint64_t &system_time, logger &l);
    ~memtraceThread();

    inline mth_id_t get_id() { return m_id; }

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
    inline uint64_t memory_issued_time() { return m_cur.memory_issued_time; }

    /* read from current instruction - valid for INST_MEMORY only */
    mreq_type_t rw();
    maddr_t addr();
    uint32_t byte_count();
    int home();

    /* for now, one native core per thread */
    inline int native_core() { return m_native_core; }
    inline void set_native_core(int core) { m_native_core = core; }

    /* add instructions */
    void add_mem_inst(uint32_t alu_cost, bool write, maddr_t addr, int home, uint32_t byte_count);
    void add_non_mem_inst(uint32_t repeats);

private:
    typedef struct {
        uint32_t repeats;
        uint32_t alu_cost;
        uint32_t remaining_alu_cost;
        inst_type_t type;
        mreq_type_t rw;
        maddr_t addr;
        int home;
        uint32_t byte_count;
        /* stats */
        uint64_t memory_issued_time;
    } inst_t;

    mth_id_t m_id;
    const uint64_t &system_time;
    logger &log;

    inst_t m_cur;
    vector<inst_t> m_insts;

    int m_native_core;
};

#endif
