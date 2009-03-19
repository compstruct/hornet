// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include "channel_alloc.hpp"

channel_alloc::channel_alloc(node_id new_id, logger &l)
    throw() : id(new_id), log(l) { }

channel_alloc::~channel_alloc() throw() { }

