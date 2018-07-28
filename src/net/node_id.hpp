// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __NODE_ID_HPP__
#define __NODE_ID_HPP__

#include <iostream>
#include "cstdint.hpp"

using namespace std;

class node_id {
public:
    node_id(uint32_t new_id);
    node_id();
    bool is_valid() const;
    void operator=(const node_id &);
    bool operator==(const node_id &) const;
    bool operator!=(const node_id &) const;
    bool operator<(const node_id &) const;
    uint32_t get_numeric_id() const;
    friend ostream &operator<<(ostream &, const node_id &);
private:
    uint32_t id;
private:
};

inline node_id::node_id(uint32_t new_id) : id(new_id) { }

inline node_id::node_id() : id(0xffffffffUL) { }

inline bool node_id::is_valid() const { return id != 0xffffffffUL; }

inline void node_id::operator=(const node_id &o) { id = o.id; }

inline bool node_id::operator==(const node_id &o) const {
    return id == o.id;
}

inline bool node_id::operator!=(const node_id &o) const {
    return id != o.id;
}

inline bool node_id::operator<(const node_id &o) const {
    return id < o.id;
}

inline uint32_t node_id::get_numeric_id() const { return id; }

ostream &operator<<(ostream &, const node_id &);

#endif // __NODE_ID_HPP__

