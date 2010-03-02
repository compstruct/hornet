// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __FLIT_HPP__
#define __FLIT_HPP__

#include "cstdint.hpp"
#include <iostream>
#include <cassert>
#include <boost/thread.hpp>
#include "flow_id.hpp"

using namespace std;
using namespace boost;

typedef uint64_t flit_id;
typedef uint64_t packet_id;

class flit {
public:
    flit(uint64_t data, packet_id pid=UINT64_MAX) throw();
    flit(const flit &flit) throw();
    flit() throw();
    flit &operator=(const flit &flit) throw();
    const uint64_t &get_data() const throw();
    const flit_id &get_uid() const throw();
    const packet_id &get_packet_id() const throw();
protected:
    uint64_t data;
    flit_id uid;
    packet_id pid;
protected:
    static flit_id next_uid;
    static mutex next_uid_mutex;
};

inline flit::flit(uint64_t new_data, packet_id new_pid) throw()
    : data(new_data), pid(new_pid) {
    unique_lock<mutex> lock(next_uid_mutex);
    assert(next_uid != UINT64_MAX);
   uid = next_uid;
   ++next_uid;
}

inline flit::flit(const flit &f) throw()
    : data(f.data), uid(f.uid), pid(f.pid) { }

inline flit::flit() throw()
    : data(0xdeadbeefdeadbeefULL), uid(UINT64_MAX), pid(UINT64_MAX) { }

inline flit &flit::operator=(const flit &f) throw() {
    data = f.data;
    uid = f.uid;
    return *this;
}

inline const uint64_t &flit::get_data() const throw() { return data; }

inline const uint64_t &flit::get_uid() const throw() { return uid; }

inline const uint64_t &flit::get_packet_id() const throw() { return pid; }

// for head flits, length is the number of packets that follow the head
class head_flit : public flit {
public:
    head_flit(flow_id id, uint32_t length,
              packet_id pid=0xffffffffffffffffULL) throw();
    head_flit(const head_flit &f, flow_id new_id) throw();
    flow_id get_flow_id() const throw();
    uint32_t get_length() const throw();
};

inline head_flit::head_flit(flow_id id, uint32_t length, packet_id pid) throw()
    : flit((((uint64_t) id.get_numeric_id()) << 32) | length, pid) {
    assert(id.get_numeric_id() == (id.get_numeric_id() & 0xffffffffUL));
}

inline head_flit::head_flit(const head_flit &f, flow_id new_id) throw()
    : flit(f) {
    assert(new_id.get_numeric_id() == (new_id.get_numeric_id() & 0xffffffffUL));
    data = (((uint64_t) new_id.get_numeric_id()) << 32) | (data & 0xffffffffUL);
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
