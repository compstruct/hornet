// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MESSAGE_HPP__
#define __MESSAGE_HPP__

#include "memory.hpp"

#define NUM_MSG_CORE 2

typedef enum {
    /* must maintain the order */
    MSG_MEM_REQ = 0,
    MSG_MEM_REP,
    MSG_RA_REQ,
    MSG_RA_REP,
    /* TODO (Later) : Cache coherence protocol message */
    MSG_CORE_0,
    MSG_CORE_1,
    NUM_MSG_TYPES
} msg_type_t;

typedef struct {
    void* context;
} msg_core_t;

typedef struct {
    mreq_id_t req_id;
    shared_ptr<memoryRequest> req;
    uint32_t target_level;
} msg_mem_t;

typedef struct {
    msg_type_t type;
    uint32_t dst;
    uint32_t flit_count;
    msg_core_t core_msg;
    msg_mem_t mem_msg;
    uint32_t src; /* no need to specify */
} msg_t;

/* TODO (Later) : message pool (no need to malloc every time (for the performance) ) */

#endif
