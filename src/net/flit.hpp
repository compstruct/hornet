// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __FLIT_HPP__
#define __FLIT_HPP__

#include <iostream>
#include <cassert>
#include "cstdint.hpp"
#include "flow_id.hpp"

using namespace std;

class flit {
public:
    flit(uint64_t data) throw();
    flit(const flit &flit) throw();
    flit &operator=(const flit &flit) throw();
    const uint64_t &get_data() const throw();
    const uint64_t &get_uid() const throw();
protected:
    uint64_t data;
    uint64_t uid;
protected:
    static uint64_t next_uid;
};

inline flit::flit(uint64_t new_data) throw() : data(new_data), uid(next_uid++) {
    assert(next_uid != 0);
}

inline flit::flit(const flit &f) throw() : data(f.data), uid(f.uid) { }

inline flit &flit::operator=(const flit &f) throw() {
    data = f.data;
    uid = f.uid;
    return *this;
}

inline const uint64_t &flit::get_data() const throw() { return data; }

inline const uint64_t &flit::get_uid() const throw() { return uid; }

// for head flits, length is the number of packets that follow the head
class head_flit : public flit {
public:
    head_flit(flow_id id, uint32_t length) throw();
    flow_id get_flow_id() const throw();
    uint32_t get_length() const throw();
};

inline head_flit::head_flit(flow_id id, uint32_t length) throw()
    : flit((((uint64_t) id.get_numeric_id()) << 32) | length) { 
    assert(id.get_numeric_id() == (id.get_numeric_id() & 0xffffffffUL));
}

inline flow_id head_flit::get_flow_id() const throw() {
    return flow_id(data >> 32);
}

inline uint32_t head_flit::get_length() const throw() {
    return data & 0xffffffffUL;
}

ostream &operator<<(ostream &out, const flit &f);

ostream &operator<<(ostream &out, const head_flit &f);

#endif // __FLIT_HPP__

