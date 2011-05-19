// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CACHE_HPP__
#define __CACHE_HPP__

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include "statistics.hpp"
#include "logger.hpp"
#include "random.hpp"
#include "memory_types.hpp"

using namespace std;
using namespace boost;

/* do not change the order */
typedef enum {
    REPLACE_LRU = 0,
    REPLACE_RANDOM = 1,
    REPLACE_CUSTOM /* using memory-specific information */
} replacementPolicy_t;

typedef struct {
    bool empty; /* if yes, no acitivity at all on this line. anyone can take this line */
    bool valid; /* if both empty and valid is false, the line is reserved and waiting for data */
    bool dirty;
    maddr_t start_maddr;;
    shared_array<uint32_t> data;
    shared_ptr<void> coherence_info;
    uint64_t last_access_time;
} cacheLine;

typedef enum {
    CACHE_REQ_READ = 0,
    CACHE_REQ_WRITE,      
    CACHE_REQ_UPDATE, /* update and validate the cache line */ 
    CACHE_REQ_INVALIDATE

} cacheReqType_t;

typedef enum {
    CACHE_REQ_NEW = 0,
    CACHE_REQ_WAIT,
    CACHE_REQ_HIT,
    CACHE_REQ_MISS
} cacheReqStatus_t;

class cacheRequest {
public:

    explicit cacheRequest(maddr_t maddr, cacheReqType_t request_type, 
                 uint32_t word_count = 0,
                 shared_array<uint32_t> data_to_write = shared_array<uint32_t>(), 
                 shared_ptr<void> coherence_info_to_write = shared_ptr<void>());

    ~cacheRequest();

    inline cacheReqStatus_t status() { return m_status; }

    inline shared_ptr<cacheLine> line_copy() { return m_line_copy; }
    inline shared_ptr<cacheLine> victim_line_copy() { return m_victim_line_copy; }

    inline cacheReqType_t request_type() { return m_request_type; }
    inline maddr_t maddr() { return m_maddr; }

    inline void reset() { assert(m_status != CACHE_REQ_WAIT); 
                          m_status = CACHE_REQ_NEW; 
                          m_line_copy = shared_ptr<cacheLine>();
                          m_victim_line_copy = shared_ptr<cacheLine>(); }

    /* to support architecture-specific functionalities */
    inline void set_clean_write(bool enable = true) { m_do_clean_write = enable; }
    inline void set_reserve(bool enable) { m_do_reserve= enable; }

    friend class cache;

private:
    inline bool use_read_ports() { return m_request_type == CACHE_REQ_READ ||
                                          m_request_type == CACHE_REQ_INVALIDATE; }

    cacheReqType_t m_request_type;
    maddr_t m_maddr;
    uint32_t m_word_count;

    cacheReqStatus_t m_status;
    shared_ptr<cacheLine> m_line_copy;
    shared_ptr<cacheLine> m_victim_line_copy;
    shared_ptr<void> m_coherence_info_to_write;
    shared_array<uint32_t> m_data_to_write;

    bool m_do_clean_write;
    bool m_do_reserve;

};

class cache {
public:
    typedef shared_ptr<void> (*helperCopyCoherenceInfo) (shared_ptr<void>);
    typedef bool (*helperIsHit) (shared_ptr<cacheRequest>, cacheLine&, const uint64_t&);
    typedef void (*helperReserveLine) (cacheLine&);
    typedef bool (*helperCanEvictLine) (cacheLine&, const uint64_t&);
    typedef uint32_t (*helperReplacementPolicy) (vector<uint32_t>&, cacheLine const*, const uint64_t&, shared_ptr<random_gen> ran);

    cache(uint32_t cache_level, uint32_t numeric_id, const uint64_t &system_time, 
          shared_ptr<tile_statistics> stats, logger &log, shared_ptr<random_gen> ran, 
          uint32_t words_per_line, uint32_t total_lines, uint32_t associativity,
          replacementPolicy_t replacement_policy, 
          uint32_t hit_test_latency, uint32_t num_read_ports, uint32_t num_write_ports);
    ~cache();

    void tick_positive_edge();
    void tick_negative_edge();

    void print_contents();
    void request(shared_ptr<cacheRequest> req);

    inline bool read_port_available() { return m_available_read_ports > 0; }
    inline bool write_port_available() { return m_available_write_ports > 0; }

    inline void set_helper_copy_coherence_info (helperCopyCoherenceInfo fptr) { m_helper_copy_coherence_info = fptr; }
    inline void set_helper_is_hit (helperIsHit fptr) { m_helper_is_hit = fptr; }
    inline void set_helper_reserve_line (helperReserveLine fptr) { m_helper_reserve_line = fptr; }
    inline void set_helper_can_evict_line (helperCanEvictLine fptr) { m_helper_can_evict_line = fptr; }
    inline void set_helper_replacement_policy (helperReplacementPolicy fptr) { 
        m_replacement_policy = REPLACE_CUSTOM;
        m_helper_replacement_policy = fptr; }

private:
    typedef map<uint32_t/*index*/, cacheLine*> cacheTable;

    typedef enum {
        ENTRY_HIT_TEST,
        ENTRY_DONE
    } reqEntryStatus_t;

    typedef struct {
        reqEntryStatus_t status;
        shared_ptr<cacheRequest> request;
        uint32_t remaining_hit_test_cycles;
        bool need_to_evict_and_reserve;
    } reqEntry;

    typedef vector<reqEntry> reqQueue;
    typedef map<maddr_t, reqQueue> reqTable;

    inline uint32_t get_index(maddr_t maddr) { return (uint32_t)((maddr.address&m_index_mask)>>m_index_pos); }
    inline uint32_t get_offset(maddr_t maddr) { return (uint32_t)(maddr.address&m_offset_mask); }
    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { maddr.address -= get_offset(maddr); return maddr; }

    shared_ptr<cacheLine> copy_cache_line(const cacheLine &line);

    uint32_t m_level;
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

    uint32_t m_available_read_ports;
    uint32_t m_available_write_ports;

    helperCopyCoherenceInfo m_helper_copy_coherence_info;
    helperIsHit m_helper_is_hit;
    helperReserveLine m_helper_reserve_line;
    helperCanEvictLine m_helper_can_evict_line;
    helperReplacementPolicy m_helper_replacement_policy;

    reqTable m_req_table;
    cacheTable m_cache;

    set<tuple<uint32_t/*idx*/, uint32_t/*ways*/> > m_lines_to_evict; /* and leave it empty */
    set<tuple<uint32_t/*idx*/, uint32_t/*way*/, maddr_t> > m_lines_to_evict_and_reserve; /* evict and reserve it for another line */

};

#endif

