// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "remoteMemory.hpp"

remoteMemory::remoteMemory(const uint32_t numeric_id, const uint64_t &system_time, 
        logger &log, shared_ptr<random_gen> ran, remoteMemory_cfg_t cfgs)
: memory(numeric_id, system_time, log, ran), m_cfgs(cfgs), m_max_remote_mreq_id(0), 
    m_bytes_per_flit(8), m_flits_per_header(1)
{
}

remoteMemory::~remoteMemory() {}

void remoteMemory::set_home(int location, uint32_t level) {
    m_default_home = location;
    m_default_level = level;
}

shared_ptr<memory> remoteMemory::next_memory() {
    return shared_ptr<memory>();
}

mreq_id_t remoteMemory::request(shared_ptr<memoryRequest> req) {
    return request(req, m_default_home, m_default_level, false);
}

mreq_id_t remoteMemory::request(shared_ptr<memoryRequest> req, int location, uint32_t level, bool ra) {
    /* put an entry */
    /* assumes an infinite request table - if it's finite, deadlock must be considered */
    mreq_id_t new_id = take_new_mreq_id();
    in_req_entry_t new_entry;
    new_entry.status = REQ_INIT;
    new_entry.remaining_process_time = m_cfgs.process_time;
    new_entry.req = req;
    new_entry.location = location;
    new_entry.level = level;
    new_entry.ra = ra;
    m_in_req_table[new_id] = new_entry;

    return new_id;
}

mreq_id_t remoteMemory::take_new_remote_mreq_id() {
    if (m_remote_mreq_id_pool.empty()) {
        return ++m_max_remote_mreq_id;
    } else {
        mreq_id_t ret = m_remote_mreq_id_pool.front();
        m_remote_mreq_id_pool.erase(m_remote_mreq_id_pool.begin());
        return ret;
    }
}

void remoteMemory::return_remote_mreq_id(mreq_id_t old_id) {
    m_remote_mreq_id_pool.push_back(old_id);
}

bool remoteMemory::ready(mreq_id_t id) {
    return (m_in_req_table.count(id) > 0 && m_in_req_table[id].status == REQ_DONE);
}

bool remoteMemory::finish(mreq_id_t id) {
    if (m_in_req_table.count(id) == 0) {
        return true;
    } else if (m_in_req_table[id].status != REQ_DONE) {
        return false;
    } else {
        m_in_req_table.erase(id);
        return_mreq_id(id);
    }
    return true;
}

void remoteMemory::initiate() {
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
        if (i->second.status == REQ_INIT) {
            i->second.status = REQ_BUSY;
        }
    }
}

void remoteMemory::update() {
    /* iterate on ra replies and memory replies */
    msg_type_t mem_reps[] = {MSG_RA_REP, MSG_MEM_REP};
    vector<msg_type_t> mem_rep_vec (mem_reps, mem_reps + sizeof(mem_reps)/sizeof(msg_type_t));
    for (vector<msg_type_t>::iterator i_queue = mem_rep_vec.begin(); i_queue != mem_rep_vec.end(); ++i_queue) {
        uint32_t num_msgs = m_in_queues[*i_queue]->size();
        for (uint32_t i_msg = 0; i_msg < num_msgs; ++i_msg) {
            mreq_id_t remote_req_id = m_in_queues[*i_queue]->front().mem_msg.req_id;
            mreq_id_t in_req_id = m_remote_req_table[remote_req_id];
            m_in_req_table[in_req_id].status = REQ_DONE;
            m_remote_req_table.erase(remote_req_id);
            m_in_queues[*i_queue]->pop();
        }
    }
}

void remoteMemory::process() {
    for (map<mreq_id_t, in_req_entry_t>::iterator i = m_in_req_table.begin(); i != m_in_req_table.end(); ++i) {
        /* logic processing */
        if (i->second.status == REQ_BUSY) {
            assert(i->second.remaining_process_time > 0);
            --(i->second.remaining_process_time);
            if (i->second.remaining_process_time == 0) {
                i->second.status = REQ_PROCESSED;
            }
        }
        /* actions */
        if (i->second.status == REQ_PROCESSED) {
            shared_ptr<memoryRequest> req = i->second.req;
            /* initiates network packet */
            msg_t new_msg;
            new_msg.type = (i->second.ra)? MSG_RA_REQ : MSG_MEM_REQ;
            new_msg.flit_count = m_flits_per_header;
            if ((i->second.req)->rw() == MEM_WRITE) {
                new_msg.flit_count += ((i->second.req)->byte_count() + m_bytes_per_flit - 1) / m_bytes_per_flit;
            }
            new_msg.dst = (i->second.location);
            new_msg.mem_msg.target_level = i->second.level;
            new_msg.mem_msg.req_id = take_new_remote_mreq_id();
            new_msg.mem_msg.req = (i->second.req);
            if (m_out_queues[new_msg.type]->push_back(new_msg)) {
                i->second.status = REQ_WAIT;
                m_remote_req_table[new_msg.mem_msg.req_id] = i->first;
                LOG(log,3) << "a memory request is put into a send queue to " << i->second.location << endl;
            } else {
                return_remote_mreq_id(new_msg.mem_msg.req_id);
            }
        }
    }
}
