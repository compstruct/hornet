// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __MEMTRACE_CORE_HPP__
#define __MEMTRACE_CORE_HPP__

#include "core.hpp"
#include "memtraceThread.hpp"

class memtraceCore : public core {
public:
    memtraceCore(const pe_id &id, const uint64_t &system_time,
                 shared_ptr<id_factory<packet_id> > packet_id_factory,
                 shared_ptr<tile_statistics> stats, logger &log,
                 shared_ptr<random_gen> ran,
                 shared_ptr<memtraceThreadPool> pool,
                 shared_ptr<memory> mem,
                 bool support_migration,
                 uint32_t msg_queue_size,
                 uint32_t bytes_per_flit,
                 uint32_t flits_per_context,
                 uint32_t max_threads) throw(err);
    virtual ~memtraceCore() throw();

    virtual void execute();
    virtual void commit_memory_requests();

    virtual uint64_t next_pkt_time() throw(err);
    virtual bool is_drained() const throw();

    void spawn(memtraceThread* thread);

private:
    map<uint32_t, flow_id> flow_ids;

    /* Configurations */
    uint32_t m_flits_per_context;
    uint32_t m_max_threads;

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
        shared_ptr<memoryRequest> req;
        uint64_t last_memory_issued;
    } lane_entry_t;

    typedef vector<lane_entry_t>::size_type lane_idx_t;

    typedef struct {
        bool valid;
        lane_idx_t idx;
        int dst; /* for logs */
    } pending_mig_t;

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

    /* Native contexts */
    set<mth_id_t> m_native_list;

    /* Running state */
    bool m_do_evict;
    pending_mig_t m_pending_mig;

};

#endif

