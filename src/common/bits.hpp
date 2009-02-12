// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __BITS_HPP__
#define __BITS_HPP__

#include <cassert>
#include "cstdint.hpp"

template<class V> unsigned num_bits() { return sizeof(V) << 3; }

template <class V>
inline V bits(V arg, unsigned hi, unsigned lo) throw() {
    assert(hi < num_bits<V>());
    assert(lo <= 31);
    assert(lo <= hi);
    return (arg >> lo) & ((((V) 1) << (1 + hi - lo)) - 1);
}

template <class V>
inline V combine(V templ, V arg, V mask) {
    return templ ^ ((templ ^ arg) & mask);
}

template <class V>
inline V splice(V templ, V arg, unsigned hi, unsigned lo)
    throw () {
    V mask = ((((V) 2) << (hi - lo)) - 1) << lo;
    assert(((arg << lo) & ~mask) == 0);
    return combine(templ, arg << lo, mask);
}

#endif

