// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __FLIT_HPP__
#define __FLIT_HPP__

#include <iostream>
#include <cassert>
#include "cstdint.hpp"
#include "flow_id.hpp"

using namespace std;

typedef uint64_t flit_id;
typedef uint64_t packet_id;

class flit {
public:
    flit(uint64_t data, packet_id pid=0xffffffffffffffffULL) throw();
    flit(const flit &flit) throw();
    flit() throw();
    flit &operator=(const flit &flit) throw();
    const uint64_t &get_data() const throw();
    const flit_id &get_uid() const throw();
    const packet_id &get_packet_id() const throw();
    uint64_t get_next_uid() throw();
protected:
    uint64_t data;
    flit_id uid;
    packet_id pid;
protected:
    static flit_id next_uid;
};

inline flit::flit(uint64_t new_data, packet_id new_pid) throw()
    : data(new_data), uid(get_next_uid()), pid(new_pid) {
    assert(next_uid != 0);
}

inline flit::flit(const flit &f) throw()
    : data(f.data), uid(f.uid), pid(f.pid) { }

inline flit::flit() throw()
    : data(0xdeadbeefdeadbeefULL), uid(0xffffffffffffffffULL),
      pid(0xffffffffffffffffULL) { }

inline flit &flit::operator=(const flit &f) throw() {
    data = f.data;
    uid = f.uid;
    return *this;
}

inline uint64_t flit::get_next_uid() throw() {
   static pthread_mutex_t fuid_mutex = PTHREAD_MUTEX_INITIALIZER;
   pthread_mutex_lock (&fuid_mutex);
   uint64_t l_next_uid = next_uid++;
   pthread_mutex_unlock (&fuid_mutex);
   return l_next_uid;
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
