// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __DRAM_CONTROLLER_HPP__
#define __DRAM_CONTROLLER_HPP__

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include "statistics.hpp"
#include "logger.hpp"
#include "random.hpp"
#include "memory_types.hpp"
#include "mem.hpp"

// C F FIX  
// was:
//#define WORDS_IN_DRAM_BLOCK 256 /* must be a power of 2 */
//#define DRAM_INDEX_MASK WORDS_IN_DRAM_BLOCK /* must be a power of 2 */
// changed to: 
#define WORDS_IN_DRAM_BLOCK 256 /* must be a power of 2 */
#define DRAM_INDEX_MASK 0x0FF /* must be a power of 2 */
// as a quick hack fix
// (really a better fix is warrented...)

using namespace std;
using namespace boost;

typedef enum {
    /* Normal mode */
    DRAM_REQ_READ = 0,
    DRAM_REQ_WRITE
} dramReqType_t;

typedef enum {
    DRAM_REQ_NEW = 0,
    DRAM_REQ_WAIT, /* request fetched. being processed */
    DRAM_REQ_DONE
} dramReqStatus_t;

class dramRequest {
public:
    /* read */
    dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count);
    /* normal write */
    dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count, shared_array<uint32_t> wdata);
    /* extended write */
    dramRequest(maddr_t maddr, dramReqType_t request_type, 
                uint32_t word_count, shared_array<uint32_t> wdata, 
                uint32_t aux_word_size, shared_ptr<void> aux_data);

    inline dramReqStatus_t status() { return m_status; }
    inline shared_array<uint32_t> read() { return m_data; }
    inline shared_ptr<void> read_aux() { return m_aux_data; }

    inline bool is_read() { return m_request_type == DRAM_REQ_READ; }
    inline maddr_t maddr() { return m_maddr; }

    /* cost breakdown study */
    inline void set_milestone_time(uint64_t time) { m_milestone_time = time; }
    inline uint64_t milestone_time() { return m_milestone_time; }

    friend class dramController;

private:
    dramReqType_t m_request_type;
    maddr_t m_maddr;
    uint32_t m_word_count;
    uint32_t m_aux_word_size;
    dramReqStatus_t m_status;
    shared_array<uint32_t> m_data;
    shared_ptr<void> m_aux_data;

    /* cost breakdown study */
    uint64_t m_milestone_time;

};

class dram {
public:
    dram();
    ~dram();

    friend class dramController;

private:
    typedef map<uint64_t/*start_address*/, shared_array<uint32_t> > memSpace;
    typedef map<uint32_t/*mem space id*/, memSpace> memSpaces;
    typedef map<maddr_t, shared_ptr<void> > auxMemSpaces;

    memSpaces m_memory;
    auxMemSpaces m_aux_memory;

    mutable recursive_mutex dram_mutex;

public:
    void mem_read_instant(uint32_t *, uint32_t, uint32_t, uint32_t, bool);
    void mem_write_instant(void * source, uint32_t mem_space, uint32_t mem_start, uint32_t mem_size, bool endianc);
    void mem_write_instant(shared_ptr<mem>, uint32_t, uint32_t, uint32_t);
};

class dramController {
public:
    dramController(uint32_t numeric_id, const uint64_t &system_time,
                   shared_ptr<tile_statistics> stats, logger &log, shared_ptr<random_gen> ran,
                   shared_ptr<dram> connected_dram,
                   uint32_t dram_controller_latency, uint32_t offchip_oneway_latency, uint32_t dram_latency,
                   uint32_t msg_header_size_in_words, uint32_t max_requests_in_flight, uint32_t bandwidth_in_words_per_cycle,
                   bool use_lock); 
    ~dramController();

    void tick_positive_edge();
    void tick_negative_edge();

    void request(shared_ptr<dramRequest> req);

    inline bool available() { return m_number_of_free_ports > 0; }

private:
    void dram_access(shared_ptr<dramRequest> req);
    void dram_access_safe(shared_ptr<dramRequest> req);

    typedef enum {
        ENTRY_PORT = 0,
        ENTRY_LATENCY,
        ENTRY_BANDWIDTH,
        ENTRY_DONE
    } entryStatus_t;

    typedef struct {
        entryStatus_t status;
        shared_ptr<dramRequest> request;
        uint32_t port;
        uint32_t remaining_latency_cycles;
        uint32_t remaining_words_to_transfer;
    } entry_t;

    typedef vector<shared_ptr<entry_t> > entryQueue;

    uint32_t m_id;
    const uint64_t &system_time;
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;

    shared_ptr<dram> m_dram;

    bool m_use_lock;
    /* simple latency model */
    uint32_t m_total_latency;
    uint32_t m_msg_header_size_in_words;
    uint32_t m_number_of_free_ports;
    uint32_t m_bandwidth_in_words_per_cycle;

    entryQueue m_entry_queue;

};

#endif

