// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_TYPES_HPP__
#define __MEMORY_TYPES_HPP__

#include <stdint.h>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <iostream>

using namespace std;
using namespace boost;

typedef enum {
    MEM_PRIVATE_SHARED_MSI_MESI = 0,
    MEM_PRIVATE_SHARED_LCC = 1, /* obsolete */
    MEM_PRIVATE_SHARED_EMRA = 2, /* obsolete */
    MEM_SHARED_SHARED_EMRA = 3,
    MEM_SHARED_SHARED_LCC = 4,
    MEM_PRIVATE_SHARED_PTI = 5
} memoryType_t;

typedef enum {
    TOP_AND_BOTTOM_TO_DRAM = 0,
    BOUNDARY_TO_DRAM = 1
} dramLocationType_t;

typedef struct {
    uint32_t space;
    uint64_t address;
} maddr_t;

bool operator<(const maddr_t &left, const maddr_t &right);
bool operator==(const maddr_t &left, const maddr_t &right);
ostream& operator<<(ostream& output, const maddr_t &right);

typedef enum {
    REQ_NEW = 0,
    REQ_RETRY,
    REQ_WAIT,
    REQ_DONE,
    REQ_MIGRATE
} memReqStatus_t;

#endif
