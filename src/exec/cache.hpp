// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CACHE_HPP__
#define __CACHE_CPP__

#include "memory.hpp"
#include "logger.hpp"
#include "random.hpp"
#include <boost/shared_ptr.hpp>
#include <map>

using namespace std;
using namespace boost;

class cache : public memory {
public:
    typedef enum {
        CACHE_RANDOM,
        CACHE_LRU
    } cache_policy_t;

    typedef struct {
        uint32_t associativity;
        uint32_t block_size_bytes;
        uint32_t total_block;
        uint32_t process_time;
        uint32_t block_per_cycle;
        cache_policy_t policy;
    } cache_cfg_t;

    cache(const uint32_t numeric_id, const uint32_t level, const uint64_t &system_time,
          logger &log, shared_ptr<random_gen> ran,
          cache_cfg_t cfgs);
    virtual ~cache();

    virtual mreq_id_t request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t target_level);
    virtual bool ready(mreq_id_t id);
    virtual bool finish(mreq_id_t id);

    virtual void initiate();
    virtual void update();
    virtual void process();

    void set_home_memory(shared_ptr<memory> home);
    inline void set_home_location(uint32_t location, uint32_t level) { m_home_location = location; m_home_level = level; }

private:
    typedef enum {
        REQ_INIT,    /* just requested */
        REQ_BUSY,    /* working on cache logic */
        REQ_WAIT, 
        REQ_DONE
    } req_status_t;

    typedef struct {
        req_status_t status;
        uint32_t     remaining_process_time;
        shared_ptr<memoryRequest> req;
    } in_req_entry_t;

    typedef struct {
        bool valid;         /* entry is being used */
        bool ready;         /* data has come */
        bool doomed;        /* will be evicted (write_back issued) */
        bool on_the_fly;    /* data is coming (a request sent) */
        uint64_t tag;
        uint32_t *data;
        uint64_t last_access;
        bool dirty;
    } cache_line_t;

private:
    inline uint64_t get_tag(maddr_t addr) { return (addr&m_tag_mask)>>m_tag_pos; }
    inline uint64_t get_index(maddr_t addr) { return (addr&m_index_mask)>>m_index_pos; }
    inline uint64_t get_offset(maddr_t addr) { return (addr&m_offset_mask); }

    cache_line_t* cache_line(maddr_t addr);

private:
    cache_cfg_t m_cfgs;
    shared_ptr<memory> m_home;
    uint32_t m_home_location;
    uint32_t m_home_level;

    map<mreq_id_t, in_req_entry_t> m_in_req_table;
    map<mreq_id_t, shared_ptr<memoryRequest> > m_out_req_table;
    map<uint64_t, cache_line_t*>  m_cache;

    uint64_t m_offset_mask;
    uint64_t m_index_mask;
    uint32_t m_index_pos;
    uint64_t m_tag_mask;
    uint32_t m_tag_pos;

};

#endif
