// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CLOCKABLE_HPP__
#define __CLOCKABLE_HPP__

#include "error.hpp"

class clockable {
public:
    virtual void tick_positive_edge() throw(err) = 0;
    virtual void tick_negative_edge() throw(err) = 0;
};

#endif // __CLOCKABLE_HPP__

