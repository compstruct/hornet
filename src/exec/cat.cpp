// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "cat.hpp"
#include <iostream>

catRequest::catRequest(maddr_t maddr, uint32_t sender) : m_status(CAT_REQ_NEW), m_maddr(maddr), m_sender(sender) {}

catRequest::~catRequest() {}

SynchedCATModel::SynchedCATModel() : m_semaphore(0) {}

SynchedCATModel::~SynchedCATModel() {}

bool SynchedCATModel::lock() {
    unique_lock<recursive_mutex> l(m_mutex);
    if (m_semaphore == 0) {
        ++m_semaphore;
        return true;
    } else {
        return false;
    }
}

void SynchedCATModel::unlock() {
    unique_lock<recursive_mutex> l(m_mutex);
    --m_semaphore;
}

void SynchedCATModel::set(maddr_t tag, uint64_t available_time, uint32_t core) {
    map_entry_t new_entry = {core, available_time};
    m_map[tag] = new_entry;
}

bool SynchedCATModel::has_tag(maddr_t tag) {
    return m_map.count(tag) > 0;
}

uint32_t SynchedCATModel::core_for_tag(maddr_t tag) {
    assert(m_map.count(tag));
    return m_map[tag].home;
}

uint64_t SynchedCATModel::available_time_for_tag(maddr_t tag) {
    assert(m_map.count(tag));
    return m_map[tag].available_time;
}


cat::cat(uint32_t num_nodes, const uint64_t &t, uint32_t num_ports, uint32_t latency, uint32_t allocation_unit_in_bytes) : 
    m_num_nodes(num_nodes), system_time(t), m_latency(latency), m_allocation_unit_in_bytes(allocation_unit_in_bytes),
    m_num_ports(num_ports), m_num_free_ports(num_ports)
{
    assert(m_allocation_unit_in_bytes > 0);

}

cat::~cat() {}

/*****************/
/* catStripe     */
/*****************/

catStripe::catStripe(uint32_t num_nodes, const uint64_t& t, uint32_t num_ports, uint32_t latency, uint32_t allocation_unit_in_bytes) :
    cat(num_nodes, t, num_ports, latency, allocation_unit_in_bytes) {}

catStripe::~catStripe() {}

void catStripe::request(shared_ptr<catRequest> req) {
    assert(available());
    if (m_num_ports > 0) {
        --m_num_free_ports;
    }
    entry_t new_entry = { req, system_time + m_latency };
    m_entry_queue.push_back(new_entry);
    set_req_status(req, CAT_REQ_WAIT);
}

void catStripe::tick_positive_edge() {}

void catStripe::tick_negative_edge() {
    while (!m_entry_queue.empty() && m_entry_queue.front().available_time <= system_time) {
        shared_ptr<catRequest> req = m_entry_queue.front().request;
        set_req_home(req, (req_maddr(req).address/m_allocation_unit_in_bytes)%m_num_nodes);
        set_req_status(req, CAT_REQ_DONE);
        if (m_num_ports > 0) {
            ++m_num_free_ports;
        }
        m_entry_queue.erase(m_entry_queue.begin());
    }
}
    
/*****************/
/* catStatic     */
/*****************/

catStatic::catStatic(uint32_t num_nodes, const uint64_t& t, uint32_t num_ports, uint32_t latency, uint32_t allocation_unit_in_bytes, 
                     uint32_t synch_delay, shared_ptr<SynchedCATModel> model) : 
    cat(num_nodes, t, num_ports, latency, allocation_unit_in_bytes), m_model(model), m_synch_delay(synch_delay) {}

catStatic::~catStatic() {}

void catStatic::set(maddr_t maddr, uint32_t home, bool delay_to_synch) {
    maddr_t start_maddr = get_start_maddr_in_unit(maddr);
    while(!m_model->lock()) {}
    m_model->set(start_maddr, (delay_to_synch)? system_time + m_synch_delay : 0, home);
    m_model->unlock();
}

void catStatic::request(shared_ptr<catRequest> req) {
    assert(available());
    if (m_num_ports > 0) {
        --m_num_free_ports;
    }

    request_entry_t new_req_entry = { req, 0 };
    maddr_t start_maddr = get_start_maddr_in_unit(req_maddr(req));

    while(!m_model->lock()) {}

    if (m_model->has_tag(start_maddr)) {
        set_req_home(req, m_model->core_for_tag(start_maddr));
        if (m_model->available_time_for_tag(start_maddr) > system_time) {
            new_req_entry.available_time = m_model->available_time_for_tag(start_maddr) + m_latency;
        } else {
            new_req_entry.available_time = system_time + m_latency;
        }
        m_model->unlock();
    } else {
        /* set core 0 for any uninitialized address */
        m_model->set(start_maddr, system_time + m_synch_delay, 0);
        m_model->unlock();
        new_req_entry.available_time = system_time + m_synch_delay + m_latency;
        set_req_home(req, 0);
    }


    set_req_status(req, CAT_REQ_WAIT);
    m_req_entry_queue.push_back(new_req_entry);

}

void catStatic::tick_positive_edge() {}

void catStatic::tick_negative_edge() {
    vector<request_entry_t>::iterator it;
    for (it = m_req_entry_queue.begin(); it != m_req_entry_queue.end(); ++it) {
        if (it->available_time <= system_time) {
            set_req_status(it->request, CAT_REQ_DONE);
        }
    }
    while (!m_req_entry_queue.empty() && m_req_entry_queue.front().available_time <= system_time) {
        m_req_entry_queue.erase(m_req_entry_queue.begin());
        if (m_num_ports > 0) {
            ++m_num_free_ports;
        }
    }
}
    
/*****************/
/* catFirstTouch */
/*****************/

catFirstTouch::catFirstTouch(uint32_t num_nodes, const uint64_t& t, uint32_t num_ports, uint32_t latency, 
                             uint32_t allocation_unit_in_bytes, uint32_t synch_delay, shared_ptr<SynchedCATModel> model) : 
    cat(num_nodes, t, num_ports, latency, allocation_unit_in_bytes), m_model(model), m_synch_delay(synch_delay) {}

catFirstTouch::~catFirstTouch() {}

void catFirstTouch::request(shared_ptr<catRequest> req) {
    assert(available());
    if (m_num_ports > 0) {
        --m_num_free_ports;
    }

    maddr_t start_maddr = get_start_maddr_in_unit(req_maddr(req));
    request_entry_t new_req_entry = { req, 0 };
    
    while(!m_model->lock()) {}

    if (!m_model->has_tag(start_maddr)) {
        /* this is the first touch */
        m_model->set(start_maddr, system_time + m_synch_delay, req_sender(req));
        m_model->unlock();
        new_req_entry.available_time = system_time + m_synch_delay + m_latency;
        set_req_home(req, req_sender(req));
    } else {
        set_req_home(req, m_model->core_for_tag(start_maddr));
        if (m_model->available_time_for_tag(start_maddr) <= system_time) {
            new_req_entry.available_time = system_time + m_latency;
        } else {
            new_req_entry.available_time = m_model->available_time_for_tag(start_maddr) + m_latency;
        }
        m_model->unlock();
    }
    set_req_status(req, CAT_REQ_WAIT);
    m_req_entry_queue.push_back(new_req_entry);
}

void catFirstTouch::tick_positive_edge() {}

void catFirstTouch::tick_negative_edge() {
    vector<request_entry_t>::iterator it;
    for (it = m_req_entry_queue.begin(); it != m_req_entry_queue.end(); ++it) {
        if (it->available_time <= system_time) {
            set_req_status(it->request, CAT_REQ_DONE);
        }
    }
    while (!m_req_entry_queue.empty() && m_req_entry_queue.front().available_time <= system_time) {
        m_req_entry_queue.erase(m_req_entry_queue.begin());
        if (m_num_ports > 0) {
            ++m_num_free_ports;
        }
    }
}
    
