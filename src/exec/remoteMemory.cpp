// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "remoteMemory.hpp"

remoteMemory::remoteMemory(const uint32_t numeric_id, const uint64_t &system_time, 
                           logger &log, shared_ptr<random_gen> ran)
: memory(numeric_id, system_time, log, ran) {
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
    return 0;
}

bool remoteMemory::ready(mreq_id_t id) {
    return true;
}

bool remoteMemory::finish(mreq_id_t id) {
    return true;
}

void remoteMemory::initiate() {
}

void remoteMemory::update() {
}

void remoteMemory::process() {
}
