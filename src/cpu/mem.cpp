// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#include <iomanip>
#include <cassert>
#include "mem.hpp"

mem::mem(uint32_t new_start, uint32_t new_size,
        shared_ptr<logger> new_log) throw(err)
    : start(new_start), size(new_size), contents(new uint8_t[new_size]),
      log(new_log) {
    if (contents == NULL) throw err_out_of_mem();
    log << verbosity(3) << "memory segment created starting at "
        << hex << setfill('0') << setw(8) << start << " and containing "
        << dec << size << " bytes" << endl;
};

mem::~mem() throw() {
    assert(contents != NULL);
    if (contents != NULL) delete contents;
};

