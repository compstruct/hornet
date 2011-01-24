// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CORE_MESSAGE_QUEUE_HPP__
#define __CORE_MESSAGE_QUEUE_HPP__

#include "memory.hpp"
#include "message.hpp"
#include "assert.h"
#include <vector>

using namespace std;

class coreMessageQueue {
public:
    coreMessageQueue(uint32_t capacity);
    ~coreMessageQueue();

    inline uint32_t size() { return m_queue.size(); }
    bool push_back(const msg_t &msg);
    inline msg_t front() { assert(size()>0); return m_queue.front(); }
    bool pop();
    msg_t at(uint32_t idx);
    
private:
    uint32_t m_capacity;
    vector<msg_t> m_queue;
};
#endif

