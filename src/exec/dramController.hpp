// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __DRAM_CONTROLLER_HPP__
#define __DRAM_CONTROLLER_HPP__

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include "statistics.hpp"
#include "logger.hpp"
#include "random.hpp"
#include "memory.hpp"

#define WORDS_IN_DRAM_BLOCK 256 /* must be a power of 2 */
#define DRAM_INDEX_MASK (256-WORDS_IN_DRAM_BLOCK) /* must be a power of 2 */

using namespace std;
using namespace boost;

typedef enum {
    /* Normal mode */
    DRAM_REQ_READ = 0,
    DRAM_REQ_WRITE,

    /* Indirect mode */

    /* In indirect mode, word_count is only used for the performance model.*/
    /* Actual data is written & read in a shared_ptr<void> pointer.        */
    /* The first address becomes the only unique key for the data.         */

    /* USE WITH CAUTION                                                    */
    /* NOTE : Overlaps of indirect data is ignored.                        */
    /* NOTE : Data is not copied. Better use it only for one cycle         */

    DRAM_REQ_READ_INDIRECT,
    DRAM_REQ_WRITE_INDIRECT
} dramReqType_t;

typedef enum {
    DRAM_NEW = 0,
    DRAM_WAIT,
    DRAM_DONE
} dramReqStatus_t;

class dramRequest {
public:
    /* read */
    dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count);
    /* normal mode - write */
    dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count, shared_array<uint32_t> wdata);
    /* dummy mode - write */
    dramRequest(maddr_t maddr, dramReqType_t request_type, uint32_t word_count, shared_ptr<void> wdata);

    inline dramReqStatus_t status() { return m_status; }
    inline const shared_array<uint32_t> read() { assert(m_request_type == DRAM_REQ_READ); return m_data; }
    inline const shared_ptr<void> read_indirect() { assert(m_request_type == DRAM_REQ_READ_INDIRECT); return m_indirect_data; }

    friend class dramController;

private:
    inline bool is_indirect() { return (m_request_type == DRAM_REQ_READ_INDIRECT || m_request_type == DRAM_REQ_WRITE_INDIRECT); }
    inline bool is_read() { return (m_request_type == DRAM_REQ_READ || m_request_type == DRAM_REQ_READ_INDIRECT); }
    dramReqType_t m_request_type;
    maddr_t m_maddr;
    uint32_t m_word_count;
    dramReqStatus_t m_status;
    shared_array<uint32_t> m_data;
    shared_ptr<void> m_indirect_data;;

};

class dram {
public:
    dram();
    ~dram();

    friend class dramController;

private:
    typedef map<uint64_t/*index*/, shared_array<uint32_t> > memSpace;
    typedef map<uint64_t/*address*/, shared_ptr<void> > indirectMemSpace;

    map<uint32_t/*mem space id*/, memSpace> m_memory;
    map<uint32_t/*mem space id*/, indirectMemSpace> m_indirect_memory;
    mutable recursive_mutex dram_mutex;

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
    entryQueue m_entries_waiting_for_ports;

};

#endif

