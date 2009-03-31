// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __RANDOM_HPP__
#define __RANDOM_HPP__

// return a number selected u.a.r. from [0..max)
int random_range(int max) throw();

// init the system random number generator with the given seed
int init_random(int seed) throw();

// init the random number generator using system entropy and return the seed
int init_random() throw();

#endif // __RANDOM_HPP__
