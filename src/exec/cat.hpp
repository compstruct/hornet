// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CAT_HPP__
#define __CAT_HPP__

#include <boost/shared_ptr.hpp>
#include "memoryRequest.hpp"

using namespace boost;

class cat {
public:
    cat();
    virtual ~cat();

    virtual uint32_t getCoreID(shared_ptr<memoryRequest> req) = 0;
    
};
#endif
