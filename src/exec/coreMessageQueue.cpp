// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "coreMessageQueue.hpp"

coreMessageQueue::coreMessageQueue(msg_type_t type, uint32_t capacity) : m_type(type), m_capacity(capacity) {}

coreMessageQueue::~coreMessageQueue() {}

bool coreMessageQueue::push_back(msg_t &msg) {
    msg.type = m_type;
    if (size() < m_capacity) {
        m_queue.push_back(msg);
        return true;
    }
    return false;
}

bool coreMessageQueue::pop() {
    if (size()>0) {
        m_queue.erase(m_queue.begin());
        return true;
    }
    return false;
}

msg_t coreMessageQueue::at(uint32_t idx) {
    assert(size() > idx);
    return m_queue[idx];
}


