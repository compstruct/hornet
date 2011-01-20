// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "message.hpp"

message::message(void* context_ptr) {
    m_type = MSG_MIG;
    m_data.mig_msg.context_ptr = context_ptr;
}

message::message(mem_msg_dir_t dir, mreq_id_t req_id, mreq_type_t rw, maddr_t addr, uint32_t num_bytes, bool cacheable, void* data) {
    m_type = MSG_MEM;
    m_data.mem_msg.dir = dir;
    m_data.mem_msg.req_id = req_id;
    m_data.mem_msg.rw = rw;
    m_data.mem_msg.addr = addr;
    m_data.mem_msg.num_bytes = num_bytes;
    m_data.mem_msg.cacheable = cacheable;
    m_data.mem_msg.data = data;
}

message* new_message_with_context(void* context_ptr) {
    message* newMessage = new message(context_ptr);
    return newMessage;
}

message* new_message_with_memory_request(mreq_id_t req_id, mreq_type_t rw, maddr_t addr, uint32_t num_bytes, bool cacheable, void* data) {
    message* newMessage = new message(MEM_REQ, req_id, rw, addr, num_bytes, cacheable, data);
    return newMessage;
}

message* new_message_with_memory_reply(mreq_id_t req_id, mreq_type_t rw, maddr_t addr, uint32_t num_bytes, bool cacheable, void* data) {
    message* newMessage = new message(MEM_REPLY, req_id, rw, addr, num_bytes, cacheable, data);
    return newMessage;
}






