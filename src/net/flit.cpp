// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iostream>
#include <iomanip>
#include "flit.hpp"

uint64_t flit::next_uid = 0;

ostream &operator<<(ostream &out, const flit &f) {
    return out << "<" << hex << setfill('0') << setw(16) << f.get_data() << '>';
}

ostream &operator<<(ostream &out, const head_flit &f) {
    return out << "<f:" << hex << f.get_flow_id() << ",l:" << f.get_length()
               << ">";
}

