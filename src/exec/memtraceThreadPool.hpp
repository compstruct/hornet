// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMTRACE_THREAD_POOL_HPP__
#define __MEMTRACE_THREAD_POOL_HPP__

#include "memtraceThread.hpp"
#include <vector>

class memtraceThreadPool {
public:
    memtraceThreadPool();
    ~memtraceThreadPool();

    void add_thread(memtraceThread* p);
    memtraceThread* find(mth_id_t id);
    memtraceThread* thread_at(uint32_t n);
    unsigned int size();

    bool empty();

private:
    /* memtraceThreadPool class has the following restrictions for the performance reason */
    /* 1. no thread is added to the pool during simulation */
    /* 2. no thread is removed from the pool during simulation */
    map<mth_id_t, memtraceThread*> m_threads;
    mutable recursive_mutex memtraceThreadPool_mutex;
};

#endif
