// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __DRAM_CONTROLLER_HPP__
#define __DRAM_CONTROLLER_HPP__

#include "memory.hpp"
#include "logger.hpp"
#include "random.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/cstdint.hpp>

using namespace boost;

#define DRAM_BLOCK_SIZE 256 /* in bytes, must be a multiple of 4 */

class dram {
public:
    dram();
    ~dram();

    friend class dramController;

private:
    map<uint64_t, uint32_t* > space;
    mutable recursive_mutex dram_mutex;;
};

class dramController : public memory {
public:
    typedef struct {
        bool use_lock;
        uint32_t off_chip_latency;
        uint32_t bytes_per_cycle;
        uint32_t dram_process_time;
        uint32_t dc_process_time;
        uint32_t header_size_bytes;
    } dramController_cfg_t;

    dramController(const uint32_t numeric_id, const uint32_t level, const uint64_t &system_time,
          logger &log, shared_ptr<random_gen> ran,
          shared_ptr<dram> dram,
          dramController_cfg_t cfgs);
    virtual ~dramController();

    virtual mreq_id_t request(shared_ptr<memoryRequest> req, uint32_t location, uint32_t target_level);
    virtual bool ready(mreq_id_t id);
    virtual shared_ptr<memoryRequest> get_req(mreq_id_t id);
    virtual bool finish(mreq_id_t id);

    virtual void initiate();
    virtual void update();
    virtual void process();

private:
    typedef enum {
        REQ_INIT,
        REQ_DC_PROCESS,
        REQ_TO_DRAM,
        REQ_DRAM_PROCESS,
        REQ_FROM_DRAM,
        REQ_DONE
    } req_status_t;

    typedef struct {
        uint32_t byte_count;
        uint64_t time_to_arrive;
    } on_the_fly_t;

    typedef struct {
        req_status_t status;
        uint32_t remaining_process_time;
        shared_ptr<memoryRequest> req;
        uint32_t bytes_to_send;
        vector<on_the_fly_t> packets;
    } in_req_entry_t;
    
private:
    void mem_access(shared_ptr<memoryRequest> req);
    void mem_access_safe(shared_ptr<memoryRequest> req);

private:
    shared_ptr<dram> m_dram;
    dramController_cfg_t m_cfgs;
    map<mreq_id_t, in_req_entry_t> m_in_req_table;

    uint32_t m_channel_width;
    uint32_t m_to_dram_in_transit;
    uint32_t m_from_dram_in_transit;
};

#endif
