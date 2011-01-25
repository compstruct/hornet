// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __REMOTE_MEMORY_HPP__
#define __REMOTE_MEMORY_HPP__

#include "memory.hpp"
#include "message.hpp"
#include "coreMessageQueue.hpp"
#include <boost/shared_ptr.hpp>

class remoteMemory: public memory {
public:
    typedef struct {
        uint32_t process_time;
    } remoteMemory_cfg_t;

    remoteMemory(const uint32_t numeric_id, const uint32_t level, const uint64_t &system_time, 
                 logger &log, shared_ptr<random_gen> ran,
                 remoteMemory_cfg_t cfgs);
    ~remoteMemory();

    /* requests by cores */
    virtual mreq_id_t request(shared_ptr<memoryRequest> req);
    virtual mreq_id_t ra_request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t level = 1);

    /* requests by cache */
    virtual mreq_id_t request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t level);

    virtual bool ready(mreq_id_t id);
    virtual bool finish(mreq_id_t id);

    virtual void initiate();
    virtual void update();
    virtual void process();

    void set_remote_home(int location, uint32_t level);
    inline void set_bytes_per_flit(uint32_t bytes) { m_bytes_per_flit = bytes; }
    inline void set_flits_per_header(uint32_t flits) { m_flits_per_header = flits; }
    inline void set_out_queue(msg_type_t type, shared_ptr<coreMessageQueue> queue) { m_out_queues[type] = queue; }
    inline void set_in_queue(msg_type_t type, shared_ptr<coreMessageQueue> queue) { m_in_queues[type] = queue; }

private:
    virtual mreq_id_t _request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t level, bool ra);

private:
    typedef enum {
        REQ_INIT,
        REQ_BUSY,
        REQ_PROCESSED,
        REQ_WAIT,
        REQ_DONE
    } req_status_t;

    typedef struct {
        req_status_t status;
        uint32_t remaining_process_time;
        int location;
        uint32_t level;
        shared_ptr<memoryRequest> req;
        bool ra;
    } in_req_entry_t;

    mreq_id_t take_new_remote_mreq_id();
    void return_remote_mreq_id(mreq_id_t old_id);

    remoteMemory_cfg_t m_cfgs;
    int m_default_home;
    uint32_t m_default_level;
    map<msg_type_t, shared_ptr<coreMessageQueue> > m_out_queues;
    map<msg_type_t, shared_ptr<coreMessageQueue> > m_in_queues;

    map<mreq_id_t, in_req_entry_t> m_in_req_table;
    map<mreq_id_t, mreq_id_t> m_remote_req_table;

    uint32_t m_max_remote_mreq_id;
    vector<mreq_id_t> m_remote_mreq_id_pool;

    uint32_t m_bytes_per_flit;
    uint32_t m_flits_per_header;
};

#endif
