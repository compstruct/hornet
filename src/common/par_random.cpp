// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iostream>
#include <boost/random.hpp>
#include "par_random.hpp"

BoostRand::BoostRand( const uint32_t seed )
  : rng(), gen(rng) {
     rng.seed (seed);
}

int BoostRand::random_range_p (int max) {
   double uniform_random = gen();
   return static_cast<int>(max * uniform_random);
}

double BoostRand::random_range_double (double max) {
   double uniform_random = gen();
   return max * uniform_random;
}
