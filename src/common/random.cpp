// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <cstdlib>
#include <fstream>
#include "random.hpp"

static int fresh_random_seed() {
    int random_seed = 0xdeadbeef;
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    urandom.read(reinterpret_cast<char *>(&random_seed), sizeof(random_seed));
    if (urandom.fail()) random_seed = time(NULL);
    return random_seed;
}

int random_range(int max) throw() {
    double uniform_random = random()/(static_cast<double>(RAND_MAX)+1.0);
    return static_cast<int>(max * uniform_random);
}

double random_range_double(double max) throw() {
    double uniform_random = random()/(static_cast<double>(RAND_MAX)+1.0);
    return max * uniform_random;
}

int init_random(int seed) throw() {
    srandom(seed);
    return seed;
}

int init_random() throw() {
    return init_random(fresh_random_seed());
}
