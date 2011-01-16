// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MESSAGE_HPP__
#define __MESSAGE_HPP__

#include "memory.hpp"

typedef enum {
    MSG_MIG = 0,
    MSG_MEM
    /* TODO (Later) : Cache coherence protocol message */
} msg_type_t;

typedef enum {
    MEM_REQ = 0,
    MEM_REPLY
} mem_msg_dir_t;

typedef struct {
    void* context_ptr;
} mig_msg_t;

typedef struct {
    mem_msg_dir_t dir;
    mem_req_id_t req_id;
    mem_req_type_t rw;
    mem_addr_t addr;
    uint32_t num_bytes;
    bool cacheable;
    void* data;
} mem_msg_t;

class message {
public:
    message(void* thread_ptr);
    message(mem_msg_dir_t dir, mem_req_id_t req_id, mem_req_type_t rw, mem_addr_t addr, uint32_t num_bytes, bool cacheable, void* data);
    ~message();

    /* Introspection */
    inline msg_type_t getType() {return m_type;}

    /* Retrieval */
    inline mig_msg_t getMigMsg() { assert(m_type == MSG_MIG); return m_data.mig_msg; }
    inline mem_msg_t getMemMsg() { assert(m_type == MSG_MEM); return m_data.mem_msg; }

protected:
    typedef union {
        mig_msg_t mig_msg;
        mem_msg_t mem_msg;
    } data_t;

    msg_type_t m_type;
    data_t m_data;

};

message* messageWithContext(void* context_ptr);
message* messageWithMemoryRequest(mem_req_id_t req_id, mem_req_type_t rw, mem_addr_t addr, uint32_t num_bytes, bool cacheable, void* data);
message* messageWithMemoryReply(mem_req_id_t req_id, mem_req_type_t rw, mem_addr_t addr, uint32_t num_bytes, bool cacheable, void* data);

/* TODO (Phase 6) : Implement message pool for performance optimization (elimiating malloc/deletes) */

#endif
