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
    memtraceThread(mth_id_t id, logger &l);
    ~memtraceThread();

    inline mth_id_t get_id() { return m_id; }

    bool finished();

    /* move to the next instruction */
    void fetch();

    /* decrease alu_time */
    void execute();

    /* read from current instruction */
    inst_type_t type();
    uint32_t remaining_alu_cycle();

    /* read from current instruction - valid for INST_MEMORY only */
    mreq_type_t rw();
    maddr_t addr();
    uint32_t byte_count();
    int home();

    /* add instructions */
    void add_mem_inst(uint32_t alu_cost, bool write, maddr_t addr, int home, uint32_t byte_count);
    void add_non_mem_inst(uint32_t alu_cost);

private:
    typedef struct {
        uint32_t remaining_alu_cost;
        inst_type_t type;
        mreq_type_t rw;
        maddr_t addr;
        int home;
        uint32_t byte_count;
    } inst_t;

    mth_id_t m_id;
    logger &log;

    inst_t m_cur;
    vector<inst_t> m_insts;

};

#endif
