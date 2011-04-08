// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "dramController.hpp"

dramRequest::dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count) :
    m_request_type(request_type), m_maddr(maddr), m_word_count(word_count), m_status(DRAM_NEW), 
    m_data(shared_array<uint32_t>()), m_indirect_data(shared_ptr<void>()) 
{
    assert(m_request_type == DRAM_REQ_READ || m_request_type == DRAM_REQ_READ_INDIRECT);
}

dramRequest::dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count, shared_array<uint32_t> wdata) :
    m_request_type(request_type), m_maddr(maddr), m_word_count(word_count), m_status(DRAM_NEW), 
    m_data(wdata), m_indirect_data(shared_ptr<void>()) 
{
    assert(m_request_type == DRAM_REQ_WRITE);
}

dramRequest::dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count, shared_ptr<void> wdata) :
    m_request_type(request_type), m_maddr(maddr), m_word_count(word_count), m_status(DRAM_NEW), 
    m_data(shared_array<uint32_t>()), m_indirect_data(wdata) 
{
    assert(m_request_type == DRAM_REQ_WRITE_INDIRECT);
}

dram::dram() {}

dram::~dram() {}

dramController::dramController(uint32_t id, const uint64_t &t,
                               shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r,
                               shared_ptr<dram> connected_dram,
                               uint32_t dram_controller_latency, uint32_t offchip_oneway_latency, uint32_t dram_latency,
                               uint32_t msg_header_size_in_words,
                               uint32_t max_requests_in_flight, uint32_t bandwidth_in_words_per_cycle, bool use_lock) :
    m_id(id), system_time(t), stats(st), log(l), ran(r), m_dram(connected_dram), m_use_lock(use_lock),
    m_total_latency(dram_controller_latency + 2*offchip_oneway_latency + dram_latency),
    m_msg_header_size_in_words(msg_header_size_in_words),
    m_number_of_free_ports(max_requests_in_flight), m_bandwidth_in_words_per_cycle(bandwidth_in_words_per_cycle)
{}

dramController::~dramController() {}

void dramController::request(shared_ptr<dramRequest> req) {
    req->m_status = DRAM_WAIT;
    shared_ptr<entry_t> new_entry = shared_ptr<entry_t>(new entry_t);
    new_entry->status = ENTRY_PORT;
    new_entry->request = req;
    if (m_number_of_free_ports > 0) {
        --m_number_of_free_ports;
        new_entry->remaining_words_to_transfer = req->m_word_count;
        if (m_total_latency > 0) {
            new_entry->status = ENTRY_LATENCY;
            new_entry->remaining_latency_cycles = m_total_latency;
        } else {
            new_entry->status = ENTRY_BANDWIDTH;
        }
        m_entry_queue.push_back(new_entry);
    } else {
        m_entries_waiting_for_ports.push_back(new_entry);
    }
}

void dramController::tick_positive_edge() {
    uint32_t available_bandwidth = m_bandwidth_in_words_per_cycle;
    /* bandwidth constraints */
    for (entryQueue::iterator it_entry = m_entry_queue.begin(); 
         it_entry != m_entry_queue.end() && (*it_entry)->status == ENTRY_BANDWIDTH && available_bandwidth > 0; ++it_entry) {
        if (available_bandwidth < (*it_entry)->remaining_words_to_transfer) {
            (*it_entry)->remaining_words_to_transfer -= available_bandwidth;
            break;
        } else {
            (*it_entry)->status = ENTRY_DONE;
            available_bandwidth -= (*it_entry)->remaining_words_to_transfer;
        }
    }
    /* process */
    while (!m_entry_queue.empty() && m_entry_queue.front()->status == ENTRY_DONE) {
        if (m_use_lock) {
            dram_access_safe(m_entry_queue.front()->request);
        } else {
            dram_access(m_entry_queue.front()->request);
        }
        m_entry_queue.erase(m_entry_queue.begin());
        ++m_number_of_free_ports;
    }

}

void dramController::tick_negative_edge() {

    /* advance */
    for (entryQueue::iterator it_entry = m_entry_queue.begin(); 
         it_entry != m_entry_queue.end() && (*it_entry)->status == ENTRY_LATENCY; ++it_entry) {
        if (--((*it_entry)->remaining_latency_cycles) == 0) {
            (*it_entry)->status = ENTRY_BANDWIDTH;
        }
    }

    /* port serialization */
    while(m_number_of_free_ports > 0 && !m_entries_waiting_for_ports.empty()) {
        shared_ptr<entry_t> entry = m_entries_waiting_for_ports.front();
        --m_number_of_free_ports;
        if (m_total_latency > 0) {
            entry->status = ENTRY_LATENCY;
            entry->remaining_latency_cycles = m_total_latency;
        } else {
            entry->status = ENTRY_BANDWIDTH;
        }
        m_entry_queue.push_back(entry);
        m_entries_waiting_for_ports.erase(m_entries_waiting_for_ports.begin());
    }
        
}

void dramController::dram_access_safe(shared_ptr<dramRequest> req) {
    unique_lock<recursive_mutex> lock(m_dram->dram_mutex);
    dram_access(req);
}

void dramController::dram_access(shared_ptr<dramRequest> req) {
    uint32_t mem_space_id = req->m_maddr.mem_space_id;
    if (req->is_indirect()) {
        uint64_t address = req->m_maddr.address;
        if (m_dram->m_indirect_memory[mem_space_id].count(address) == 0) {
            m_dram->m_indirect_memory[mem_space_id][address] = shared_ptr<void>();
        }
        if (req->is_read()) {
            req->m_indirect_data = m_dram->m_indirect_memory[mem_space_id][address];
        } else {
            m_dram->m_indirect_memory[mem_space_id][address] = req->m_indirect_data;
        }
    } else {
        if (req->is_read()) {
            req->m_data = shared_array<uint32_t>(new uint32_t[req->m_word_count]);
        }
        for (uint32_t it_word = 0; it_word != req->m_word_count; ++it_word) {
            uint64_t address = req->m_maddr.address + it_word;
            uint64_t offset = address & DRAM_INDEX_MASK;
            uint64_t index = address - offset;
            if (m_dram->m_memory[mem_space_id].count(index) == 0) {
                m_dram->m_memory[mem_space_id][index] = shared_array<uint32_t>(new uint32_t[WORDS_IN_DRAM_BLOCK]);
                for (uint32_t i = 0; i < WORDS_IN_DRAM_BLOCK; ++i) {
                    m_dram->m_memory[mem_space_id][index][i] = 0;
                }
            }
            if (req->is_read()) {
                req->m_data[it_word] = m_dram->m_memory[mem_space_id][index][offset];
            } else {
                m_dram->m_memory[mem_space_id][index][offset] = req->m_data[it_word];
            }
        }
    }
}




