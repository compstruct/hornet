// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceThread.hpp"

#define DEBUG
#undef DEBUG

#ifdef DEBUG
#define mh_log(X) if(true) cout
#define mh_assert(X) assert(X)
#else
#define mh_assert(X) 
#define mh_log(X) LOG(log,X)
#endif

memtraceThread::memtraceThread(uint32_t id, const uint64_t &t, logger &l) : 
    m_id(id), system_time(t), log(l), m_stats(std::shared_ptr<memtraceThreadStatsPerThread>()), m_cur()
{
    m_cur.type = INST_NONE;
    m_cur.repeats = 0;
}

memtraceThread::~memtraceThread() { }

void memtraceThread::add_non_mem_inst(uint32_t repeats) {
    /* for now, just assume all non-memory instruction takes 1 cycle */
    inst_t new_inst;
    new_inst.repeats = repeats;
    new_inst.alu_cost = 1;
    new_inst.remaining_alu_cost = 1;
    new_inst.type = INST_OTHER;
    m_insts.push_back(new_inst);
}

void memtraceThread::add_mem_inst(uint32_t alu_cost, bool write, maddr_t maddr, uint32_t word_count) {
    inst_t new_inst;
    new_inst.repeats = 1;
    new_inst.alu_cost = alu_cost;
    new_inst.remaining_alu_cost = alu_cost;
    new_inst.type = INST_MEMORY;
    new_inst.is_read = !write;
    new_inst.maddr = maddr;
    new_inst.word_count = word_count;
    new_inst.first_memory_issued = false;
    new_inst.per_mem_instr_runtime_info = std::shared_ptr<std::shared_ptr<void> >(new std::shared_ptr<void>(std::shared_ptr<void>()));
    m_insts.push_back(new_inst);
}

void memtraceThread::fetch() {
    if (m_cur.repeats > 0) {
        --(m_cur.repeats);
        m_cur.remaining_alu_cost = m_cur.alu_cost;
    } else if (m_insts.size() > 0) {
        m_cur = m_insts.front();
        --(m_cur.repeats);
        m_insts.erase(m_insts.begin());
        mh_log(4) << "[thread " << get_id() << " ] fetched a new instruction " << endl;
    } else {
        mh_log(1) << "[thread " << get_id() << " ] finished " << endl;
        m_cur.type = INST_NONE;
    }
}

void memtraceThread::execute() {
    assert(m_cur.remaining_alu_cost > 0);
    --(m_cur.remaining_alu_cost);
    if (m_cur.remaining_alu_cost == 0 && m_cur.type == INST_MEMORY && !m_cur.first_memory_issued) {
        m_cur.first_memory_issued = true;
        m_cur.first_memory_issued_time = system_time;
    }
}

void memtraceThread::reset_current_instruction() {
    m_cur.remaining_alu_cost = m_cur.alu_cost;
}

memtraceThread::inst_type_t memtraceThread::type() { 
    return m_cur.type;
}

uint32_t memtraceThread::remaining_alu_cycle() { 
    return (m_cur.type == INST_NONE)? 0 : m_cur.remaining_alu_cost; 
}

bool memtraceThread::is_read() { 
    assert(m_cur.type == INST_MEMORY);
    return m_cur.is_read;
}

maddr_t memtraceThread::maddr() {
    assert(m_cur.type == INST_MEMORY);
    return m_cur.maddr;
}

uint32_t memtraceThread::word_count() {
    assert(m_cur.type == INST_MEMORY);
    return m_cur.word_count;
}

bool memtraceThread::finished() {
    return (m_insts.empty() && m_cur.type == INST_NONE);
}

memtraceThreadPool::memtraceThreadPool() {}

memtraceThreadPool::~memtraceThreadPool() {}

void memtraceThreadPool::add_thread(std::shared_ptr<memtraceThread> p) {
    boost::unique_lock<boost::recursive_mutex> lock(memtraceThreadPool_mutex);
    assert(m_threads.count(p->get_id()) == 0);
    m_threads[p->get_id()] = p;
}

std::shared_ptr<memtraceThread> memtraceThreadPool::find(uint32_t id) {
    /* if a thread may be added or removed during simulation, */
    /* it must be protected by a lock (which means slow) */
    if (m_threads.count(id) > 0) {
        return m_threads[id];
    } else {
        return std::shared_ptr<memtraceThread>();
    }
}

std::shared_ptr<memtraceThread> memtraceThreadPool::thread_at(uint32_t n) {
    /* if a thread may be added or removed during simulation, */
    /* it must be protected by a lock (which means slow) */
    assert(m_threads.size() > n);
    map<uint32_t, std::shared_ptr<memtraceThread> >::iterator i = m_threads.begin();
    for (; n > 0; ++i, --n) ;
    return i->second;
}

unsigned int memtraceThreadPool::size() {
    boost::unique_lock<boost::recursive_mutex> lock(memtraceThreadPool_mutex);
    return m_threads.size();
}

bool memtraceThreadPool::empty() {
    boost::unique_lock<boost::recursive_mutex> lock(memtraceThreadPool_mutex);
    map<uint32_t, std::shared_ptr<memtraceThread> >::iterator i;
    for (i = m_threads.begin(); i != m_threads.end(); ++i) {
        if (!(i->second)->finished()) {
            return false;
        }
    }
    return true;
}


