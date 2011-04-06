/* -*- mode:c++; c-style:k&r; c-basic-offset:4; indent-tabs-mode: nil; -*-  */
/* vi:set et cin sw=4 cino=>se0n0f0{0}0^0\:0=sl1g0hspst0+sc3C0/0(0u0U0w0m0: */

#ifndef __RTS_H__
#define __RTS_H__

/* MIPS core syscall API */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Hardware
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/* returns the ID of the CPU on which the program is executing */
static unsigned cpu_id() __attribute__((const));

/* returns the CPU cycle counter (see also resolution below) */
static unsigned cpu_cycle_counter();

/* returns n s.t. the CPU cycle counter is incremented every n real cycles */
static unsigned cpu_cycle_counter_resolution() __attribute__((const));

/* returns the ID of the current thread; in cases where the thread does not move 
   from its starting core, thread_id() == cpu_id() */
static unsigned thread_id();

/* effects: checks whether an asertion is true, and throws an error if it is 
   not. */
static void     __H_assert(int);

/* effects: calls exit() with the provided error code */
static void     __H_exit(int);

//------------------------ Implementation --------------------------------------

inline static unsigned cpu_id() {
    unsigned result;
    __asm__ ("rdhwr %0, $0;" : "=r"(result));
    return result;
}

inline static unsigned cpu_cycle_counter() {
    unsigned result;
    __asm__ __volatile__ ("rdhwr %0, $2;" : "=r"(result));
    return result;
}

inline static unsigned cpu_cycle_counter_resolution() {
    unsigned result;
    __asm__ ("rdhwr %0, $4;" : "=r"(result));
    return result;
}

inline static void __H_assert(int b) {
    __asm__ __volatile__
    ("move $a0, %0; addiu $v0, $0, 0x12; syscall;"
     : 
     : "r"(b)
     : "v0");
}

inline static unsigned thread_id() {
    int ret;
    __asm__ __volatile__
    ("addiu $v0, $0, 0x78; syscall; move %0, $v0;"
     : "=r"(ret)
     : 
     : "v0");
    return ret;
}

inline static void __H_exit(int code) {
    __asm__ __volatile__
    ("move $a0, %0; addiu $v0, $0, 0x11; syscall;"
     : 
     : "r"(code)
     : "a0", "v0");
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Memory Hierarchy 
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/* effects: turns on the Hornet memory hierarchy.  Before this function is 
   called, all loads and stores are magic single-cycle operations. This magic 
   mode is convenient when performing startup operations such as file I/O, as a 
   means of speeding up the simulation. */
static void     __H_enable_memory_hierarchy();

/* effects: loads a word directly from backing store (DRAM), bypassing the 
   memory hierarchy (regardless of whether __H_enable_memory_hierarchy has been 
   called.  This can be used by a master thread to check whether a set of slave 
   threads have finished a computation.
   returns: the loaded word. */
static int      __H_ucLoadWord(int *);

/* effects: sets a specified bit (given by its position relative to the LSB) in 
   the word specified by the address. The bit is set in a way that bypasses the 
   memory hierarchy, regardless of whether __H_enable_memory_hierarchy has been 
   called.  This can be used by slave threads to announce that they have 
   finished a task. */
static void     __H_ucSetBit(int *, int);

//------------------------ Implementation --------------------------------------

inline static void __H_enable_memory_hierarchy() {
    __asm__ __volatile__
    ("addiu $v0, $0, 0x77; syscall;"
     : 
     : 
     : "v0");
}

inline static int __H_ucLoadWord(int * addr) {
    int ret;
    __asm__ __volatile__
    ("move $a0, %1; addiu $v0, $0, 0x70; syscall; move %0, $v0;"
     : "=r"(ret)
     : "r"(addr)
     : "a0", "v0");
    return ret;
}
inline static void __H_ucSetBit(int * addr, int position) {
    __asm__ __volatile__
    ("move $a0, %0; move $a1, %1; addiu $v0, $0, 0x72; syscall;"
     : 
     : "r"(addr), "r"(position)
     : "a0", "a1", "v0");
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Printers
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#define __prefix_double_out__   unsigned long int bot_o; \
                                unsigned long int top_o;
#define __prefix_double_in__    unsigned long long int temp = *((unsigned long long int *) (&in)); \
                                unsigned long int bot_i = temp; \
                                unsigned long int top_i = temp >> 32;
#define __suffix_double__       unsigned long long int out = bot_o | (((unsigned long long int) top_o) << 32); \
                                union { \
		                                double f; \
		                                unsigned long long int i; \
	                                } u; \
	                                u.i = out; \
                                return u.f;

/* effects: prints various types to standard output (line-buffered) */
static void print_int(int);
static void print_string(const char *);
static void print_float(float);
static void print_double(double);
static void print_char(char);

/* effects: flush std out; 
   Note: all 'print_' functions place their output in std out. */
static void     __H_fflush();

//------------------------ Implementation --------------------------------------

inline static void print_int(int n) {
    __asm__ __volatile__
        ("move $a0, %0; move $v0, $0; syscall;"
         :
         : "r"(n)
         : "a0", "v0" );
}

inline static void print_string(const char *s) {
    __asm__ __volatile__
        ("move $a0, %0; addiu $v0, $0, 4; syscall;"
         :
         : "r"(s)
         : "a0", "v0" );
}

inline static void print_float(float n) {
    __asm__ __volatile__
        ("move $a0, %0; addiu $v0, $0, 0x06; syscall;"
         : 
         : "r"(n)
         : "a0", "v0" );
}

inline static void print_double(double in) {
    __prefix_double_in__
    __asm__ __volatile__
        ("move $a0, %0; move $a1, %1; addiu $v0, $0, 0x07; syscall;"
         :
         : "r"(bot_i), "r"(top_i) 
         : "a0", "a1", "v0");
}

inline static void print_char(char c) {
    __asm__ __volatile__
        ("move $a0, %0; addiu $v0, $0, 0x01; syscall;"
         :
         : "r"(c)
         : "a0",  "v0");
}

inline static void __H_fflush() {
    __asm__ __volatile__
    ("addiu $v0, $0, 0x09; syscall;"
     : 
     : 
     : "v0");
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Function intrinsics
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/* effects: perform single-point (_s) and double-point (_d) precision function 
            intrinsics. 
   returns: the float/double result */

// Single precision instrinsics

static float    __H_sqrt_s(float);
static float    __H_log_s(float);
static float    __H_exp_s(float);

// Double precision intrinsics 

static double   __H_sqrt_d(double);
static double   __H_log_d(double);
static double   __H_exp_d(double);

//------------------------ Implementation --------------------------------------

inline static float __H_sqrt_s(float in) {
    float result;
    __asm__ __volatile__
        ("move $a0, %1; addiu $v0, $0, 0x40; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(in)
         : "a0", "v0");
    return result;
}
inline static float __H_log_s(float in) {
    float result;
    __asm__ __volatile__
        ("move $a0, %1; addiu $v0, $0, 0x41; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(in)
         : "a0", "v0");
    return result;
}
inline static float __H_exp_s(float in) {
    float result;
    __asm__ __volatile__
        ("move $a0, %1; addiu $v0, $0, 0x42; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(in)
         : "a0", "v0");
    return result;
}

inline static double __H_sqrt_d(double in) {
    __prefix_double_in__
    __prefix_double_out__
    __asm__ __volatile__
        ("move $a0, %2; move $a1, %3; addiu $v0, $0, 0x50; syscall; move %0, $v0; move %1, $v1;"
         : "=r"(bot_o), "=r"(top_o) 
         : "r"(bot_i), "r"(top_i)
         : "a0", "a1", "v0", "v1");
    __suffix_double__
}
inline static double __H_log_d(double in) {
    __prefix_double_in__
    __prefix_double_out__
    __asm__ __volatile__
        ("move $a0, %2; move $a1, %3; addiu $v0, $0, 0x51; syscall; move %0, $v0; move %1, $v1;"
         : "=r"(bot_o), "=r"(top_o) 
         : "r"(bot_i), "r"(top_i)
         : "a0", "a1", "v0", "v1");
    __suffix_double__
}
inline static double __H_exp_d(double in) {
    __prefix_double_in__
    __prefix_double_out__
    __asm__ __volatile__
        ("move $a0, %2; move $a1, %3; addiu $v0, $0, 0x52; syscall; move %0, $v0; move %1, $v1;"
         : "=r"(bot_o), "=r"(top_o) 
         : "r"(bot_i), "r"(top_i)
         : "a0", "a1", "v0", "v1");
    __suffix_double__
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// File I/O
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/* effects: takes a filename and opens a file. 
   returns: a handle (unique id) to the opened file.
   NOTE: Only one file can be opened at a time right now.  In order to open 
   multiple files in a single program, call open/close multiple times. */
static int      __H_fopen(char *);

/* effects: takes a file handle (returned from __H_fopen), a destination buffer, 
   and a byte count, and writes the requested number of bytes from the file into 
   the buffer. 
   returns: the number of bytes successfully transferred. */
static int      __H_read_line(int, char *, int);

/* effects: given a file handle, closes the file. 
   returns: 0 if the operation was sucessfull. */
static int      __H_fclose(int);

//------------------------ Implementation --------------------------------------

inline static int __H_fopen(char * fname) {
    int ret;
    __asm__ __volatile__
    ("move $a0, %1; addiu $v0, $0, 0x60; syscall; move %0, $v0;"
     : "=r"(ret)
     : "r"(fname)
     : "a0", "v0");
    return ret;
}
inline static int __H_read_line(int fid, char * dest, int count) {
    int ret;
    __asm__ __volatile__
    ("move $a0, %1; move $a1, %2; move $a2, %3; addiu $v0, $0, 0x61; syscall; move %0, $v0;"
     : "=r"(ret)
     : "r"(fid), "r"(dest), "r"(count)
     : "a0", "a1", "a2", "v0");
    return ret;
}
inline static int __H_fclose(int fid) {    
    int ret;
    __asm__ __volatile__
    ("move $a0, %1; addiu $v0, $0, 0x62; syscall; move %0, $v0;"
     : "=r"(ret)
     : "r"(fid)
     : "a0", "v0");
    return ret;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Network Interface
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/* effects: transmits a length-byte packet starting at src on flow flow_id
 *          (if length is not divisible by 8 then the effects are undefined)
 * returns: non-zero transmission ID suitable for passing to transmitted()
 *          or 0 on failure (e.g., when all channels are busy)
 */
static unsigned send(unsigned flow_id, const void *src, unsigned length);

/* effects: copies length bytes from queue queue_id to buffer starting at dst
 *          (if length is not divisible by 8 then the effects are undefined)
 * returns: non-zero transmission ID suitable for passing to transmitted()
 *          or 0 on failure (e.g., when all channels are busy)
 */
static unsigned receive(unsigned queue_id, void *dst, unsigned length);

/* effects: none
 * returns: returns non-zero if transmission xmit_id has completed,
 *          or 0 otherwise
 */
static unsigned transmission_done(unsigned xmit_id) ;

/* effects: none
 * returns: returns a bit vector where bit n is set if and only if
 *          queue n has waiting data
 */
static unsigned waiting_queues();

/* effects: none
 * returns: returns the flow ID of the packet at the head of the given queue
 */
static unsigned next_packet_flow(unsigned queue);

/* effects: none
 * returns: returns the remaining number of bytes in the packet
 *          at the head of the given queue
 */
static unsigned next_packet_length(unsigned queue);

//------------------------ Implementation --------------------------------------

inline static unsigned send(unsigned flow_id, const void *src, unsigned len) {
    unsigned result;
    __asm__ __volatile__
        ("move $a0, %1; move $a1, %2; move $a2, %3;"
         "addiu $v0, $0, 0x80; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(flow_id), "r"(src), "r"(len)
         : "a0", "v0" );
    return result;
}

inline static unsigned receive(unsigned queue_id, void *dst, unsigned len) {
    unsigned result;
    __asm__
        ("move $a0, %1; move $a1, %2; move $a2, %3;"
         "addiu $v0, $0, 0x81; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(queue_id), "r"(dst), "r"(len)
         : "a0", "a1", "a2", "v0", "memory" );
    return result;
}

inline static unsigned transmission_done(unsigned xmit_id) {
    unsigned result;
    __asm__
        ("move $a0, %1; addiu $v0, $0, 0x82; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(xmit_id)
         : "a0", "v0" );
    return result;
}

inline static unsigned waiting_queues() {
    unsigned result;
    __asm__ __volatile__
        ("addiu $v0, $0, 0x83; syscall; move %0, $v0;"
         : "=r"(result)
         :
         : "v0" );
    return result;
}

inline static unsigned next_packet_flow(unsigned queue) {
    unsigned result;
    __asm__ __volatile__
        ("move $a0, %1; addiu $v0, $0, 0x84; syscall; move %0, $v0;" \
         : "=r"(result)
         : "r"(queue)
         : "a0", "v0" );
    return result;
}

inline static unsigned next_packet_length(unsigned queue) {
    unsigned result;
    __asm__ __volatile__
        ("move $a0, %1; addiu $v0, $0, 0x85; syscall; move %0, $v0;"
         : "=r"(result)
         : "r"(queue)
         : "a0", "v0" );
    return result;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Dynamic Memory Management
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/* Implementations of several dynamic memory management routines from the C 
standard libraries. (Malloc and friends) */

static void *_bottom, *_top, *_empty;

static void * memcpy(void *dst, const void *src, unsigned int len) __attribute__((unused));
static void * malloc(unsigned int) __attribute__((unused));
static void * realloc(void *oldp, unsigned int size) __attribute__((unused));
static void free(void *) __attribute__((unused));

/* If we assume that the OS will always give us more memory, we can ignore sbrk 
and brk */
//extern void *_sbrk(int);
//extern int _brk(void *);

// Memory management -----------------------------------------------------------

#ifdef _KERNEL
#include <types.h>
#include <lib.h>
#else
#include <string.h>
#endif

/* replace undef by define */
#undef   DEBUG          /* check assertions */
#undef   SLOWDEBUG      /* some extra test loops (requires DEBUG) */

#ifndef DEBUG
#define NDEBUG
#endif

#if _EM_WSIZE == _EM_PSIZE
#define ptrint          int
#else
#define ptrint          long
#endif

#if     _EM_PSIZE == 2
#define BRKSIZE         1024
#else
#define BRKSIZE         4096
#endif
#define PTRSIZE         ((int) sizeof(void *))
#define Align(x,a)      (((x) + (a - 1)) & ~(a - 1))
#define NextSlot(p)     (* (void **) ((p) - PTRSIZE))
#define NextFree(p)     (* (void **) (p))

static void * memcpy(void *dst, const void *src, unsigned int len) {
	unsigned int i;
	if ((unsigned int)dst % sizeof(long) == 0 &&
			(unsigned int)src % sizeof(long) == 0 &&
			len % sizeof(long) == 0) {

			long *d = dst;
			const long *s = src;

			for (i=0; i<len/sizeof(long); i++) d[i] = s[i];
	}
	else {
		char *d = dst;
		const char *s = src;
		for (i=0; i<len; i++) d[i] = s[i];
	}
	return dst;
}

static int grow(unsigned int len) {
	register char *p;

	//__H_assert(NextSlot((char *)_top) == 0);
	if ((char *) _top + len < (char *) _top
		|| (p = (char *)Align((ptrint)_top + len, BRKSIZE)) < (char *) _top ) {
		 //errno = ENOMEM; NOTE: Removed
		 return(0);
	}
	//if (_brk(p) != 0) return(0); NOTE: Removed
	NextSlot((char *)_top) = p;
	NextSlot(p) = 0;
	free(_top);
	_top = p;
	return 1;
}

static void * malloc(unsigned int size) {
	register char *prev, *p, *next, *new_;
	register unsigned len, ntries;

	if (size == 0) return NULL;

	for (ntries = 0; ntries < 2; ntries++) {
		if ((len = Align(size, PTRSIZE) + PTRSIZE) < 2 * PTRSIZE) {
			//errno = ENOMEM; NOTE: Removed
			return NULL;
		}
		if (_bottom == 0) {
			// if ((p = _sbrk(2 * PTRSIZE)) == (char *) -1) return NULL; NOTE: Removed
			p = (char *) Align((ptrint)p, PTRSIZE);
			p += PTRSIZE;
			_top = _bottom = p;
			NextSlot(p) = 0;
		}
#ifdef SLOWDEBUG
		for (p = _bottom; (next = NextSlot(p)) != 0; p = next) __H_assert(next > p);
		//__H_assert(p == _top);
#endif
		for (prev = 0, p = _empty; p != 0; prev = p, p = NextFree(p)) {
			next = NextSlot(p);
			new_ = p + len; /* easily overflows!! */
			if (new_ > next || new_ <= p)  continue; /* too small */
			if (new_ + PTRSIZE < next) { /* too big, so split */
				/* + PTRSIZE avoids tiny slots on free list */
				NextSlot(new_) = next;
				NextSlot(p) = new_;
				NextFree(new_) = NextFree(p);
				NextFree(p) = new_;
			}
			if (prev) NextFree(prev) = NextFree(p);
			else _empty = NextFree(p);
			/*print_string("Malloc returning: ");
			print_int(p);
			print_string("\n");
			__H_fflush();*/
			return p;
		}
		if (grow(len) == 0) break;
	}
	//__H_assert(ntries != 2);
	return NULL;
}

void * realloc(void *oldp, unsigned int size) {
	register char *prev, *p, *next, *new_;
	char *old = oldp;
	register unsigned int len, n;

	if (old == 0) return malloc(size);
	if (size == 0) {
		free(old);
		return NULL;
	}
	len = Align(size, PTRSIZE) + PTRSIZE;
	next = NextSlot(old);
	n = (int)(next - old); // old length
	// extend old if there is any free space just behind it
	for (prev = 0, p = _empty; p != 0; prev = p, p = NextFree(p)) {
		if (p > next) break;
		if (p == next) { // 'next' is a free slot: merge
		NextSlot(old) = NextSlot(p);
		if (prev)  NextFree(prev) = NextFree(p);
		else _empty = NextFree(p);
		next = NextSlot(old);
		break;
		}
	}
	new_ = old + len;
	// Can we use the old, possibly extended slot?
	if (new_ <= next && new_ >= old) { // it does fit 
		if (new_ + PTRSIZE < next) { // too big, so split 
			// + PTRSIZE avoids tiny slots on free list
			NextSlot(new_) = next;
			NextSlot(old) = new_;
			free(new_);
		}
		return old;
	}
	if ((new_ = malloc(size)) == NULL) return NULL; // it didn't fit
	memcpy(new_, old, n); // n < size
	free(old);
	return new_;
}

static void free(void *ptr) {
	register char *prev, *next;
	char *p = ptr;

	if (p == 0) return;

	//__H_assert(NextSlot(p) > p); NOTE: Removed
	for (prev = 0, next = _empty; next != 0; prev = next, next = NextFree(next))
		if (p < next) break;
	NextFree(p) = next;
	if (prev) NextFree(prev) = p;
	else _empty = p;
	if (next) {
		//__H_assert(NextSlot(p) <= next); NOTE: Removed
		if (NextSlot(p) == next) { /* merge p and next */
			NextSlot(p) = NextSlot(next);
			NextFree(p) = NextFree(next);
		}
	}
	if (prev) {
		//__H_assert(NextSlot(prev) <= p); NOTE: Removed
		if (NextSlot(prev) == p) { /* merge prev and p */
			NextSlot(prev) = NextSlot(p);
			NextFree(prev) = NextFree(p);
		}
	}
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Currently Unimplemented
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

inline static int __H_printf(const char * format, ...) {
    __asm__ __volatile__
        ("sll $0, $0, $0"); // does nothing right now
    return 0;
}

//------------------------------------------------------------------------------
#endif /* __RTS_H__ */
