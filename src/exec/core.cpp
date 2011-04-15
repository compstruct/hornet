// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "core.hpp"

core::core(const pe_id &id, const uint64_t &t, shared_ptr<id_factory<packet_id> > pif,
           shared_ptr<tile_statistics> st, logger &l, shared_ptr<random_gen> r, 
           shared_ptr<memory> mem,
           uint32_t number_of_core_msg_types, uint32_t msg_q_size, uint32_t bytes_per_flit) throw(err) :
    pe(id), system_time(t), stats(st), log(l), ran(r), 
    m_msg_queue_size(msg_q_size), m_bytes_per_flit(bytes_per_flit), m_memory(mem), m_packet_id_factory(pif), 
    m_receive_channel_round_robin_pointer(0), m_send_queue_round_robin_pointer(0)
{ 
    m_number_of_msg_types = number_of_core_msg_types + mem->number_of_mem_msg_types();
    m_first_core_msg_type = mem->number_of_mem_msg_types();
    for (uint32_t i = 0; i < m_number_of_msg_types; ++i) {
        m_in_msg_queues[i] = shared_ptr<messageQueue> (new messageQueue(i, m_msg_queue_size));
        m_out_msg_queues[i] = shared_ptr<messageQueue> (new messageQueue(i, m_msg_queue_size));
    }
    m_memory->set_core_send_queues(m_out_msg_queues);
    m_memory->set_core_receive_queues(m_in_msg_queues);
}

core::~core() throw() { }

void core::connect(shared_ptr<bridge> net_bridge) throw(err) {
    m_net = net_bridge;
    shared_ptr<vector<uint32_t> > qs = m_net->get_ingress_queue_ids();
    m_queue_ids.clear();
    copy(qs->begin(), qs->end(), back_insert_iterator<vector<uint32_t> >(m_queue_ids));
}

shared_ptr<messageQueue> core::receive_queue(uint32_t type) {
    assert((uint32_t)type < m_number_of_msg_types);
    return m_in_msg_queues[type];
}

shared_ptr<messageQueue> core::send_queue(uint32_t type) {
    assert((uint32_t)type < m_number_of_msg_types);
    return m_out_msg_queues[type];
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

    /* send messages in send queues */
    for (uint32_t it = 0; it < m_number_of_msg_types; ++it) {
        uint32_t type =  (m_send_queue_round_robin_pointer + it)%m_number_of_msg_types;
        shared_ptr<messageQueue> send_q = m_out_msg_queues[type];
        if (send_q->size() == 0) {
            continue;
        }
        /* allcate new memory for the message (alive until it gets the destination) */
        message_t *msg = new message_t;
        *msg = *(send_q->front());
        uint64_t *flits = new uint64_t[msg->flit_count];
        flits[0] = (uint64_t)(msg); /* The first data flit has the pointer to the message and other data flits are dummies */
        packet_id pid = m_packet_id_factory->get_fresh_id();
        /* make it sure to have the correct values. could be redundant */
        msg->type = type;
        msg->src = get_id().get_numeric_id();
        uint32_t flow_id = ((((type << 8) | msg->src) << 8) | msg->dst) << 8;
        uint32_t xid = m_net->send(flow_id, (void*)flits, msg->flit_count, pid, stats->is_started());
        if (xid != 0) {
            /* packet is said to be 'offered' when it begins to move... */ 
            /* this is somewhat different from previous cases, where a packet is offered when it gets in the queue */
            stats->offer_packet(flow_id, pid, msg->flit_count);
            /* save in xmit buffer until the transfer finished */
            m_xmit_buffer[xid] = flits;
            LOG(log, 4) << "[core " << get_id().get_numeric_id() << " @ " << system_time
                << " ] has sent a message type " << (uint32_t) type << " to " << msg->dst << endl;
            m_out_msg_queues[type]->pop();
        } else {
            /* retry later */
            delete[] flits;
        }
    }
    if (++m_send_queue_round_robin_pointer == m_number_of_msg_types) {
        /* increase the round robin pointer */
        m_send_queue_round_robin_pointer = 0;
    }

    execute();
    if (m_memory != shared_ptr<memory>()) {
        m_memory->tick_positive_edge();
    }

    /* receive messages in to receive queues */
    for (uint32_t it = 0; it < 32 ; ++it) {
        uint32_t channel = (it + m_receive_channel_round_robin_pointer)%32;
        if (m_incoming_packets.find(channel) != m_incoming_packets.end()) {
            core_incoming_packet_t &ip = m_incoming_packets[channel];
            if (m_net->get_transmission_done(ip.xmit)) {
                /* old_msg (regular pointer) is deleted once received */
                /* new_msg (smart pointer) is pushed back into the queues */
                message_t* old_msg = (message_t*)((uint64_t)ip.payload[0]);
                shared_ptr<message_t> new_msg = shared_ptr<message_t>(new message_t);
                *new_msg = *old_msg;
                if (m_in_msg_queues[new_msg->type]->push_back(new_msg)) {
                    LOG(log,3) << "[core " << get_id().get_numeric_id() << " @ " << system_time
                        << " ] has received a message type " << (uint32_t) new_msg->type 
                        << " waiting queue size "
                        << m_in_msg_queues[new_msg->type]->size() << endl;
                    m_incoming_packets.erase(channel);
                    delete old_msg;
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

    /* update requests */
    commit_memory_requests();

    if (m_memory != shared_ptr<memory>()) {
        m_memory->tick_negative_edge();
    }
}

/* not used methods */
void core::add_packet(uint64_t time, const flow_id &flow, uint32_t len) throw(err) { assert(false); }
bool core::work_queued() throw(err) { assert(false); return false; }
bool core::is_ready_to_offer() throw(err) { assert(false); return false; }
void core::set_stop_darsim() throw(err) { assert(false); }

uint64_t core::next_pkt_time() throw(err) {
    return system_time;
}
