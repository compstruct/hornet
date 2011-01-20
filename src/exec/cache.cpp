// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cache.hpp"

cache::cache(const uint32_t id, const uint64_t &t, 
             logger &log, shared_ptr<random_gen> ran,
             cache_cfg_t cfgs)
: memory(id, t, log, ran), m_cfgs(cfgs), m_home(shared_ptr<cache>()) {
}

cache::~cache() {}

void cache::set_home(shared_ptr<memory> home) {
    m_home = home;
}

mreq_id_t cache::request(shared_ptr<memoryRequest> req) {
    mreq_id_t new_id = take_new_mreq_id();
    req_entry_t new_entry;
    new_entry.status = REQ_INIT;
    new_entry.remaining_process_time = m_cfgs.process_time;
    new_entry.remaining_process_time = ran->random_range(3)+1;

    new_entry.home_req_id = MAX_INVALID_MREQ_ID;
    new_entry.req = req;
    m_table[new_id] = new_entry;
    //cerr << "[cache " << m_id << "] get request " << new_id << " @ " << system_time << endl;
    return new_id;
}

bool cache::ready(mreq_id_t id) {
    return (m_table.count(id) > 0 && m_table[id].status == REQ_DONE);
}

bool cache::finish(mreq_id_t id) {
    if (m_table.count(id) == 0) {
        return true;
    } else if (m_table[id].status != REQ_DONE) {
        return false;
    } else {
        m_table.erase(id);
        return_mreq_id(id);
    }
    return true;
}

void cache::initiate() {
    for (map<mreq_id_t, req_entry_t>::iterator i = m_table.begin(); i != m_table.end(); ++i) {
        if (i->second.status == REQ_INIT) {
            //cerr << "[cache " << m_id << "] request start " << i->first << " @ " << system_time << endl;
            i->second.status = REQ_BUSY;
        }
    }
}

void cache::update() {
    for (map<mreq_id_t, req_entry_t>::iterator i = m_table.begin(); i != m_table.end(); ++i) {
        if (i->second.status == REQ_WAIT) {
            if (m_home->ready(i->second.home_req_id)) {
                i->second.status = REQ_DONE;
                m_home->finish(i->second.home_req_id);
            }
        }
    }
}

void cache::process() {
    for (map<mreq_id_t, req_entry_t>::iterator i = m_table.begin(); i != m_table.end(); ++i) {
        if (i->second.status == REQ_BUSY) {
            assert(i->second.remaining_process_time > 0);
            --(i->second.remaining_process_time);
            //cerr << "[cache " << m_id << "] request " << i->first << " remaining: " << i->second.remaining_process_time << " @ " << system_time << endl;
            if (i->second.remaining_process_time == 0) {
                // dummy magic memory 
                //cerr << "[cache " << m_id << "] request done " << i->first << " @ " << system_time << endl;
                i->second.status = REQ_DONE;
            }
        }
    }
}


