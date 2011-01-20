// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "dramController.hpp"

dramController::dramController(const uint32_t id, const uint64_t &t, 
                               logger &log, shared_ptr<random_gen> ran,
                               bool use_lock )
: memory(id, t, log, ran), m_protected(use_lock) {
}

dramController::~dramController() {}

mreq_id_t dramController::request(shared_ptr<memoryRequest> req) {
    return 0;
}

bool dramController::ready(mreq_id_t id) {
    return true;
}

bool dramController::finish(mreq_id_t id) {
    return true;
}

void dramController::initiate() {
}

void dramController::update() {
}

void dramController::process() {
}


