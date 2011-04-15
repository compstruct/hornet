// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "memory_types.hpp"

bool operator<(const maddr_t &left, const maddr_t &right) {
    return left.space < right.space || (left.space == right.space && left.address < right.space);
}

bool operator==(const maddr_t &left, const maddr_t &right) {
    return left.space == right.space && left.address == right.address;
}

ostream& operator<<(ostream& output, const maddr_t &right) {
    output << hex << right.space << ":" << right.address << dec;
    return output;
}

