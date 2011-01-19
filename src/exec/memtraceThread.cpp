// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceThread.hpp"

memtraceThread::memtraceThread(mth_id_t id, logger &l) 
    :m_id(id), log(l) {
    /* Temporary */
    m_cur_type = INST_OTHER;
    m_remaining = 2;
}

memtraceThread::~memtraceThread() { }

/* Temporary implementation */

void memtraceThread::fetch() {
    m_cur_type = (m_cur_type == INST_OTHER) ? INST_MEMORY : INST_OTHER;
    m_remaining = (m_cur_type == INST_OTHER)? 2 : 1;
}

void memtraceThread::execute() {
    assert(m_remaining > 0);
    --m_remaining;
}

memtraceThread::inst_type_t memtraceThread::type() { return m_cur_type; }

uint32_t memtraceThread::remaining_alu_cycle() { return m_remaining; }

mreq_type_t memtraceThread::rw() { 
    assert(m_cur_type == INST_MEMORY);
    return MEM_READ;
}

maddr_t memtraceThread::addr() {
    return 0xdeadbeef;
}

uint32_t memtraceThread::byte_count() {
    return 4;
}
