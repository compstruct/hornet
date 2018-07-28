// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include "endian.hpp"
#include "main_mem.hpp"

main_mem::main_mem(uint32_t new_id, uint32_t new_start, uint32_t new_size,
                   logger &new_log)
    : id(new_id), start(new_start), size(new_size),
      contents(new uint8_t[new_size]), interfaces(), log(new_log) {
    if (contents == NULL) throw err_out_of_mem();
    LOG(log,3) << "memory segment " << hex << setfill('0') << setw(2)
               << get_id() << " created starting at "
               << hex << setfill('0') << setw(8) << start << " and containing "
               << dec << size << " bytes" << endl;
};

main_mem::~main_mem() {
    assert(contents != NULL);
    if (contents != NULL) delete[] contents;
};

shared_ptr<mem_ifc> main_mem::new_interface() {
    shared_ptr<mem_ifc::reqs_t> reqs =
        shared_ptr<mem_ifc::reqs_t>(new mem_ifc::reqs_t());
    shared_ptr<mem_ifc::resps_t> resps =
        shared_ptr<mem_ifc::resps_t>(new mem_ifc::resps_t());
    shared_ptr<mem_ifc> ifc = shared_ptr<mem_ifc>(new mem_ifc(reqs, resps));
    interfaces.push_back(ifc);
    return ifc;
}

void main_mem::tick_positive_edge() {
    assert(contents);
    for (interfaces_t::iterator ii = interfaces.begin();
         ii != interfaces.end(); ++ii) {
        shared_ptr<mem_ifc> ifc = *ii;
        while (!ifc->requests->empty()) {
            mem_req &req = ifc->requests->front();
            mem_resp resp;
            resp.tag = req.tag;
            switch (req.type) {
            case MR_LOAD_8:
                resp.data = *((uint8_t *) ptr(req.addr));
                LOG(log,5) << "[mem " << get_id() << "]     "
                    << hex << setfill('0') << setw(8) << req.addr
                    << " -> " << setw(2) << resp.data << endl;
                break;
            case MR_LOAD_16:
                resp.data = endian(*((uint16_t *) ptr(req.addr)));
                LOG(log,5) << "[mem " << get_id() << "]     "
                    << hex << setfill('0') << setw(8) << req.addr
                    << " -> " << setw(4) << resp.data << endl;
                break;
            case MR_LOAD_32:
                resp.data = endian(*((uint32_t *) ptr(req.addr)));
                LOG(log,5) << "[mem " << get_id() << "]     "
                    << hex << setfill('0') << setw(8) << req.addr
                    << " -> " << setw(8) << resp.data << endl;
                break;
            case MR_STORE_8:
                assert((req.data & 0xff) == req.data);
                *((uint8_t *) ptr(req.addr)) = (uint8_t) req.data;
                LOG(log,5) << "[mem " << get_id() << "]     "
                    << hex << setfill('0') << setw(8) << req.addr
                    << " <- " << setw(2) << req.data << endl;
                break;
            case MR_STORE_16:
                assert((req.data & 0xffff) == req.data);
                *((uint16_t *) ptr(req.addr)) = endian((uint16_t) req.data);
                LOG(log,5) << "[mem " << get_id() << "]     "
                    << hex << setfill('0') << setw(8) << req.addr
                    << " <- " << setw(4) << req.data << endl;
                break;
            case MR_STORE_32:
                *((uint32_t *) ptr(req.addr)) = endian(req.data);
                LOG(log,5) << "[mem " << get_id() << "]     "
                    << hex << setfill('0') << setw(8) << req.addr
                    << " <- " << setw(8) << req.data << endl;
                break;
            default:
                throw err_bad_mem_req(req.type);
            }
            ifc->responses->push(resp);
            ifc->requests->pop();
        }
    }
}

void main_mem::tick_negative_edge() { }

