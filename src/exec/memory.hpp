// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

#include <memory>
#include <boost/shared_array.hpp>
#include "statistics.hpp"
#include "logger.hpp"
#include "random.hpp"
#include "dramController.hpp"
#include "messages.hpp"

#include "memory_types.hpp"

using namespace std;
using namespace boost;

class memoryRequest {
public:
    /* for reads */
    memoryRequest(maddr_t maddr, uint32_t word_count);
    memoryRequest(maddr_t maddr, uint32_t word_count, std::shared_ptr<std::shared_ptr<void> > per_mem_instr_info);
    /* for writes */
    memoryRequest(maddr_t maddr, uint32_t word_count, shared_array<uint32_t> wdata);
    memoryRequest(maddr_t maddr, uint32_t word_count, shared_array<uint32_t> wdata, std::shared_ptr<std::shared_ptr<void> > per_mem_instr_info);
    ~memoryRequest();

    inline memReqStatus_t status() { return m_status; }
    inline maddr_t maddr() { return m_maddr; }
    inline shared_array<uint32_t> data() { return m_data; }
    inline bool is_read() { return m_is_read; }
    inline uint32_t home() { return m_home; }
    inline uint32_t word_count() { return m_word_count; }

    inline void set_per_mem_instr_runtime_info(std::shared_ptr<std::shared_ptr<void> > info) { m_per_mem_instr_runtime_info = info; }
    inline std::shared_ptr<std::shared_ptr<void> > per_mem_instr_runtime_info() { return m_per_mem_instr_runtime_info; }

    friend class memory;

private:

    memReqStatus_t m_status;
    bool m_is_read;
    maddr_t m_maddr;
    uint32_t m_word_count;
    shared_array<uint32_t> m_data;
    uint32_t m_home;

    std::shared_ptr<std::shared_ptr<void> > m_per_mem_instr_runtime_info;
};

class memory {
public:
    memory(uint32_t numeric_id, 
           const uint64_t &system_time,
           std::shared_ptr<tile_statistics> stats,
           logger &log,
           std::shared_ptr<random_gen> ran);
    virtual ~memory();

    virtual void request(std::shared_ptr<memoryRequest> req) = 0;
    virtual void tick_positive_edge() = 0;
    virtual void tick_negative_edge() = 0;

    virtual uint32_t number_of_mem_msg_types() = 0;

    void add_local_dram_controller(std::shared_ptr<dram> connected_dram, 
                                   uint32_t dram_controller_latency, uint32_t offchip_oneway_latency, uint32_t dram_latency,
                                   uint32_t msg_header_size_in_words, uint32_t max_requests_in_flight, 
                                   uint32_t bandwidth_in_words_per_cycle, bool use_lock);
    void set_remote_dram_controller(uint32_t location);

    inline void set_core_send_queues(map<uint32_t, std::shared_ptr<messageQueue> > queues) { m_core_send_queues = queues; }
    inline void set_core_receive_queues(map<uint32_t, std::shared_ptr<messageQueue> > queues) { m_core_receive_queues = queues; }

    /* force quit background working as the simulation ends */
    inline void turn_off() { m_power = false; }

protected:
    inline void set_req_status(std::shared_ptr<memoryRequest> req, memReqStatus_t status) { req->m_status = status; }
    inline void set_req_data(std::shared_ptr<memoryRequest> req, shared_array<uint32_t> data) { req->m_data = data; }
    inline void set_req_home(std::shared_ptr<memoryRequest> req, uint32_t home) { req->m_home = home; }

    uint32_t m_id;
    const uint64_t &system_time;
    std::shared_ptr<tile_statistics> stats;
    logger &log;
    std::shared_ptr<random_gen> ran;

    dramController* m_dramctrl;
    uint32_t m_dramctrl_location;

    map<uint32_t/*msg type*/, std::shared_ptr<messageQueue> > m_core_send_queues;
    map<uint32_t/*msg type*/, std::shared_ptr<messageQueue> > m_core_receive_queues;

    bool m_power;

};

#endif
