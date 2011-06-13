// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __CAT_HPP__
#define __CAT_HPP__

#include <boost/shared_ptr.hpp>
#include <vector>
#include <map>

#include "memory_types.hpp"

using namespace std;
using namespace boost;

typedef enum {
    CAT_STRIPE = 0,
    CAT_STATIC = 1,
    CAT_FIRST_TOUCH = 2
} catType_t;

typedef enum {
    CAT_REQ_NEW = 0,
    CAT_REQ_WAIT,
    CAT_REQ_DONE 
} catReqStatus_t;

class catRequest {
public:
    catRequest(maddr_t maddr, uint32_t sender);
    ~catRequest();

    inline catReqStatus_t status() { return m_status; }
    inline uint32_t home() { return m_home; }
    inline maddr_t maddr() { return m_maddr; }

    /* cost breakdown study */
    inline void set_milestone_time(uint64_t time) { m_milestone_time = time; }
    inline uint64_t milestone_time() { return m_milestone_time; }

    friend class cat;

private:
    catReqStatus_t m_status;
    maddr_t m_maddr;
    uint32_t m_sender;
    uint32_t m_home;

    /* cost breakdown study */
    uint64_t m_milestone_time;

};

/* simple performance model : unlimited read ports */

class cat {
public:
    cat(uint32_t num_nodes, const uint64_t& system_time, 
        uint32_t num_ports, uint32_t latency, uint32_t allocation_unit_in_bytes);
    virtual ~cat();

    virtual void request(shared_ptr<catRequest> req) = 0;
    virtual void tick_positive_edge() = 0;
    virtual void tick_negative_edge() = 0;

    inline bool available() { return (m_num_ports == 0 || m_num_free_ports > 0); }

protected:
    inline void set_req_status(shared_ptr<catRequest> req, catReqStatus_t status) { req->m_status = status; }
    inline void set_req_home(shared_ptr<catRequest> req, uint32_t home) { req->m_home = home; }
    inline maddr_t req_maddr(shared_ptr<catRequest> req) { return req->m_maddr; }
    inline uint32_t req_sender(shared_ptr<catRequest> req) { return req->m_sender; }

    inline maddr_t get_start_maddr_in_unit(maddr_t maddr) { 
        maddr.address -= maddr.address % m_allocation_unit_in_bytes; 
        return maddr; 
    }

    uint32_t m_num_nodes;
    const uint64_t &system_time;
    uint32_t m_latency;
    uint32_t m_allocation_unit_in_bytes;
    uint32_t m_num_ports;
    uint32_t m_num_free_ports;

};

class catStripe : public cat {
public:
    catStripe(uint32_t num_nodes, const uint64_t& system_time, uint32_t num_ports, 
              uint32_t latency, uint32_t allocation_unit_in_bytes);
    virtual ~catStripe();

    virtual void request(shared_ptr<catRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

protected:
    typedef struct {
        shared_ptr<catRequest> request;
        uint64_t available_time;
    } entry_t;

    vector<entry_t> m_entry_queue;
};

class catStatic : public cat {
public:
    catStatic(uint32_t num_nodes, const uint64_t& system_time, uint32_t num_ports, 
              uint32_t latency, uint32_t allocation_unit_in_bytes, uint32_t synch_delay);
    virtual ~catStatic();

    virtual void request(shared_ptr<catRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

    void set(maddr_t maddr, uint32_t home, bool delay_to_synch = true);

protected:
    typedef struct {
        shared_ptr<catRequest> request;
        uint64_t available_time;
    } request_entry_t;

    typedef struct {
        uint32_t home;
        uint64_t available_time;
    } map_entry_t;


    uint32_t m_synch_delay;
    map<maddr_t, map_entry_t> m_map;
    //map<uint32_t/*space*/, map<uint64_t/*index*/, map_entry_t> > m_map;
    vector<request_entry_t> m_req_entry_queue;
};

class catFirstTouch : public cat {
public:
    catFirstTouch(uint32_t num_nodes, const uint64_t& system_time, uint32_t num_port, 
                  uint32_t latency, uint32_t allocation_unit_in_bytes, uint32_t synch_delay);
    virtual ~catFirstTouch();

    virtual void request(shared_ptr<catRequest> req);
    virtual void tick_positive_edge();
    virtual void tick_negative_edge();

protected:
    typedef struct {
        shared_ptr<catRequest> request;
        uint64_t available_time;
    } request_entry_t;

    typedef struct {
        uint32_t home;
        uint64_t available_time;
    } map_entry_t;

    uint32_t m_synch_delay;
    map<maddr_t, map_entry_t> m_map;
    vector<request_entry_t> m_req_entry_queue;
};

#endif
