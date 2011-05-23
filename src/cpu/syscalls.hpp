// -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-
// vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0:

#ifndef __SYSCALLS_HPP__
#define __SYSCALLS_HPP__

typedef enum {
    // function intrinsics
    SYSCALL_SQRT_S = 0x40,
    SYSCALL_LOG_S = 0x41,
    SYSCALL_EXP_S = 0x42,
    SYSCALL_SQRT_D = 0x50,
    SYSCALL_LOG_D = 0x51,
    SYSCALL_EXP_D = 0x52,

    // File I/O
    SYSCALL_FOPEN = 0x60,
    SYSCALL_READ_LINE = 0x61,
    SYSCALL_FCLOSE = 0x62,    

    // Uncached accesses
    SYSCALL_ENABLE_MEMORY_HIERARCHY = 0x77,
    SYSCALL_THREAD_ID = 0x78,

    // network communication
    SYSCALL_SEND = 0x80,
    SYSCALL_RECEIVE = 0x81,
    SYSCALL_TRANSMISSION_DONE = 0x82,
    SYSCALL_WAITING_QUEUES = 0x83,
    SYSCALL_PACKET_FLOW = 0x84,
    SYSCALL_PACKET_LENGTH = 0x85,

    // simulator convenience functions
    SYSCALL_PRINT_INT = 0x00,
    SYSCALL_PRINT_CHAR = 0x01,    
    SYSCALL_PRINT_STRING = 0x04,
    SYSCALL_PRINT_FLOAT = 0x06,
    SYSCALL_PRINT_DOUBLE = 0x07,
    SYSCALL_FLUSH = 0x09,
    SYSCALL_EXIT_SUCCESS = 0x0a,
    SYSCALL_EXIT = 0x11,
    SYSCALL_ASSERT = 0x12,
} mips_syscall;

#endif // __SYSCALLS_HPP__

