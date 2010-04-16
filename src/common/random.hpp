// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __RAND_HPP__
#define __RAND_HPP__


#include "cstdint.hpp"
#include <boost/random.hpp>

using namespace boost;

class random_gen { 
private:
   boost::mt19937 rng;
   boost::uniform_01<boost::mt19937> gen;

public:
   random_gen(const uint32_t id, const uint32_t seed) throw();
   
   // return a natural number selected u.a.r. from [0..max)
   int random_range(int max) throw();
   
   // return a real number selected u.a.r. from [0..max)
   double random_range_double(double max) throw();

   uint32_t get_id() const throw();
private:
   uint32_t id;
};

#endif // __RAND_HPP__
