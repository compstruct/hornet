// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SYSCALLS_HPP__
#define __SYSCALLS_HPP__

typedef enum {
    // network communication
    SYSCALL_SEND = 0x80,
    SYSCALL_RECEIVE = 0x81,
    SYSCALL_TRANSMISSION_DONE = 0x82,
    SYSCALL_WAITING_QUEUES = 0x83,
    SYSCALL_PACKET_FLOW = 0x84,
    SYSCALL_PACKET_LENGTH = 0x85,

    // simulator convenience functions
    SYSCALL_PRINT_INT = 0x00,
    SYSCALL_PRINT_STRING = 0x04,
    SYSCALL_EXIT_SUCCESS = 0x0a,
    SYSCALL_EXIT = 0x11
} mips_syscall;

#endif // __SYSCALLS_HPP__

