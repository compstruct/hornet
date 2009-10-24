// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEM_HPP__
#define __MEM_HPP__

#include <queue>
#include <boost/shared_ptr.hpp>
#include "cstdint.hpp"
#include "pe.hpp"
#include "error.hpp"
#include "logger.hpp"

using namespace std;
using namespace boost;

typedef enum {
    MR_LOAD_8, MR_LOAD_16, MR_LOAD_32, MR_STORE_8, MR_STORE_16, MR_STORE_32
} mem_req_type;

class mem_req {
public:
    uint32_t tag;
    mem_req_type type;
    uint32_t addr;
    uint32_t data;
};

class mem_resp {
public:
    uint32_t tag;
    uint32_t data;
};

class mem_ifc {
public:
typedef queue<mem_req> reqs_t;
typedef queue<mem_resp> resps_t;
public:
    explicit mem_ifc(shared_ptr<reqs_t> reqs,
                     shared_ptr<resps_t> resps) throw();
public:
    shared_ptr<reqs_t> requests;
    shared_ptr<resps_t> responses;
};

class mem {
public:
    virtual shared_ptr<mem_ifc> new_interface() throw() = 0;
};

#endif // __MEM_HPP__

