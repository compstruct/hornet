// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __FLOW_HPP__
#define __FLOW_HPP__

#include <iostream>
#include "cstdint.hpp"

using namespace std;

class flow_id {
public:
    flow_id(uint32_t new_id) throw();
    uint32_t get_numeric_id() const throw();
    bool operator==(const flow_id &) const throw();
    bool operator<(const flow_id &) const throw();
private:
    friend ostream &operator<<(ostream &, const flow_id &);
    explicit flow_id() throw(); // not implemented
    uint32_t id;
};

inline flow_id::flow_id(uint32_t new_id) throw() : id(new_id) {}

inline uint32_t flow_id::get_numeric_id() const throw() { return id; }

inline bool flow_id::operator==(const flow_id &o) const throw() {
    return id == o.id;
}

inline bool flow_id::operator<(const flow_id &o) const throw() {
    return id < o.id;
}

ostream &operator<<(ostream &, const flow_id &);

#endif // __FLOW_HPP__

