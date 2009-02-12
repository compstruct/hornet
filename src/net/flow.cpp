// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iostream>
#include <iomanip>
#include "flow.hpp"

ostream &operator<<(ostream &out, const flow_id &id) {
    return out << hex << setfill('0') << setw(8) << id.id;
}

