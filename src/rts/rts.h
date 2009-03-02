/* -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-  */
/* vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0: */

#ifndef __RTS_H__
#define __RTS_H__

/* INTERFACE */

/* MIPS PRIMITIVES */

/* returns the ID of the CPU on which the program is executing */
extern unsigned cpu_id() __attribute__((const));

/* returns the CPU cycle counter (see also resolution below) */
extern unsigned cpu_cycle_counter();

/* returns n s.t. the CPU cycle counter is incremented every n real cycles */
extern unsigned cpu_cycle_counter_resolution() __attribute__((const));


/* NETWORK INTERFACE */

/* effects: transmits a length-byte packet starting at src on flow flow_id
 *          (if length is not divisible by 8 then the effects are undefined)
 * returns: non-zero transmission ID suitable for passing to transmitted()
 *          or 0 on failure (e.g., when all channels are busy)
 */
extern unsigned send(unsigned flow_id, const void *src, unsigned length);

/* effects: copies length bytes from queue queue_id to buffer starting at dst
 *          (if length is not divisible by 8 then the effects are undefined)
 * returns: non-zero transmission ID suitable for passing to transmitted()
 *          or 0 on failure (e.g., when all channels are busy)
 */
extern unsigned receive(unsigned queue_id, void *dst, unsigned length);

/* effects: none
 * returns: returns non-zero if transmission xmit_id has completed,
 *          or 0 otherwise
 */
extern unsigned transmission_done(unsigned xmit_id) ;

/* effects: none
 * returns: returns a bit vector where bit n is set if and only if
 *          queue n has waiting data
 */
extern unsigned waiting_queues();

/* effects: none
 * returns: returns the flow ID of the packet at the head of the given queue
 */
extern unsigned next_packet_flow(unsigned queue);

/* effects: none
 * returns: returns the remaining number of bytes in the packet
 *          at the head of the given queue
 */
extern unsigned next_packet_length(unsigned queue);


/* SIMULATOR-ONLY CONVENIENCE FUNCTIONS */

/* effects: prints the given integer to standard output (line-buffered) */
extern void print_int(int);

/* effects: prints the given string to standard output (line-buffered) */
extern void print_string(const char *);

/* effects: halts the processor */
extern void halt() __attribute__((noreturn));



/* IMPLEMENTATION */

inline unsigned cpu_id() {
    unsigned result;
    asm ("rdhwr %0, $0;" : "=r"(result));
    return result;
}

inline unsigned cpu_cycle_counter() {
    unsigned result;
    asm ("rdhwr %0, $2;" : "=r"(result));
    return result;
}

inline unsigned cpu_cycle_counter_resolution() {
    unsigned result;
    asm ("rdhwr %0, $4;" : "=r"(result));
    return result;
}

inline extern unsigned send(unsigned flow_id, const void *src, unsigned len) {
    unsigned result;
    asm volatile ("move $a0, %1; move $a1, %2; move $a2, %3;"
                  "addiu $v0, $0, 0x80; syscall; move %0, $v0;"
                  : "=r"(result)
                  : "r"(flow_id), "r"(src), "r"(len)
                  : "a0", "v0" );
    return result;
}

inline extern unsigned receive(unsigned queue_id, void *dst, unsigned len) {
    unsigned result;
    asm ("move $a0, %1; move $a1, %2; move $a2, %3;"
         "addiu $v0, $0, 0x81; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(queue_id), "r"(dst), "r"(len)
         : "a0", "a1", "a2", "v0", "memory" );
    return result;
}

inline unsigned transmission_done(unsigned xmit_id) {
    unsigned result;
    asm ("move $a0, %1; addiu $v0, $0, 0x82; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(xmit_id)
         : "a0", "v0" );
    return result;
}

inline unsigned waiting_queues() {
    unsigned result;
    asm ("addiu $v0, $0, 0x83; syscall; move %0, $v0;"
         : "=r"(result)
         :
         : "v0" );
    return result;
}

inline unsigned next_packet_flow(unsigned queue) {
    unsigned result;
    asm ("move $a0, %1; addiu $v0, $0, 0x84; syscall; move %0, $v0;" \
         : "=r"(result)
         : "r"(queue)
         : "a0", "v0" );
    return result;
}

inline unsigned next_packet_length(unsigned queue) {
    unsigned result;
    asm ("move $a0, %1; addiu $v0, $0, 0x85; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(queue)
         : "a0", "v0" );
    return result;
}

inline void print_int(int n) {
    asm volatile ("move $a0, %0; move $v0, $0; syscall;"
                  :
                  : "r"(n)
                  : "a0", "v0" );
}

inline void print_string(const char *s) {
    asm volatile ("move $a0, %0; addiu $v0, $0, 4; syscall;"
                  :
                  : "r"(s)
                  : "a0", "v0" );
}

inline void halt() {
    asm volatile ("addiu $v0, $0, 0xa; syscall;"
                  :
                  :
                  : "v0" );
}

#endif /* __RTS_H__ */

