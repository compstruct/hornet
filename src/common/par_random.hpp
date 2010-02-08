// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __BRAND_HPP__
#define __BRAND_HPP__


#include <stdint.h>
#include <boost/random.hpp>

using namespace boost;

class BoostRand { 
private:
   boost::mt19937 rng;
   boost::uniform_01<boost::mt19937> gen;

public:
   BoostRand(const uint32_t seed);
   
   // return a natural number selected u.a.r. from [0..max)
   int random_range_p (int max);
   
   // return a real number selected u.a.r. from [0..max)
   double random_range_double(double max);

   int getUniform();

};

inline int BoostRand::getUniform() {
   return static_cast<int>(gen());
}

#endif // __BRAND_HPP__
