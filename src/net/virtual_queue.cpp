// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include <sstream>
#include "virtual_queue.hpp"
#include "channel_alloc.hpp"

virtual_queue::virtual_queue(node_id new_node_id, virtual_queue_id new_vq_id,
                             node_id new_src_node_id, 
                             ingress_id new_ingress_id,
                             uint32_t new_max_size,
                             std::shared_ptr<channel_alloc> new_vc_alloc,
                             std::shared_ptr<pressure_tracker> new_pt,
                             std::shared_ptr<tile_statistics> st,
                             std::shared_ptr<vcd_writer> new_vcd,
                             logger &l)
    : id(make_tuple(new_node_id, new_vq_id)), src_node_id(new_src_node_id),
      parent_ingress_id(new_ingress_id),
      buffer_size(new_max_size + 1), contents(new_max_size + 1),
      front_head(0), front_stale_tail(0),
      front_egress_packet_flits_remaining(0), front_old_flow(),
      front_next_hop_node(), front_next_hop_flow(), front_next_hop_vq(),
      front_powered(true),
      back_stale_head(0), back_tail(0),
      back_ingress_packet_flits_remaining(0),
      back_stale_egress_packet_flits_remaining(0),
      back_powered(true),
      vc_alloc(new_vc_alloc), pressures(new_pt),
      stats(st), vcd(new_vcd), log(l) {
    if (vcd) {
        vector<string> path;
        ostringstream oss;
        oss << get<0>(id) << "_" << get<1>(id);
        path.push_back("queues");
        path.push_back("size");
        path.push_back(oss.str());
        vcd->new_signal(&vcd_hooks.v_size, path, 32);
    }
}

const virtual_queue_node_id &virtual_queue::get_id() const {
    return id;
}

node_id virtual_queue::get_src_node_id() const {
    return src_node_id;
}

ingress_id virtual_queue::get_ingress_id() const { 
    return parent_ingress_id;
}

bool virtual_queue::front_is_empty() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    return front_head == front_stale_tail;
}

bool virtual_queue::front_is_head_flit() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    return front_egress_packet_flits_remaining == 0;
}

node_id virtual_queue::front_node_id() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    return front_next_hop_node;
}

virtual_queue_id virtual_queue::front_vq_id() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    return front_next_hop_vq;
}

flow_id virtual_queue::front_old_flow_id() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    assert(front_old_flow.is_valid());
    return front_old_flow;
}

flow_id virtual_queue::front_new_flow_id() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    return front_next_hop_flow;
}

uint32_t virtual_queue::front_num_remaining_flits_in_packet() const {
    // returns the # of flits past the head flit
    unique_lock<recursive_mutex> lock(front_mutex);
    if (!front_is_empty() && front_is_head_flit()) {
        const head_flit &h = reinterpret_cast<const head_flit &>(front_flit());
        return h.get_length();
    } else {
        return front_egress_packet_flits_remaining;
    }
}

const flit &virtual_queue::front_flit() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    return contents[front_head];
}


uint32_t virtual_queue::front_size() const {
    unique_lock<recursive_mutex> lock(front_mutex);
    return (front_stale_tail >= front_head ? front_stale_tail - front_head
            : front_stale_tail + buffer_size - front_head);
}

void virtual_queue::front_pop() {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    if (front_egress_packet_flits_remaining == 0) { // head, update flit count
        assert(front_old_flow.is_valid()); // set in tick_negative_edge()
        assert(front_next_hop_node.is_valid()); // otherwise, why pop()?
        assert(front_next_hop_flow.is_valid()); // otherwise, why pop()?
        assert(front_next_hop_vq.is_valid()); // otherwise, why pop()?
        const head_flit &h =
            reinterpret_cast<const head_flit &>(contents[front_head]);
        front_egress_packet_flits_remaining = h.get_length();
        LOG(log,3) << "[queue " << get_id() << "] egress: " << h << endl;
    } else {
        --front_egress_packet_flits_remaining;
        LOG(log,3) << "[queue " << get_id() << "] egress: "
                   << contents[front_head] << endl;
    }
    front_head = (front_head + 1) % buffer_size;
    if (front_egress_packet_flits_remaining == 0) {
        virtual_queue_node_id vqnid(front_next_hop_node, front_next_hop_vq);
        front_vqs_to_release_at_negedge.push_back(vqnid);
        front_next_hop_node = node_id(); // invalidate
        front_next_hop_flow = flow_id(); // invalidate
        front_next_hop_vq = virtual_queue_id(); // invalidate
    }
}

void virtual_queue::front_set_next_hop(const node_id &nid,
                                       const flow_id &fid) {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    assert(front_is_head_flit());
    assert(!front_next_hop_node.is_valid());
    assert(!front_next_hop_flow.is_valid());
    assert(!front_next_hop_vq.is_valid());
    assert(nid.is_valid());
    assert(fid.is_valid());
    front_next_hop_node = nid;
    front_next_hop_flow = fid;
    head_flit &h = reinterpret_cast<head_flit &>(contents[front_head]);
    if (h.get_flow_id() != fid) h.set_flow_id(fid);
}

void virtual_queue::front_set_vq_id(const virtual_queue_id &vqid) {
    unique_lock<recursive_mutex> lock(front_mutex);
    assert(!front_is_empty());
    assert(front_is_head_flit());
    assert(front_next_hop_node.is_valid());
    assert(front_next_hop_flow.is_valid());
    assert(!front_next_hop_vq.is_valid());
    assert(vqid.is_valid());
    virtual_queue_node_id vqnid(front_next_hop_node, vqid);
    vc_alloc->claim(vqnid);
    front_next_hop_vq = vqid;
}

void virtual_queue::front_power_on() {
    unique_lock<recursive_mutex> lock(front_mutex);
    front_powered = true;
}

void virtual_queue::front_power_off() {
    unique_lock<recursive_mutex> lock(front_mutex);
    front_powered = false;
}

bool virtual_queue::back_is_full() const {
    unique_lock<recursive_mutex> lock(back_mutex);
    return ((back_tail + 1) % buffer_size) == back_stale_head;
}

void virtual_queue::back_push(const flit &f) {
    unique_lock<recursive_mutex> lock(back_mutex);
    assert(!back_is_full());
    contents[back_tail] = f;
    back_tail = (back_tail + 1) % buffer_size;
    if (back_ingress_packet_flits_remaining == 0) { // head flit
        const head_flit &h =
            reinterpret_cast<const head_flit &>(f);
        back_ingress_packet_flits_remaining = h.get_length();
        LOG(log,3) << "[queue " << get_id() << "] ingress: " << h << endl;
    } else {
        --back_ingress_packet_flits_remaining;
        LOG(log,3) << "[queue " << get_id() << "] ingress: " << f << endl;
    }
}

bool virtual_queue::back_is_mid_packet() const {
    unique_lock<recursive_mutex> lock(back_mutex);
    return back_ingress_packet_flits_remaining != 0;
}

bool virtual_queue::back_is_empty() const {
    unique_lock<recursive_mutex> lock(back_mutex);
    return back_stale_head == back_tail;
}

bool virtual_queue::back_has_old_flow(const flow_id &f) {
    unique_lock<recursive_mutex> lock(back_mutex);
    uint32_t remaining = back_stale_egress_packet_flits_remaining;
    for (virtual_queue::buffer_t::size_type i = back_stale_head;
         i != back_tail; i = (i + 1) % buffer_size) {
        if (remaining == 0) {
            const head_flit &h =
                reinterpret_cast<const head_flit &>(contents[i]);
            remaining = h.get_length();
            if (h.get_flow_id() == f) {
                return true;
            }
        } else {
            --remaining;
        }
    }
    return false;
}

bool virtual_queue::back_is_powered_on() const {
    unique_lock<recursive_mutex> lock(back_mutex);
    return back_powered;
}

void virtual_queue::tick_positive_edge() { }

// synchronize front and back views of the virtual queue
void virtual_queue::tick_negative_edge() {
    unique_lock<recursive_mutex> front_lock(front_mutex);
    unique_lock<recursive_mutex> back_lock(back_mutex);
    if ((front_head != back_stale_head // head changed
         || (front_stale_tail == back_stale_head // was empty before
             && back_tail != front_stale_tail)) // but no longer
        && front_egress_packet_flits_remaining == 0) { // head
        const head_flit &h =
            reinterpret_cast<const head_flit &>(contents[front_head]);
        front_old_flow = h.get_flow_id();
    }
    back_powered = front_powered;
    back_stale_head = front_head;
    front_stale_tail = back_tail;
    back_stale_egress_packet_flits_remaining =
        front_egress_packet_flits_remaining;
    typedef vector<virtual_queue_node_id>::const_iterator vqi_t;
    for (vqi_t vqi = front_vqs_to_release_at_negedge.begin();
         vqi != front_vqs_to_release_at_negedge.end(); ++vqi) {
        vc_alloc->release(*vqi);
    }
    front_vqs_to_release_at_negedge.clear();
    for (virtual_queue::buffer_t::size_type i = front_head;
         i != front_stale_tail; i = (i + 1) % buffer_size) {
        contents[i].age();
    }
    if (vcd) {
        vcd->add_value(&vcd_hooks.v_size, front_size());
    }
}

bool virtual_queue::is_drained() const {
    unique_lock<recursive_mutex> front_lock(front_mutex);
    unique_lock<recursive_mutex> back_lock(back_mutex);
    return front_is_empty() && back_is_empty();
}

ostream &operator<<(ostream &out, const virtual_queue &vq) {
    unique_lock<recursive_mutex> front_lock(vq.front_mutex);
    unique_lock<recursive_mutex> back_lock(vq.back_mutex);
    uint32_t remaining = vq.front_egress_packet_flits_remaining;
    for (virtual_queue::buffer_t::size_type i = vq.front_head;
         i != vq.front_stale_tail; i = (i + 1) % vq.buffer_size) {
        virtual_queue::buffer_t::size_type index =
            (i >= vq.front_head ? i - vq.front_head
             : i + vq.buffer_size - vq.front_head);
        out << "[queue " << vq.get_id() << "] at [" << dec << index
            << "]: ";
        if (remaining == 0) {
            const head_flit &h =
                reinterpret_cast<const head_flit &>(vq.contents[i]);
            remaining = h.get_length();
            out << h;
        } else {
            out << vq.contents[i];
            --remaining;
        }
        out << endl;
    }
    return out;
}
