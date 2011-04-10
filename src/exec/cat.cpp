// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cat.hpp"

catRequest::catRequest(maddr_t maddr, uint32_t sender) : m_status(CAT_NEW), m_maddr(maddr), m_sender(sender) {}

catRequest::~catRequest() {}

cat::cat(uint32_t num_nodes, const uint64_t &t, uint32_t latency, uint32_t allocation_unit_in_bytes) : 
    m_num_nodes(num_nodes), system_time(t), m_latency(latency), m_allocation_unit_in_bytes(allocation_unit_in_bytes) {}

cat::~cat() {}

/*****************/
/* catStripe     */
/*****************/

catStripe::catStripe(uint32_t num_nodes, const uint64_t& t, uint32_t latency, uint32_t allocation_unit_in_bytes) :
    cat(num_nodes, t, latency, allocation_unit_in_bytes) {}

catStripe::~catStripe() {}

void catStripe::request(shared_ptr<catRequest> req) {
    entry_t new_entry = { req, system_time + m_latency };
    m_entry_queue.push_back(new_entry);
}

void catStripe::tick_positive_edge() {}

void catStripe::tick_negative_edge() {
    while (!m_entry_queue.empty() && m_entry_queue.front().available_time <= system_time) {
        shared_ptr<catRequest> req = m_entry_queue.front().request;
        set_req_core(req, (req_maddr(req).address/m_allocation_unit_in_bytes)%m_num_nodes);
        set_req_status(req, CAT_DONE);
        m_entry_queue.erase(m_entry_queue.begin());
    }
}
    
/*****************/
/* catStatic     */
/*****************/

catStatic::catStatic(uint32_t num_nodes, const uint64_t& t, uint32_t latency, uint32_t allocation_unit_in_bytes, 
                     uint32_t synch_delay) : cat(num_nodes, t, latency, allocation_unit_in_bytes), m_synch_delay(synch_delay) {}

catStatic::~catStatic() {}

void catStatic::set(maddr_t maddr, uint32_t core, bool delay_to_synch) {
    map_entry_t new_map_entry = { core, (delay_to_synch)? system_time + m_synch_delay : 0 };
    assert(m_map[maddr.mem_space_id].count(maddr.address/m_allocation_unit_in_bytes) == 0);
    m_map[maddr.mem_space_id][maddr.address/m_allocation_unit_in_bytes] = new_map_entry;
}

void catStatic::request(shared_ptr<catRequest> req) {
    request_entry_t new_req_entry = { req, 0 };
    uint64_t index = req_maddr(req).address/m_allocation_unit_in_bytes;
    if (m_map[req_maddr(req).mem_space_id].count(index) > 0) {
        set_req_core(req, m_map[req_maddr(req).mem_space_id][index].core);
        if (m_map[req_maddr(req).mem_space_id][index].available_time > system_time) {
            new_req_entry.available_time = m_map[req_maddr(req).mem_space_id][index].available_time + m_latency;
        } else {
            new_req_entry.available_time = system_time + m_latency;
        }
    } else {
        set(req_maddr(req), 0);
        new_req_entry.available_time = system_time + m_synch_delay + m_latency;
        set_req_core(req, 0);
    }
}

void catStatic::tick_positive_edge() {}

void catStatic::tick_negative_edge() {
    vector<request_entry_t>::iterator it;
    for (it = m_req_entry_queue.begin(); it != m_req_entry_queue.end(); ++it) {
        if (it->available_time <= system_time) {
            set_req_status(it->request, CAT_DONE);
        }
    }
    while (!m_req_entry_queue.empty() && m_req_entry_queue.front().available_time <= system_time) {
        m_req_entry_queue.erase(m_req_entry_queue.begin());
    }
}
    
/*****************/
/* catFirstTouch */
/*****************/

catFirstTouch::catFirstTouch(uint32_t num_nodes, const uint64_t& t, uint32_t latency, uint32_t allocation_unit_in_bytes, 
                            uint32_t synch_delay) : cat(num_nodes, t, latency, allocation_unit_in_bytes), m_synch_delay(synch_delay) {}

catFirstTouch::~catFirstTouch() {}

void catFirstTouch::request(shared_ptr<catRequest> req) {
    uint32_t space = req_maddr(req).mem_space_id;
    uint64_t index = (req_maddr(req).address)/m_allocation_unit_in_bytes;
    request_entry_t new_req_entry = { req, 0 };
    if (m_map[space].count(index) == 0) {
        map_entry_t new_map_entry = { req_sender(req), system_time + m_synch_delay };
        m_map[space][index] = new_map_entry;
        new_req_entry.available_time = system_time + m_synch_delay + m_latency;
        set_req_core(req, req_sender(req));
    } else {
        set_req_core(req, m_map[space][index].core);
        if (m_map[space][index].available_time <= system_time) {
            new_req_entry.available_time = system_time + m_latency;
        } else {
            new_req_entry.available_time = m_map[space][index].available_time + m_latency;
        }
    }
    m_req_entry_queue.push_back(new_req_entry);
}

void catFirstTouch::tick_positive_edge() {}

void catFirstTouch::tick_negative_edge() {
    vector<request_entry_t>::iterator it;
    for (it = m_req_entry_queue.begin(); it != m_req_entry_queue.end(); ++it) {
        if (it->available_time <= system_time) {
            set_req_status(it->request, CAT_DONE);
        }
    }
    while (!m_req_entry_queue.empty() && m_req_entry_queue.front().available_time <= system_time) {
        m_req_entry_queue.erase(m_req_entry_queue.begin());
    }
}
    
