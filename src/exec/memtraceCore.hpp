// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMTRACE_CORE_HPP__
#define __MEMTRACE_CORE_HPP__

#include <boost/shared_ptr.hpp>
#include "id_factory.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "random.hpp"
#include "core.hpp"
#include "memory.hpp"
#include "memtraceThread.hpp"
#include "memtraceThreadPool.hpp"

class memtraceCore : public core {
public:
    typedef enum { EM_NONE, EM_ENC }  em_type_t; /* TODO (Later) : more em schemes (swapInf, seapHS) */
    typedef enum { RA_NONE, RA_ONLY } ra_type_t; /* TODO (Later) : EM/RA hybrid */
    typedef struct {
        uint32_t    max_threads;
        uint32_t    flits_per_mig;
        uint32_t    flits_per_ra_with_data;      /* read reply, write request */
        uint32_t    flits_per_ra_without_data;   /* read request, write reply */
        em_type_t   em_type;
        ra_type_t   ra_type;
    } memtraceCore_cfg_t;

public:
    memtraceCore(const pe_id &id, const uint64_t &system_time,
                 shared_ptr<id_factory<packet_id> > packet_id_factory,
                 shared_ptr<tile_statistics> stats, logger &log,
                 shared_ptr<random_gen> ran,
                 shared_ptr<memtraceThreadPool> pool,
                 /* TODO (Later) progress marker */
                 memtraceCore_cfg_t cfgs) throw(err);
    virtual ~memtraceCore() throw();

    virtual void exec_core();

    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_drained() const throw();

    void spawn(memtraceThread* thread);
    void add_remote_memory(shared_ptr<memory> mem);
    void add_cache_chain(shared_ptr<memory> l1_cache);

private:
    map<uint32_t, flow_id> flow_ids;

    /* Configurations */
    memtraceCore_cfg_t m_cfgs;

    /* Execution lanes */
    typedef enum {
        LANE_EMPTY = 0,
        LANE_IDLE,
        LANE_BUSY,
        LANE_MIG,
        LANE_WAIT
    } lane_status_t;

    typedef struct {
        lane_status_t status;
        bool evictable;
        memtraceThread* thread;
        mreq_id_t mreq_id;
        shared_ptr<memoryRequest> req;
        shared_ptr<memory> mem_to_serve;
    } lane_entry_t;

    typedef vector<lane_entry_t>::size_type lane_idx_t;

private:
    /* Local methods */
    void load_thread(memtraceThread* thread);
    void unload_thread(lane_idx_t idx);

private:
    vector<lane_entry_t> m_lanes;
    lane_idx_t m_lane_ptr;
    uint32_t m_num_threads;
    uint32_t m_num_natives;
    uint32_t m_num_guests;

    /* Thread pool */
    shared_ptr<memtraceThreadPool> m_threads;

    /* memories */
    shared_ptr<memory> m_remote_memory;
    shared_ptr<memory> m_local_l1;

    /* Native contexts */
    set<mth_id_t> m_native_list;

    /* Memory message queue */

    /* Running state */

};

/* TODO (Phase 4) : design memtraceCore stats */

#endif
