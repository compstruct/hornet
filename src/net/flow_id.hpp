// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __FLOW_HPP__
#define __FLOW_HPP__

#include <iostream>
#include "cstdint.hpp"

using namespace std;

class flow_id {
public:
    flow_id(uint32_t new_id);
    flow_id();
    bool is_valid() const;
    uint32_t get_numeric_id() const;
    bool operator==(const flow_id &) const;
    bool operator!=(const flow_id &) const;
    bool operator<(const flow_id &) const;
private:
    friend ostream &operator<<(ostream &, const flow_id &);
    uint32_t id;
};

inline flow_id::flow_id(uint32_t new_id) : id(new_id) { }

inline flow_id::flow_id() : id(0xffffffffUL) { }

inline bool flow_id::is_valid() const { return id != 0xffffffffUL; }

inline uint32_t flow_id::get_numeric_id() const { return id; }

inline bool flow_id::operator==(const flow_id &o) const {
    return id == o.id;
}

inline bool flow_id::operator!=(const flow_id &o) const {
    return !operator==(o);
}

inline bool flow_id::operator<(const flow_id &o) const {
    return id < o.id;
}

ostream &operator<<(ostream &, const flow_id &);

#endif // __FLOW_HPP__

