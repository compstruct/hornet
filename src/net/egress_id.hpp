// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __EGRESS_ID_HPP__
#define __EGRESS_ID_HPP__

#include <string>
#include "node_id.hpp"

using namespace std;

class egress_id {
public:
    explicit egress_id(); // bogus egress ID
    explicit egress_id(const node_id parent, const string &name);
    bool operator==(const egress_id &) const;
    bool operator<(const egress_id &) const;
    friend ostream &operator<<(ostream &, const egress_id &);
private:
    node_id parent;
    string name;
};

inline egress_id::egress_id() : parent(), name("?") { }

inline egress_id::egress_id(const node_id p, const string &n)
    : parent(p), name(n) { }

inline bool egress_id::operator==(const egress_id &o) const {
    return parent == o.parent && name == o.name;
}

inline bool egress_id::operator<(const egress_id &o) const {
    return parent < o.parent || (parent == o.parent && name < o.name);
}

ostream &operator<<(ostream &, const egress_id &);

#endif // __EGRESS_ID_HPP__
