// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __FLIT_HPP__
#define __FLIT_HPP__

#include "cstdint.hpp"
#include <iostream>
#include <cassert>
#include <tuple>
#include <boost/thread.hpp>
#include "flow_id.hpp"

using namespace std;
using namespace boost;

typedef uint64_t packet_id;

typedef std::tuple<flow_id, packet_id> flow_packet_id;

class flit {
public:
    flit(uint64_t data, packet_id pid, bool count_in_stats);
    flit(const flit &flit);
    flit();
    flit &operator=(const flit &flit);
    const uint64_t &get_data() const;
    const uint64_t &get_age() const;
    const packet_id &get_packet_id() const;
    bool get_count_in_stats() const;
    void age(uint64_t ticks=1);
protected:
    uint64_t data;
    uint64_t current_age;
    packet_id pid;
    bool count_in_stats;
};

inline flit::flit(uint64_t new_data, packet_id new_pid, bool stats)
    : data(new_data), current_age(0), pid(new_pid), count_in_stats(stats) {
    assert(new_pid != UINT64_MAX);
}

inline flit::flit(const flit &f)
    : data(f.data), current_age(f.current_age), pid(f.pid),
      count_in_stats(f.count_in_stats) { }

inline flit::flit()
    : data(UINT64_MAX), current_age(UINT64_MAX), pid(UINT64_MAX),
      count_in_stats(false) { }

inline flit &flit::operator=(const flit &f) {
    data = f.data;
    current_age = f.current_age;
    pid = f.pid;
    count_in_stats = f.count_in_stats;
    return *this;
}

inline const uint64_t &flit::get_data() const { return data; }

inline const uint64_t &flit::get_age() const { return current_age; }

inline const uint64_t &flit::get_packet_id() const { return pid; }

inline bool flit::get_count_in_stats() const { return count_in_stats; }

inline void flit::age(uint64_t ticks) {
    assert(ticks != 0);
    current_age += ticks;
}

// for head flits, length is the number of packets that follow the head
class head_flit : public flit {
public:
    head_flit(flow_id id, uint32_t length, packet_id pid,
              bool count_in_stats);
    flow_id get_flow_id() const;
    void set_flow_id(const flow_id &fid);
    uint32_t get_length() const;
};

inline head_flit::head_flit(flow_id id, uint32_t length, packet_id pid,
                            bool stats)
    : flit((((uint64_t) id.get_numeric_id()) << 32) | length, pid, stats) {
    assert(id.get_numeric_id() == (id.get_numeric_id() & 0xffffffffUL));
}

inline flow_id head_flit::get_flow_id() const {
    return flow_id(data >> 32);
}

inline void head_flit::set_flow_id(const flow_id &fid) {
    assert(fid.is_valid());
    assert(fid.get_numeric_id() == (fid.get_numeric_id() & 0xffffffffUL));
    data = (((uint64_t) fid.get_numeric_id()) << 32) | (data & 0xffffffffUL);
}

inline uint32_t head_flit::get_length() const {
    return data & 0xffffffffUL;
}

ostream &operator<<(ostream &out, const flit &f);

ostream &operator<<(ostream &out, const head_flit &f);

#endif // __FLIT_HPP__
