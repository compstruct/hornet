// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CACHE_HPP__
#define __CACHE_HPP__

#include <memory>
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
    REPLACE_CUSTOM 
} replacementPolicy_t;

typedef struct {
    bool claimed; 
    bool ready; 
    bool data_dirty;
    bool coherence_info_dirty;
    maddr_t start_maddr;;
    shared_array<uint32_t> data;
    std::shared_ptr<void> coherence_info;
    uint64_t last_access_time;
} cacheLine;

typedef enum {
    CACHE_REQ_READ = 0,
    CACHE_REQ_WRITE,      
    CACHE_REQ_UPDATE, 
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
                          std::shared_ptr<void> coherence_info_to_write = std::shared_ptr<void>());

    ~cacheRequest();

    inline cacheReqStatus_t status() { return m_status; }
    
    /* request information */
    inline cacheReqType_t request_type() { return m_request_type; }
    inline bool use_read_ports() { return m_request_type == CACHE_REQ_READ; }
    inline maddr_t maddr() { return m_maddr; }
    inline uint32_t word_count() { return m_word_count; }
    inline shared_array<uint32_t> data_to_write() { return m_data_to_write; }
    inline std::shared_ptr<void> coherence_info_to_write() { return m_coherence_info_to_write; }

    /* lines to return */
    inline std::shared_ptr<cacheLine> line_copy() { return m_line_copy; }
    inline std::shared_ptr<cacheLine> line_to_evict_copy() { return m_line_to_evict_copy; }

    /* convenient function - to reuse the cache request without making a new request */
    inline void reset() { assert(m_status != CACHE_REQ_WAIT); 
                          m_status = CACHE_REQ_NEW; 
                          m_line_copy = std::shared_ptr<cacheLine>();
                          m_line_to_evict_copy = std::shared_ptr<cacheLine>(); }

    /* Options */
    inline void set_unset_dirty_on_write(bool enable) { m_do_unset_dirty_on_write = enable; }
    inline void set_claim(bool enable) { m_do_claim = enable; }
    inline void set_evict(bool enable) { m_do_evict = enable; }

    inline void set_aux_info_for_coherence(std::shared_ptr<void> states) { m_aux_info_for_coherence = states; }
    inline std::shared_ptr<void> aux_info_for_coherence() { return m_aux_info_for_coherence; }

    /* for stats */
    inline void set_serialization_begin_time(uint64_t time) { m_serialization_begin_time = time; }
    inline uint64_t serialization_begin_time(){ return m_serialization_begin_time; }
    inline void set_operation_begin_time(uint64_t time) { m_operation_begin_time = time; }
    inline uint64_t operation_begin_time(){ return m_operation_begin_time; }

    friend class cache;

private:

    cacheReqType_t m_request_type;
    maddr_t m_maddr;
    uint32_t m_word_count;
    cacheReqStatus_t m_status;
    std::shared_ptr<cacheLine> m_line_copy;
    std::shared_ptr<cacheLine> m_line_to_evict_copy;
    std::shared_ptr<void> m_coherence_info_to_write;
    shared_array<uint32_t> m_data_to_write;

    std::shared_ptr<void> m_aux_info_for_coherence;

    bool m_do_unset_dirty_on_write;;
    bool m_do_claim;
    bool m_do_evict;

    /* cost breakdown study */
    uint64_t m_serialization_begin_time;
    uint64_t m_operation_begin_time;

};

class cache {
public:
    typedef std::shared_ptr<void> (*helperCopyCoherenceInfo) (std::shared_ptr<void>);
    typedef bool (*helperIsCoherenceHit) (std::shared_ptr<cacheRequest>, cacheLine&, const uint64_t& /* system_time */);
    typedef bool (*helperTestLineToEvict) (cacheLine&, const uint64_t& /* system_time */);
    typedef uint32_t (*helperReplacementPolicy) (vector<uint32_t>&, cacheLine const*, const uint64_t&, std::shared_ptr<random_gen> ran);
    typedef void (*helperHook) (std::shared_ptr<cacheRequest>, cacheLine&, const uint64_t& /* system_time */);

    cache(uint32_t cache_level, uint32_t numeric_id, const uint64_t &system_time, 
          std::shared_ptr<tile_statistics> stats, logger &log, std::shared_ptr<random_gen> ran, 
          uint32_t words_per_line, uint32_t total_lines, uint32_t associativity,
          replacementPolicy_t replacement_policy, 
          uint32_t hit_test_latency, uint32_t num_read_ports, uint32_t num_write_ports);
    ~cache();

    void tick_positive_edge();
    void tick_negative_edge();

    void request(std::shared_ptr<cacheRequest> req);

    inline bool read_port_available() { return m_available_read_ports > 0; }
    inline bool write_port_available() { return m_available_write_ports > 0; }

    inline void set_helper_copy_coherence_info (helperCopyCoherenceInfo fptr) { m_helper_copy_coherence_info = fptr; }
    inline void set_helper_is_coherence_hit (helperIsCoherenceHit fptr) { m_helper_is_coherence_hit = fptr; }
    inline void set_helper_can_evict_line (helperTestLineToEvict fptr) { m_helper_can_evict_line = fptr; }
    inline void set_helper_evict_need_action (helperTestLineToEvict fptr) { m_helper_evict_need_action = fptr; }
    inline void set_helper_replacement_policy (helperReplacementPolicy fptr) { 
        m_replacement_policy = REPLACE_CUSTOM;
        m_helper_replacement_policy = fptr; }

    inline void set_claim_hook (helperHook fptr) { m_claim_hook = fptr; }
    inline void set_read_hook (helperHook fptr) { m_read_hook= fptr; }
    inline void set_write_hook (helperHook fptr) { m_write_hook= fptr; }
    inline void set_update_hook (helperHook fptr) { m_update_hook= fptr; }
    inline void set_invalidate_hook (helperHook fptr) { m_invalidate_hook= fptr; }

    /* debug */
    void print_contents();

private:

    /**********************************************/
    /* Private types and private member functions */
    /**********************************************/
    typedef map<uint32_t/*index*/, cacheLine*> cacheTable;
    typedef enum {
        ENTRY_HIT_TEST,
        ENTRY_DONE
    } reqEntryStatus_t;
    typedef struct {
        reqEntryStatus_t status;
        std::shared_ptr<cacheRequest> request;
        maddr_t start_maddr;
        uint32_t idx;
        uint32_t remaining_hit_test_cycles;
    } reqEntry;
    typedef vector<std::shared_ptr<reqEntry> > reqQueue;
    typedef map<maddr_t, reqQueue> reqTable;

    /* convenient functions */
    inline uint32_t get_index(maddr_t maddr) { return (uint32_t)((maddr.address&m_index_mask)>>m_index_pos); }
    inline uint32_t get_offset(maddr_t maddr) { return (uint32_t)(maddr.address&m_offset_mask); }
    inline maddr_t get_start_maddr_in_line(maddr_t maddr) { maddr.address -= get_offset(maddr); return maddr; }
    std::shared_ptr<cacheLine> copy_cache_line(const cacheLine &line);

    /********************/
    /* member variables */
    /********************/

    /* configurations */
    uint32_t m_level;
    uint32_t m_id;
    const uint64_t &system_time;
    std::shared_ptr<tile_statistics> stats;
    logger &log;
    std::shared_ptr<random_gen> ran;
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

    /* helper functions to customize cache */
    helperCopyCoherenceInfo m_helper_copy_coherence_info;
    helperIsCoherenceHit m_helper_is_coherence_hit;
    helperTestLineToEvict m_helper_can_evict_line;
    helperTestLineToEvict m_helper_evict_need_action;
    helperReplacementPolicy m_helper_replacement_policy;
    helperHook m_claim_hook;
    helperHook m_invalidate_hook;
    helperHook m_read_hook;
    helperHook m_write_hook;
    helperHook m_update_hook;

    /* states */
    reqTable m_req_table;
    cacheTable m_cache;
    vector<std::shared_ptr<reqEntry> > m_ready_requests; /* separated for scheduling */

    /* the follwoing volatile states conveys the positive edge information to the negative edge */
    vector<std::tuple<uint32_t/*idx*/, uint32_t/*ways*/, std::shared_ptr<reqEntry> > > m_lines_to_invalidate; 
    vector<std::tuple<uint32_t/*idx*/, uint32_t/*ways*/, std::shared_ptr<reqEntry> > > m_lines_to_evict; 

public:
    cacheTable& get_cache_table() { return m_cache; }
};

#endif

