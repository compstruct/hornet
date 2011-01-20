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
        uint32_t block_size;
        uint32_t total_block;
        uint32_t process_time;
        cache_policy_t policy;
    } cache_cfg_t;

    cache(const uint32_t numeric_id, const uint64_t &system_time,
          logger &log, shared_ptr<random_gen> ran,
          cache_cfg_t cfgs);
    virtual ~cache();

    virtual mreq_id_t request(shared_ptr<memoryRequest> req);
    virtual bool ready(mreq_id_t id);
    virtual bool finish(mreq_id_t id);

    virtual void initiate();
    virtual void update();
    virtual void process();

    void set_home(shared_ptr<memory> home);
    void set_home(uint32_t location, uint32_t level);

private:
    typedef enum {
        REQ_INIT,
        REQ_BUSY,
        REQ_WAIT,
        REQ_DONE
    } req_status_t;

    typedef struct {
        req_status_t status;
        uint32_t     remaining_process_time;
        mreq_id_t    home_req_id;
        shared_ptr<memoryRequest> req;
    } req_entry_t;

    cache_cfg_t m_cfgs;
    shared_ptr<memory> m_home;

    map<mreq_id_t, req_entry_t> m_table;
};

#endif
