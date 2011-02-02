// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __HOME_CACHE_HPP__
#define __HOME_CACHE_HPP__

#include "cache.hpp"
#include "logger.hpp"
#include "random.hpp"
#include <boost/shared_ptr.hpp>
#include <map>

using namespace std;
using namespace boost;

class homeCache : public cache {
public:
    homeCache(const uint32_t numeric_id, const uint32_t level, const uint64_t &system_time,
          shared_ptr<tile_statistics> stats, logger &log, shared_ptr<random_gen> ran,
          cache_cfg_t cfgs);
    virtual ~homeCache();

    virtual mreq_id_t request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t target_level);

    virtual void update();
    virtual void process();

    /* these functions are only valid in the cycle when the request becomes ready */
    void update_timestamp(mreq_id_t id, uint64_t timestamp);
    uint64_t get_timestamp(mreq_id_t id);
    uint64_t get_age(mreq_id_t id);
    uint64_t get_last_access(mreq_id_t id);
    uint64_t get_total_write_pending(mreq_id_t id);

private:
    typedef struct {
        bool valid;         /* entry is being used */
        bool ready;         /* data has come */
        bool doomed;        /* will be evicted (write_back issued) */
        bool on_the_fly;    /* data is coming (a request sent) */
        uint64_t tag;
        uint32_t *data;
        uint64_t last_access;
        bool dirty;
        uint64_t timestamp;
        uint64_t birthday;
        uint64_t total_write_pending;
    } cache_line_t;

private:
    cache_line_t* cache_line(maddr_t addr);

private:
    map<uint64_t, cache_line_t*>  m_cache;

};

#endif
