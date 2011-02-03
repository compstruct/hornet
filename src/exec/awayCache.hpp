// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __AWAY_CACHE_HPP__
#define __AWAY_CACHE_HPP__

#include "cache.hpp"
#include "logger.hpp"
#include "random.hpp"
#include "remoteMemory.hpp"
#include <boost/shared_ptr.hpp>
#include <map>

using namespace std;
using namespace boost;

class awayCache : public cache {
public:
    awayCache(const uint32_t numeric_id, const uint32_t level, const uint64_t &system_time,
          shared_ptr<tile_statistics> stats, logger &log, shared_ptr<random_gen> ran,
          cache_cfg_t cfgs);
    virtual ~awayCache();

    virtual mreq_id_t request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t target_level);

    virtual void update();
    virtual bool finish(mreq_id_t id);
    virtual void process();

#ifdef WRITE_NOW
    void invalidate(maddr_t addr);
#endif

private:
    typedef struct {
        uint32_t location;
        uint32_t target_level;
        bool     waiting_for_writeback; 
    } in_req_aux_info_t;

    typedef struct {
        bool valid;         /* entry is being used */
        bool ready;         /* data has come */
        bool on_the_fly;    /* data is coming (a request sent) */
        bool on_hold;       /* on hold until a write ra is done */
        uint64_t tag;
        uint32_t *data;
        uint64_t last_access;
        uint64_t timestamp;
    } cache_line_t;

    /* home of awayCache must be a remoteMemory instance */
    inline shared_ptr<remoteMemory> remote_memory() { return static_pointer_cast<remoteMemory, memory>(m_home); }

private:
    cache_line_t* cache_line(maddr_t addr);

private:
    map<mreq_id_t, in_req_aux_info_t> m_in_req_aux_info;
    map<uint64_t, cache_line_t*>  m_cache;

};

#endif
