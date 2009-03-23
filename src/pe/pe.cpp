// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "pe.hpp"

pe::pe(const pe_id &new_id) throw() : id(new_id) { }

pe::~pe() throw() { }

ostream &operator<<(ostream &out, const pe_id &id) {
    return out << hex << setfill('0') << setw(2) << id.id;
}
