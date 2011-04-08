// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CACHE_HPP__
#define __CACHE_HPP__

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include "statistics.hpp"
#include "logger.hpp"
#include "random.hpp"
#include "memory.hpp"

using namespace std;
using namespace boost;

typedef enum {
    REPLACE_RANDOM = 0,
    REPLACE_LRU
} replacementPolicy_t;

typedef struct {
    bool valid;
    bool dirty;
    bool reserved;
    maddr_t first_maddr;
    shared_array<uint32_t> data;
    shared_ptr<void> coherence_info;
    uint64_t last_access_time;
} cacheLine_t;

typedef enum {
    CACHE_REQ_READ = 0,
    CACHE_REQ_WRITE,     /* cache only returns a pointer to write to, not writes data itself */
    CACHE_REQ_ADD_LINE,  /* cache only returns a pointer to write to, not writes data itself */
    CACHE_REQ_INVALIDATE /* also work for flush back requests */
} cacheReqType_t;

typedef enum {

    CACHE_NEW = 0,

    /* cache is working */
    CACHE_WAIT,

    /* a cache hit for a read or write, or an invalidate is done (the requester may need to check the dirty bit) */
    /* or a new line is added to the cache */
    CACHE_HIT,

    /* a cache miss. if it need to evict a line to bring in the requested line, the information of victim is available */
    /* or a new line couldn't be added because the cache is full */
    /* or could not invalidate because the line does not exist */
    CACHE_MISS

} cacheReqStatus_t;

class cacheRequest {
public:

    /* Because cache does not know about the cache coherence protocol, it can't do writes by itself */
    /* The requester can and must write into the returned cache line on a write hit */
    cacheRequest(maddr_t maddr, cacheReqType_t request_type, shared_ptr<void> coherence_info_to_set = shared_ptr<void>());
    ~cacheRequest();

    inline cacheReqStatus_t status() { return m_status; }

    /* cache_line() is valid only at the cycle when status() becomes CACHE_HIT or CACHE_MISS */
    inline cacheLine_t* cache_line() { return m_cache_line; }

    inline bool need_invalidate() { assert(status()==CACHE_MISS); return m_need_to_invalidate; }
    inline maddr_t maddr_to_invalidate() { return m_maddr_to_invalidate; }

    friend class cache;

private:
    cacheReqType_t m_request_type;
    maddr_t m_maddr;
    cacheReqStatus_t m_status;
    cacheLine_t* m_cache_line;
    bool m_need_to_invalidate;
    maddr_t m_maddr_to_invalidate;
    shared_ptr<void> m_coherence_info_to_set;

};

class cache {
public:
    cache(uint32_t numeric_id, const uint64_t &system_time, 
          shared_ptr<tile_statistics> stats, logger &log, shared_ptr<random_gen> ran, 
          uint32_t words_per_line, uint32_t total_lines, uint32_t associativity,
          replacementPolicy_t replacement_policy, 
          uint32_t hit_test_latency, uint32_t num_read_ports, uint32_t num_write_ports);
    ~cache();

    void tick_positive_edge();
    void tick_negative_edge();

    void request(shared_ptr<cacheRequest> req);

private:
    typedef enum {
        ENTRY_PORT = 0,
        ENTRY_HIT_TEST,
        ENTRY_DONE
    } entryStatus_t;

    typedef struct {
        entryStatus_t status;
        shared_ptr<cacheRequest> request;
        uint32_t remaining_hit_test_cycles;
    } entry_t;

    typedef struct {
        uint32_t index;
        uint32_t way;
    } linePosition_t;

    typedef vector<shared_ptr<entry_t> > entryQueue;
    typedef map<uint64_t /*address*/, entryQueue> entryTable;

    inline uint32_t get_index(maddr_t maddr) { return (uint32_t)((maddr.address&m_index_mask)>>m_index_pos); }
    inline uint32_t get_offset(maddr_t maddr) { return (uint32_t)(maddr.address&m_offset_mask); }
    inline maddr_t get_first_maddr(maddr_t maddr) { maddr.address -= get_offset(maddr); return maddr; }
    cacheLine_t* cache_line(maddr_t maddr); 

    uint32_t m_id;
    const uint64_t &system_time;
    shared_ptr<tile_statistics> stats;
    logger &log;
    shared_ptr<random_gen> ran;

    uint32_t m_words_per_line;
    uint32_t m_total_lines;
    uint32_t m_associativity;
    replacementPolicy_t m_replacement_policy;
    uint32_t m_hit_test_latency;

    uint64_t m_offset_mask;
    uint64_t m_index_mask;
    uint32_t m_index_pos;

    entryQueue m_entries_waiting_for_read_ports;
    entryQueue m_entries_waiting_for_write_ports;
    /* NOTE: we assume an infinite cache request table. */
    /* Although the table size is indeterministic, the finite number of read & write ports helps making it reasonable */
    map<uint32_t/*mem space id*/, entryTable> m_entry_tables;
    map<uint32_t/*index*/, cacheLine_t* > m_cache;
    uint32_t m_number_of_free_read_ports;
    uint32_t m_number_of_free_write_ports;

    /* for the performance's sake, store lines to purge at the negative tick (rather than iterating through the cache) */
    vector<linePosition_t> m_lines_to_purge;

};

#endif
