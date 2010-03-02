// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __LINK_ID_HPP__
#define __LINK_ID_HPP__

#include <iostream>
#include "cstdint.hpp"
#include "node_id.hpp"

using namespace std;

class link_id {
public:
    link_id(const node_id &src, const node_id &dst) throw();
    link_id() throw();
    bool is_valid() const throw();
    void operator=(const link_id &) throw();
    bool operator==(const link_id &) const throw();
    bool operator!=(const link_id &) const throw();
    friend ostream &operator<<(ostream &, const link_id &);
private:
    node_id src;
    node_id dst;
private:
};

inline link_id::link_id(const node_id &new_src, const node_id &new_dst) throw()
    : src(new_src), dst(new_dst) { }

inline link_id::link_id() throw() : src(), dst() { }

inline bool link_id::is_valid() const throw() {
    return src.is_valid() && dst.is_valid();
}

inline void link_id::operator=(const link_id &o) throw() {
    src = o.src;
    dst = o.dst;
}

inline bool link_id::operator==(const link_id &o) const throw() {
    return src == o.src && dst == o.dst;
}

inline bool link_id::operator!=(const link_id &o) const throw() {
    return src != o.src || dst != o.dst;
}

ostream &operator<<(ostream &, const link_id &);

#endif // __LINK_ID_HPP__

