// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "core.hpp"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include "homeCache.hpp"

core::core(const pe_id &id, const uint64_t &t,
        shared_ptr<id_factory<packet_id> > pif,
        shared_ptr<tile_statistics> st, logger &l,
        shared_ptr<random_gen> r, core_cfg_t cfgs
        ) throw(err) : pe(id), system_time(t), stats(st), log(l), ran(r), m_cfgs(cfgs), m_packet_id_factory(pif), 
    m_away_cache(shared_ptr<memory>()), m_max_memory_level(0), m_num_active_remote_mem_requests(0), m_current_receive_channel() 
{ 
    for (int i = 0; i < NUM_MSG_TYPES; ++i) {
        m_in_msg_queues[(msg_type_t)i] = shared_ptr<coreMessageQueue> (new coreMessageQueue((msg_type_t)i, m_cfgs.msg_queue_size));
        m_out_msg_queues[(msg_type_t)i] = shared_ptr<coreMessageQueue> (new coreMessageQueue((msg_type_t)i, m_cfgs.msg_queue_size));
    }
}

core::~core() throw() { }

void core::connect(shared_ptr<bridge> net_bridge) throw(err) {
    m_net = net_bridge;
    shared_ptr<vector<uint32_t> > qs = m_net->get_ingress_queue_ids();
    m_queue_ids.clear();
    copy(qs->begin(), qs->end(), back_insert_iterator<vector<uint32_t> >(m_queue_ids));
}

void core::add_away_cache(shared_ptr<memory> cache) {
    m_away_cache = cache;    
}
void core::add_remote_memory(shared_ptr<remoteMemory> mem) {
    m_remote_memory = mem;

    /* give memory message queues so it can send/receive messages */
    msg_type_t mem_msgs[] = {MSG_RA_REQ, MSG_RA_REP, MSG_MEM_REQ, MSG_MEM_REP};
    vector<msg_type_t> mem_msg_vec (mem_msgs, mem_msgs + sizeof(mem_msgs) / sizeof(msg_type_t));
    for (vector<msg_type_t>::iterator i = mem_msg_vec.begin(); i != mem_msg_vec.end(); ++i) {
        mem->set_out_queue(*i, m_out_msg_queues[*i]);
        mem->set_in_queue(*i, m_in_msg_queues[*i]);
        mem->set_bytes_per_flit(m_cfgs.bytes_per_flit);
        mem->set_flits_per_header(m_cfgs.flits_per_mem_msg_header);
    }
}

void core::add_to_memory_hierarchy(int level, shared_ptr<memory> mem) {
    /* assume only one meory per level */
    assert(m_memory_hierarchy.count(level) == 0);
    if (m_memory_hierarchy.size() == 0 || level < m_min_memory_level) {
        m_min_memory_level = level;
    }
    if (level > m_max_memory_level) {
        m_max_memory_level = level;
    }
    m_memory_hierarchy[level] = mem;
}

shared_ptr<coreMessageQueue> core::core_receive_queue(int channel) {
    assert(channel < NUM_MSG_CORE);
    return m_in_msg_queues[(msg_type_t)((int)MSG_CORE_0 + channel)];
}

shared_ptr<coreMessageQueue> core::core_send_queue(int channel) {
    assert(channel < NUM_MSG_CORE);
    return m_out_msg_queues[(msg_type_t)((int)MSG_CORE_0 + channel)];
}

void core::release_xmit_buffer() {
    /* release xmit buffer for injection if transmission is done */
    for (map<uint32_t, uint64_t*>::iterator i = m_xmit_buffer.begin(); i != m_xmit_buffer.end(); ++i) {
        if (m_net->get_transmission_done(i->first)) {
            delete[] i->second;
            m_xmit_buffer.erase(i->first);
            break;
        }
    }
}

void core::tick_positive_edge() throw(err) {

    release_xmit_buffer();

    /* set priority of sending/receiving messages */
    msg_type_t priority[] = {MSG_CORE_0, MSG_RA_REP, MSG_MEM_REP, MSG_CORE_1, MSG_RA_REQ, MSG_MEM_REQ};
    vector<msg_type_t> priority_vector (priority, priority + sizeof(priority) / sizeof(msg_type_t));

    /* Send message(s) in out_msg_queues */
    for (vector<msg_type_t>::iterator i_msg_type = priority_vector.begin(); 
            i_msg_type != priority_vector.end(); ++i_msg_type) 
    {
        if (m_out_msg_queues[*i_msg_type]->size() == 0) {
            continue;
        }
        /* allocate new memory for msg (alive until it gets the destination) */
        msg_t* msg = new msg_t;
        /* for now assume 1 message per queue per cycle */
        *msg = m_out_msg_queues[*i_msg_type]->front();
        uint64_t *p_env = new uint64_t[msg->flit_count];
        p_env[0] = (uint64_t)(msg);
        packet_id pid = m_packet_id_factory->get_fresh_id();
        msg->src = get_id().get_numeric_id();
        uint32_t fid = (((((*i_msg_type) << 8) | msg->src ) << 8 ) | msg->dst ) << 8;
        //printf("Forming flow ~ src: %d, dst: %d\n", msg->src, msg->dst);
        uint32_t xid = m_net->send(fid, (void*)p_env, msg->flit_count, pid, stats->is_started()); 
        if (xid!=0) {
            /* packet is said to be 'offered' when it begins to move... */ 
            /* this is somewhat different from previous cases, where a packet is offered when it gets in the queue */
            stats->offer_packet(fid, pid, msg->flit_count); 
            /* save in xmit buffer until the transfer finished */
            m_xmit_buffer[xid] = p_env;
            LOG(log,4) << "[core " << get_id().get_numeric_id() << " @ " << system_time 
                       << " ] has sent a message type " << (uint32_t) *i_msg_type << endl;
            m_out_msg_queues[*i_msg_type]->pop();
        } else {
            LOG(log,4) << "[core " << get_id().get_numeric_id() << " @ " << system_time 
                       << " ] has failed sending a message type " << (uint32_t) *i_msg_type << endl;
            delete[] p_env;
        }
    }

    /* accept all new requests in memory components (they will begin working in this cycle) */
    m_remote_memory->initiate();
    mh_initiate();
    if (away_cache()) {
        away_cache()->initiate();
    }
    
    /* execute core / memory server. */
    /* they will check if the requests they issued are done */
    /* they may also make new requests (which will be accepted in the next cycle, and will not begin working in this cycle) */
    
    exec_core();
    exec_mem_server();

    /* check if requests issued by middle-level memory components are done */
    m_remote_memory->update();
    mh_update();
    if (away_cache()) {
        away_cache()->update();
    }    

    /* serve accepted memory requests (the results will be updated to parents in the next cycle) */ 
    m_remote_memory->process();
    mh_process();
    if (away_cache()) {
        away_cache()->process();
    }    

    /* receive message(s) and put into correpsonding in_msg_queues */
    m_current_receive_channel++; /* round-robin */
    for (uint32_t _i = 0; _i < 32; ++_i) {
        uint32_t i = (_i + m_current_receive_channel)%32; /* round-robin */
        if (m_incoming_packets.find(i) != m_incoming_packets.end()) {
            core_incoming_packet_t &ip = m_incoming_packets[i];
            if (m_net->get_transmission_done(ip.xmit)) {
                msg_t* msg = (msg_t*)((uint64_t)ip.payload[0]);
                if (m_in_msg_queues[msg->type]->push_back(*msg)) {
                    LOG(log,3) << "[core " << get_id().get_numeric_id() << " @ " << system_time
                               << " ] has received a message type " << (uint32_t) msg->type 
                               << " waiting queue size "
                               << m_in_msg_queues[msg->type]->size() << endl;
                    m_incoming_packets.erase(i);
                    delete msg;
                }
            }
        }
    }
    uint32_t waiting = m_net->get_waiting_queues();
    boost::function<int(int)> rr_fn = bind(&random_gen::random_range, ran, _1);
    if (waiting != 0) {
        random_shuffle(m_queue_ids.begin(), m_queue_ids.end(), rr_fn);
        for (vector<uint32_t>::iterator i = m_queue_ids.begin(); i != m_queue_ids.end(); ++i) {
            if (((waiting >> *i) & 1) && (m_incoming_packets.find(*i) == m_incoming_packets.end())) {
                core_incoming_packet_t &ip = m_incoming_packets[*i];
                ip.flow = m_net->get_queue_flow_id(*i);
                ip.len = m_net->get_queue_length(*i);
                ip.xmit = m_net->receive(&(ip.payload), *i, m_net->get_queue_length(*i), &ip.id);
            }
        }
    }
}

void core::tick_negative_edge() throw(err) {
}

void core::exec_mem_server() {

    /* initiate received messages */
    for (map<int, shared_ptr<map<mreq_id_t, server_in_req_entry_t> > >::iterator i_sender 
            = m_server_in_req_table.begin();
            i_sender != m_server_in_req_table.end(); ++i_sender) {
        for (map<mreq_id_t, server_in_req_entry_t>::iterator i_req = i_sender->second->begin(); 
                i_req != i_sender->second->end(); ++i_req) {
            if (i_req->second.status == REQ_INIT) {
                i_req->second.status = REQ_BUSY;
                i_req->second.remaining_process_time = m_cfgs.memory_server_process_time;
            }
        }
    }

    /* update from local memories */
    for (map<int, shared_ptr<map<mreq_id_t, server_local_req_entry_t> > >::iterator i_level = m_server_local_req_table.begin();
            i_level != m_server_local_req_table.end(); ++i_level)
    {
        vector<mreq_id_t> server_local_req_obsolete;
        for (map<mreq_id_t, server_local_req_entry_t>::iterator i_req = i_level->second->begin(); 
                i_req != i_level->second->end(); ++i_req)
        {
            mreq_id_t local_req_id = i_req->first;
            server_local_req_entry_t &local_entry = i_req->second;
            if (local_entry.mem_to_serve->ready(local_req_id)) {
                server_in_req_entry_t &in_entry = (*m_server_in_req_table[local_entry.sender])[local_entry.in_req_id];
                if (m_cfgs.support_library && in_entry.req->is_ra()) {
                    shared_ptr<homeCache> home_cache = static_pointer_cast<homeCache, memory>(local_entry.mem_to_serve);

                    uint64_t __attribute__((unused)) timestamp = home_cache->get_timestamp(local_req_id);
                    uint64_t __attribute__((unused)) age = home_cache->get_age(local_req_id);
                    uint64_t __attribute__((unused)) last_access = home_cache->get_last_access(local_req_id);
                    uint64_t __attribute__((unused)) write_pending = home_cache->get_total_write_pending(local_req_id);

                    /* LIBRARY COMPETITION */
                    /* make a timestamp decision logic here */
                    /* some basic information is provided in the above, */
                    /* or you may modify codes to use additional information */
                    /* but, be careful about hardware implementation costs */
                    uint64_t new_timestamp = system_time + 10;

                    in_entry.req->set_timestamp(new_timestamp);

                    home_cache->update_timestamp(local_req_id, new_timestamp);
                }
                in_entry.status = REQ_DONE;
                local_entry.mem_to_serve->finish(local_req_id);
                server_local_req_obsolete.push_back(local_req_id);
            }
        }
        for (vector<mreq_id_t>::iterator i = server_local_req_obsolete.begin(); i != server_local_req_obsolete.end(); ++i)  {
            i_level->second->erase(*i);
        }
    }

    /* sends out replies */
    for (map<int, shared_ptr<map<mreq_id_t, server_in_req_entry_t> > >::iterator i_sender 
            = m_server_in_req_table.begin();
            i_sender != m_server_in_req_table.end(); ++i_sender) {
        vector<mreq_id_t> server_in_req_obsolete;
        for (map<mreq_id_t, server_in_req_entry_t>::iterator i_req = i_sender->second->begin(); 
                i_req != i_sender->second->end(); ++i_req) {
            mreq_id_t in_req_id = i_req->first;
            server_in_req_entry_t &in_entry = i_req->second;
            if (in_entry.status == REQ_DONE) {
                msg_t new_msg;
                new_msg.type = (in_entry.msg_type == MSG_RA_REQ)? MSG_RA_REP : MSG_MEM_REP;
                new_msg.flit_count = m_cfgs.flits_per_mem_msg_header;
                if (in_entry.req->rw() == MEM_READ) {
                    new_msg.flit_count += (in_entry.req->byte_count() + m_cfgs.bytes_per_flit - 1) 
                        / m_cfgs.bytes_per_flit;
                }
                new_msg.dst = in_entry.sender;
                new_msg.mem_msg.req_id = in_req_id;
                new_msg.mem_msg.req = in_entry.req;

                if (m_out_msg_queues[new_msg.type]->push_back(new_msg)) {
                    LOG(log,3) << "[server " << get_id().get_numeric_id() << " @ " << system_time 
                        << " ] has sent a memory reply to " << new_msg.dst << endl;
                    server_in_req_obsolete.push_back(in_req_id);
                    --m_num_active_remote_mem_requests;
                }
            }
        }
        for (vector<mreq_id_t>::iterator i = server_in_req_obsolete.begin(); i != server_in_req_obsolete.end(); ++i) {
            i_sender->second->erase(*i);
        }
    }

    /* process */
    for (map<int, shared_ptr<map<mreq_id_t, server_in_req_entry_t> > >::iterator i_sender 
            = m_server_in_req_table.begin();
            i_sender != m_server_in_req_table.end(); ++i_sender) {
        for (map<mreq_id_t, server_in_req_entry_t>::iterator i_req = i_sender->second->begin(); 
                i_req != i_sender->second->end(); ++i_req) {
            /* processed by cache logic */
            if (i_req->second.status == REQ_BUSY) {
                assert(i_req->second.remaining_process_time > 0);
                --(i_req->second.remaining_process_time);
                if (i_req->second.remaining_process_time == 0) {
                    i_req->second.status = REQ_PROCESSED;
                }
            }
            /* actions in cache hierarchy */
            if (i_req->second.status == REQ_PROCESSED) {
                shared_ptr<memoryRequest> req = i_req->second.req;
                uint32_t tgt_level = i_req->second.target_level;
                assert(m_memory_hierarchy[tgt_level]); /* this node must have memory system for the level */
                LOG(log,3) << "[server " << get_id().get_numeric_id() << " @ " << system_time 
                           << " ] initiated local memory requests for a message from " << i_req->second.sender << endl;
                mreq_id_t local_req_id = m_memory_hierarchy[tgt_level]->request(req);
                if (m_server_local_req_table.count(tgt_level) == 0) {
                    m_server_local_req_table[tgt_level] = shared_ptr<map<mreq_id_t, server_local_req_entry_t> > 
                        (new map<mreq_id_t, server_local_req_entry_t>);
                }
                (*(m_server_local_req_table[tgt_level]))[local_req_id].mem_to_serve = m_memory_hierarchy[tgt_level];
                (*(m_server_local_req_table[tgt_level]))[local_req_id].sender = i_sender->first;
                (*(m_server_local_req_table[tgt_level]))[local_req_id].in_req_id = i_req->first;
                i_req->second.status = REQ_WAIT;
            }
        }
    }

    /* accepts request */
    msg_type_t mem_reqs[] = {MSG_RA_REQ, MSG_MEM_REQ};
    vector<msg_type_t> mem_req_vec (mem_reqs, mem_reqs + sizeof(mem_reqs)/sizeof(msg_type_t));
    for (vector<msg_type_t>::iterator i_queue = mem_req_vec.begin(); i_queue != mem_req_vec.end(); ++i_queue) {
        uint32_t num_msgs = m_in_msg_queues[*i_queue]->size();
        for (uint32_t i_msg = 0; i_msg < num_msgs && m_num_active_remote_mem_requests < m_cfgs.max_active_remote_mem_requests; 
                ++i_msg) {
            int sender = m_in_msg_queues[*i_queue]->front().src;
            mreq_id_t in_req_id = m_in_msg_queues[*i_queue]->front().mem_msg.req_id;
            if (m_server_in_req_table.count(sender) == 0) {
                m_server_in_req_table[sender] = shared_ptr<map<mreq_id_t, server_in_req_entry_t> > (
                        new map<mreq_id_t, server_in_req_entry_t>);
            }
            (*m_server_in_req_table[sender])[in_req_id].status = REQ_INIT;
            (*m_server_in_req_table[sender])[in_req_id].msg_type = *i_queue;
            (*m_server_in_req_table[sender])[in_req_id].sender = m_in_msg_queues[*i_queue]->front().src;
            (*m_server_in_req_table[sender])[in_req_id].target_level 
                = m_in_msg_queues[*i_queue]->front().mem_msg.target_level;
            (*m_server_in_req_table[sender])[in_req_id].req = m_in_msg_queues[*i_queue]->front().mem_msg.req;
            m_in_msg_queues[*i_queue]->pop();
            ++m_num_active_remote_mem_requests;
            LOG(log,3) << "[server " << get_id().get_numeric_id() << " @ " << system_time
                        << " ] received a memory request from " << sender << endl;
        }
    }
}

/* Never used */
void core::add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err) { assert(false); }
bool core::work_queued() throw(err) { assert(false); return false; }
bool core::is_ready_to_offer() throw(err) { assert(false); return false; }
void core::set_stop_darsim() throw(err) { assert(false); }

uint64_t core::next_pkt_time() throw(err) {
    return system_time;
}
