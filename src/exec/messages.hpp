// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MESSAGES_HPP__
#define __MESSAGES_HPP__

#include <stdint.h>
#include <memory>
#include <vector>
#include <iostream>
#include <cassert>

using namespace std;

typedef struct structMessage {
    uint32_t type;
    uint32_t src;
    uint32_t dst;
    uint32_t flit_count;
    std::shared_ptr<void> content;

    structMessage();
    ~structMessage();

} message_t;

class messageQueue {
public:
    messageQueue(uint32_t type, uint32_t capacity);
    ~messageQueue();

    uint32_t type() { return m_type; }
    inline uint32_t size() { return m_queue.size(); }
    bool push_back(std::shared_ptr<message_t> msg);
    inline std::shared_ptr<message_t> front() { assert(size()>0); return m_queue.front(); }
    bool pop();
    inline std::shared_ptr<message_t> at(uint32_t idx) { return m_queue.at(idx); }
    inline bool available() { return size() < m_capacity; }
 
private:
    uint32_t m_type;
    uint32_t m_capacity;
    vector<std::shared_ptr<message_t> > m_queue;
};

#endif

