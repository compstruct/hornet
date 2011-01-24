// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MESSAGE_HPP__
#define __MESSAGE_HPP__

#include "memory.hpp"

typedef enum {
    MSG_MEM_REQ = 0,
    MSG_MEM_REP,
    MSG_RA_REQ,
    MSG_RA_REP,
    MSG_MIG,
    MSG_MIG_PRIORITY,
    NUM_MSG_TYPES
    /* TODO (Later) : Cache coherence protocol message */
} msg_type_t;

typedef struct {
    void* context;
} msg_mig_t;

typedef struct {
    mreq_id_t req_id;
    shared_ptr<memoryRequest> req;
    uint32_t target_level;
} msg_mem_t;

typedef struct {
    msg_type_t type;
    uint32_t dst;
    uint32_t flit_count;
    msg_mig_t mig_msg;
    msg_mem_t mem_msg;
    uint32_t src; /* no need to specify */
} msg_t;

/* TODO (Later) : message pool (no need to malloc every time (for the performance) ) */

#endif
