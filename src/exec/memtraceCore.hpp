// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMTRACE_CORE_HPP__
#define __MEMTRACE_CORE_HPP__

/* TODO (Phase 2) : port the em2core (working on the memory trace) */
/*                : add ra support */

#include <boost/shared_ptr.hpp>
#include "id_factory.hpp"
#include "logger.hpp"
#include "statistics.hpp"
#include "random.hpp"
#include "core.hpp"
#include "memory.hpp"
#include "memtraceThread.hpp"

class memtraceThread;
class memtraceThreadPool;

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
                 /* TODO progress marker */
                 memtraceCore_cfg_t cfgs) throw(err);
    virtual ~memtraceCore() throw();

    /* set memory */

    virtual void tick_positive_edge() throw(err);
    virtual void tick_negative_edge() throw(err);
    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_drained() const throw();

private:
    map<uint32_t, flow_id> flow_ids;

    /* Configurations */
    memtraceCore_cfg_t m_cfgs;

    /* Execution lanes */
    typedef struct {
        bool empty;
        bool evictable;
        bool busy; 
        memtraceThread* thread;
        uint32_t remaining_alu_time;
        mem_req_id_t mem_req_id;
    } lane_entry_t;
    typedef vector<lane_entry_t>::size_type lane_idx_t;
    vector<lane_entry_t> m_lanes;
    uint32_t m_num_threads;
    uint32_t m_num_native_threads;
    uint32_t m_num_guest_threads;

    /* Thread pool */
    shared_ptr<memtraceThreadPool> m_threads;

    /* Memory message queue */

    /* Running state */

    /* Local methods */
    void release_xmit_buffer();
};

/* TODO (Phase 4) : design memtraceCore stats */

#endif
