// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __INGRESS_ID_HPP__
#define __INGRESS_ID_HPP__

#include <string>
#include "node_id.hpp"

using namespace std;

class ingress_id {
public:
    explicit ingress_id(const node_id parent, const string &name);
    bool operator==(const ingress_id &) const;
    bool operator<(const ingress_id &) const;
    friend ostream &operator<<(ostream &, const ingress_id &);
    const node_id &get_node_id() const;
    const string &get_name() const;
private:
    const node_id parent;
    const string name;
private:
    explicit ingress_id(); // not implemented
};

inline ingress_id::ingress_id(const node_id new_parent,
                              const string &new_name)
    : parent(new_parent), name(new_name) { }

inline const node_id &ingress_id::get_node_id() const { return parent; }

inline const string &ingress_id::get_name() const { return name; }

inline bool ingress_id::operator==(const ingress_id &o) const {
    return parent == o.parent && name == o.name;
}

inline bool ingress_id::operator<(const ingress_id &o) const {
    return parent < o.parent || (parent == o.parent && name < o.name);
}

ostream &operator<<(ostream &, const ingress_id &);

#endif // __INGRESS_ID_HPP__
