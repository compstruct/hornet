// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __VIRTUAL_QUEUE_ID_HPP__
#define __VIRTUAL_QUEUE_ID_HPP__

#include <iostream>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include "cstdint.hpp"
#include "node_id.hpp"

using namespace std;
using namespace boost;

class virtual_queue_id {
public:
    virtual_queue_id() throw();
    virtual_queue_id(uint32_t new_id) throw();
    virtual_queue_id operator+(int) const throw();
    bool is_valid() const throw();
    bool operator==(const virtual_queue_id &) const throw();
    bool operator<(const virtual_queue_id &) const throw();
    uint32_t get_numeric_id() const throw();
    friend ostream &operator<<(ostream &, const virtual_queue_id &);
private:
    uint32_t id;
};

typedef tuple<node_id, virtual_queue_id> virtual_queue_node_id;

inline virtual_queue_id::virtual_queue_id() throw()
    : id(0xffffffffUL) { }

inline virtual_queue_id::virtual_queue_id(uint32_t new_id) throw()
    : id(new_id) { }

inline bool virtual_queue_id::is_valid() const throw() {
    return id != 0xffffffffUL;
}

inline virtual_queue_id virtual_queue_id::operator+(int diff) const throw() {
    return virtual_queue_id(id + diff);
}

inline bool
virtual_queue_id::operator==(const virtual_queue_id &o) const throw() {
    return id == o.id;
}

inline bool
virtual_queue_id::operator<(const virtual_queue_id &o) const throw() {
    return id < o.id;
}

inline uint32_t virtual_queue_id::get_numeric_id() const throw() { return id; }

ostream &operator<<(ostream &, const virtual_queue_id &);

ostream &operator<<(ostream &, const virtual_queue_node_id &);

#endif // __VIRTUAL_QUEUE_ID_HPP__
