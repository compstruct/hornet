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
    link_id(const node_id &src, const node_id &dst);
    link_id();
    bool is_valid() const;
    void operator=(const link_id &);
    bool operator==(const link_id &) const;
    bool operator!=(const link_id &) const;
    friend ostream &operator<<(ostream &, const link_id &);
private:
    node_id src;
    node_id dst;
private:
};

inline link_id::link_id(const node_id &new_src, const node_id &new_dst)
    : src(new_src), dst(new_dst) { }

inline link_id::link_id() : src(), dst() { }

inline bool link_id::is_valid() const {
    return src.is_valid() && dst.is_valid();
}

inline void link_id::operator=(const link_id &o) {
    src = o.src;
    dst = o.dst;
}

inline bool link_id::operator==(const link_id &o) const {
    return src == o.src && dst == o.dst;
}

inline bool link_id::operator!=(const link_id &o) const {
    return src != o.src || dst != o.dst;
}

ostream &operator<<(ostream &, const link_id &);

#endif // __LINK_ID_HPP__

