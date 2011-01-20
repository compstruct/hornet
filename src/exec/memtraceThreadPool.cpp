// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memtraceThreadPool.hpp"

memtraceThreadPool::memtraceThreadPool() {}

memtraceThreadPool::~memtraceThreadPool() {
    map<mth_id_t, memtraceThread*>::iterator i;
    for (i = m_threads.begin(); i != m_threads.end(); ++i) {
        delete i->second;
    }
    m_threads.clear();
}

void memtraceThreadPool::add_thread(memtraceThread* p) {
    unique_lock<recursive_mutex> lock(memtraceThreadPool_mutex);
    assert(m_threads.count(p->get_id()) == 0);
    m_threads[p->get_id()] = p;
}

memtraceThread* memtraceThreadPool::find(mth_id_t id) {
    /* if a thread may be added or removed during simulation, */
    /* it must be protected by a lock (which means slow) */
    if (m_threads.count(id) > 0) {
        return m_threads[id];
    } else {
        return NULL;
    }
}

memtraceThread* memtraceThreadPool::thread_at(uint32_t n) {
    /* if a thread may be added or removed during simulation, */
    /* it must be protected by a lock (which means slow) */
    assert(m_threads.size() > n);
    map<mth_id_t, memtraceThread*>::iterator i = m_threads.begin();
    for (; n > 0; ++i, --n) ;
    return i->second;
}

unsigned int memtraceThreadPool::size() {
    unique_lock<recursive_mutex> lock(memtraceThreadPool_mutex);
    return m_threads.size();
}

bool memtraceThreadPool::empty() {
    unique_lock<recursive_mutex> lock(memtraceThreadPool_mutex);
    map<mth_id_t, memtraceThread*>::iterator i;
    for (i = m_threads.begin(); i != m_threads.end(); ++i) {
        if (!(i->second)->finished()) {
            return false;
        }
    }
    return true;
}


