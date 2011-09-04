// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MESSAGES_HPP__
#define __MESSAGES_HPP__

#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <vector>

using namespace std;
using namespace boost;

typedef struct {
    uint32_t type;
    uint32_t src;
    uint32_t dst;
    uint32_t flit_count;
    shared_ptr<void> content;
} message_t;

class messageQueue {
public:
    messageQueue(uint32_t type, uint32_t capacity);
    ~messageQueue();

    uint32_t type() { return m_type; }
    inline uint32_t size() { return m_queue.size(); }
    bool push_back(shared_ptr<message_t> msg);
    inline shared_ptr<message_t> front() { assert(size()>0); return m_queue.front(); }
    bool pop();
    inline shared_ptr<message_t> at(uint32_t idx) { return m_queue.at(idx); }
    inline bool available() { return size() < m_capacity; }
 
private:
    uint32_t m_type;
    uint32_t m_capacity;
    vector<shared_ptr<message_t> > m_queue;
};

#endif

