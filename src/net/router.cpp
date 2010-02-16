// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "router.hpp"

router::router(node_id i, logger &l) throw() : id(i), ingresses(), log(l) { }

router::~router() throw() { }

void router::add_ingress(shared_ptr<ingress> ing) throw(err) {
    ingresses.push_back(ing);
}
