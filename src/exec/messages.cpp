// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "messages.hpp"

messageQueue::messageQueue(uint32_t type, uint32_t capacity) : m_type(type), m_capacity(capacity) {}

messageQueue::~messageQueue() {}

bool messageQueue::push_back(shared_ptr<message_t> msg) {
    if (size() < m_capacity) {
        m_queue.push_back(msg);
        return true;
    }
    return false;
}

bool messageQueue::pop() {
    if (size() > 0) {
        m_queue.erase(m_queue.begin());
        return true;
    } 
    return false;
}


