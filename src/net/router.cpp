// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "router.hpp"

router::router(node_id i, logger &l) : id(i), ingresses(), log(l), m_multi_path_routing(RT_PROBABILITY) { }

router::~router() { }

void router::add_ingress(std::shared_ptr<ingress> ing) {
    ingresses.push_back(ing);
}
