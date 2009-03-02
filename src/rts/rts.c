/* -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-  */
/* vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0: */

#include "rts.h"

unsigned cpu_id() {
    unsigned result;
    asm ("rdhwr %0, $0;" : "=r"(result));
    return result;
}

unsigned cpu_cycle_counter() {
    unsigned result;
    asm ("rdhwr %0, $2;" : "=r"(result));
    return result;
}

unsigned cpu_cycle_counter_resolution() {
    unsigned result;
    asm ("rdhwr %0, $4;" : "=r"(result));
    return result;
}

extern unsigned send(unsigned flow_id, const void *src, unsigned length) {
    unsigned result;
    asm volatile ("move $a0, %1; move $a1, %2; move $a2, %3;"
                  "addiu $v0, $0, 0x80; syscall; move %0, $v0;"
                  : "=r"(result)
                  : "r"(flow_id), "r"(src), "r"(length)
                  : "a0", "v0" );
    return result;
}

extern unsigned receive(unsigned queue_id, void *dst, unsigned length) {
    unsigned result;
    asm ("move $a0, %1; move $a1, %2; move $a2, %3;"
         "addiu $v0, $0, 0x81; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(queue_id), "r"(dst), "r"(length)
         : "a0", "a1", "a2", "v0", "memory" );
    return result;
}

unsigned transmission_done(unsigned xmit_id) {
    unsigned result;
    asm ("move $a0, %1; addiu $v0, $0, 0x82; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(xmit_id)
         : "a0", "v0" );
    return result;
}

unsigned waiting_queues() {
    unsigned result;
    asm ("addiu $v0, $0, 0x83; syscall; move %0, $v0;"
         : "=r"(result)
         :
         : "v0" );
    return result;
}

unsigned next_packet_flow(unsigned queue) {
    unsigned result;
    asm ("move $a0, %1; addiu $v0, $0, 0x84; syscall; move %0, $v0;" \
         : "=r"(result)
         : "r"(queue)
         : "a0", "v0" );
    return result;
}

unsigned next_packet_length(unsigned queue) {
    unsigned result;
    asm ("move $a0, %1; addiu $v0, $0, 0x85; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(queue)
         : "a0", "v0" );
    return result;
}

void print_int(int n) {
    asm volatile ("move $a0, %0; move $v0, $0; syscall;"
                  :
                  : "r"(n)
                  : "a0", "v0" );
}

void print_string(const char *s) {
    asm volatile ("move $a0, %0; addiu $v0, $0, 4; syscall;"
                  :
                  : "r"(s)
                  : "a0", "v0" );
}

void halt() {
    asm volatile ("addiu $v0, $0, 0xa; syscall;"
                  :
                  :
                  : "v0" );
}

void __start() {
    char arg[4] = { 'd', 'a', 'r', '\0' };
    main(1, &arg);
    halt();
}

