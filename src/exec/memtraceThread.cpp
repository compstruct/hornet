// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceThread.hpp"

memtraceThread::memtraceThread(mth_id_t id, logger &l) 
    :m_id(id), log(l) {
}

memtraceThread::~memtraceThread() { }

void memtraceThread::add_non_mem_inst(uint32_t alu_cost) {
    inst_t new_inst;
    new_inst.remaining_alu_cost = alu_cost;
    new_inst.type = INST_OTHER;
    m_insts.push_back(new_inst);
}

void memtraceThread::add_mem_inst(uint32_t alu_cost, bool write, maddr_t addr, int home, uint32_t byte_count) {
    inst_t new_inst;
    new_inst.remaining_alu_cost = alu_cost;
    new_inst.type = INST_MEMORY;
    new_inst.rw = (write)? MEM_WRITE : MEM_READ;
    new_inst.addr = addr;
    new_inst.home = home;
    new_inst.byte_count = byte_count;
    m_insts.push_back(new_inst);
}

void memtraceThread::fetch() {
    if (m_insts.size() > 0) {
        m_cur = m_insts.front();
        m_insts.erase(m_insts.begin());
    } else {
        m_cur.type = INST_NONE;
    }
}

void memtraceThread::execute() {
    assert(m_cur.remaining_alu_cost > 0);
    --(m_cur.remaining_alu_cost);
}

memtraceThread::inst_type_t memtraceThread::type() { 
    return m_cur.type;
}

uint32_t memtraceThread::remaining_alu_cycle() { 
    assert(m_cur.type != INST_NONE);
    return m_cur.remaining_alu_cost; 
}

mreq_type_t memtraceThread::rw() { 
    assert(m_cur.type == INST_MEMORY);
    return m_cur.rw;
}

maddr_t memtraceThread::addr() {
    assert(m_cur.type == INST_MEMORY);
    return m_cur.addr;
}

uint32_t memtraceThread::byte_count() {
    assert(m_cur.type == INST_MEMORY);
    return m_cur.byte_count;
}

int memtraceThread::home() {
    assert(m_cur.type == INST_MEMORY);
    return m_cur.home;
}

bool memtraceThread::finished() {
    return (m_insts.empty() && m_cur.type == INST_NONE);
}