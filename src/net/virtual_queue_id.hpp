// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __VIRTUAL_QUEUE_ID_HPP__
#define __VIRTUAL_QUEUE_ID_HPP__

#include <iostream>
#include "cstdint.hpp"
#include "node_id.hpp"

using namespace std;

class virtual_queue_id {
public:
    virtual_queue_id();
    virtual_queue_id(uint32_t new_id);
    virtual_queue_id operator+(int) const;
    bool is_valid() const;
    bool operator==(const virtual_queue_id &) const;
    bool operator<(const virtual_queue_id &) const;
    uint32_t get_numeric_id() const;
    friend ostream &operator<<(ostream &, const virtual_queue_id &);
private:
    uint32_t id;
};

typedef std::tuple<node_id, virtual_queue_id> virtual_queue_node_id;

inline virtual_queue_id::virtual_queue_id()
    : id(0xffffffffUL) { }

inline virtual_queue_id::virtual_queue_id(uint32_t new_id)
    : id(new_id) { }

inline bool virtual_queue_id::is_valid() const {
    return id != 0xffffffffUL;
}

inline virtual_queue_id virtual_queue_id::operator+(int diff) const {
    return virtual_queue_id(id + diff);
}

inline bool
virtual_queue_id::operator==(const virtual_queue_id &o) const {
    return id == o.id;
}

inline bool
virtual_queue_id::operator<(const virtual_queue_id &o) const {
    return id < o.id;
}

inline uint32_t virtual_queue_id::get_numeric_id() const { return id; }

ostream &operator<<(ostream &, const virtual_queue_id &);

ostream &operator<<(ostream &, const virtual_queue_node_id &);

#endif // __VIRTUAL_QUEUE_ID_HPP__
