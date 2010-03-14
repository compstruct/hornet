// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __FLIT_HPP__
#define __FLIT_HPP__

#include "cstdint.hpp"
#include <iostream>
#include <cassert>
#include <boost/tuple/tuple.hpp>
#include <boost/thread.hpp>
#include "flow_id.hpp"

using namespace std;
using namespace boost;

typedef uint64_t packet_id;

typedef tuple<flow_id, packet_id> flow_packet_id;

class flit {
public:
    flit(uint64_t data, packet_id pid, bool count_in_stats) throw();
    flit(const flit &flit) throw();
    flit() throw();
    flit &operator=(const flit &flit) throw();
    const uint64_t &get_data() const throw();
    const uint64_t &get_age() const throw();
    const packet_id &get_packet_id() const throw();
    bool get_count_in_stats() const throw();
    void age(uint64_t ticks=1) throw();
protected:
    uint64_t data;
    uint64_t current_age;
    packet_id pid;
    bool count_in_stats;
};

inline flit::flit(uint64_t new_data, packet_id new_pid, bool stats) throw()
    : data(new_data), current_age(0), pid(new_pid), count_in_stats(stats) {
    assert(new_pid != UINT64_MAX);
}

inline flit::flit(const flit &f) throw()
    : data(f.data), current_age(f.current_age), pid(f.pid) { }

inline flit::flit() throw()
    : data(UINT64_MAX), current_age(UINT64_MAX), pid(UINT64_MAX) { }

inline flit &flit::operator=(const flit &f) throw() {
    data = f.data;
    current_age = f.current_age;
    pid = f.pid;
    count_in_stats = f.count_in_stats;
    return *this;
}

inline const uint64_t &flit::get_data() const throw() { return data; }

inline const uint64_t &flit::get_age() const throw() { return current_age; }

inline const uint64_t &flit::get_packet_id() const throw() { return pid; }

inline bool flit::get_count_in_stats() const throw() { return count_in_stats; }

inline void flit::age(uint64_t ticks) throw() {
    assert(ticks != 0);
    current_age += ticks;
}

// for head flits, length is the number of packets that follow the head
class head_flit : public flit {
public:
    head_flit(flow_id id, uint32_t length, packet_id pid,
              bool count_in_stats) throw();
    flow_id get_flow_id() const throw();
    void set_flow_id(const flow_id &fid) throw();
    uint32_t get_length() const throw();
};

inline head_flit::head_flit(flow_id id, uint32_t length, packet_id pid,
                            bool stats) throw()
    : flit((((uint64_t) id.get_numeric_id()) << 32) | length, pid, stats) {
    assert(id.get_numeric_id() == (id.get_numeric_id() & 0xffffffffUL));
}

inline flow_id head_flit::get_flow_id() const throw() {
    return flow_id(data >> 32);
}

inline void head_flit::set_flow_id(const flow_id &fid) throw() {
    assert(fid.is_valid());
    assert(fid.get_numeric_id() == (fid.get_numeric_id() & 0xffffffffUL));
    data = (((uint64_t) fid.get_numeric_id()) << 32) | (data & 0xffffffffUL);
}

inline uint32_t head_flit::get_length() const throw() {
    return data & 0xffffffffUL;
}

ostream &operator<<(ostream &out, const flit &f);

ostream &operator<<(ostream &out, const head_flit &f);

#endif // __FLIT_HPP__
