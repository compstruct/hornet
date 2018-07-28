// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iostream>
#include <boost/random.hpp>
#include "random.hpp"

using namespace std;

random_gen::random_gen(uint32_t new_id, const uint32_t seed)
    : rng(seed), gen(rng), id(new_id) { }

uint32_t random_gen::get_id() const { return id; }

int random_gen::random_range(int max) {
    double uniform_random = gen();
    return static_cast<int>(max * uniform_random);
}

double random_gen::random_range_double(double max) {
    double uniform_random = gen();
    return max * uniform_random;
}
