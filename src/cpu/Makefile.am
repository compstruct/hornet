CPPFLAGS += $(BOOST_CPPFLAGS) -I$(top_srcdir)/src/net
LDFLAGS += $(BOOST_LDFLAGS)

noinst_LTLIBRARIES = libdarmips.la

libdarmips_la_SOURCES = \
    reg.hpp reg.cpp \
    syscalls.hpp \
    instr.hpp instr.cpp \
    mem.hpp mem.cpp \
    cpu.hpp cpu.cpp \
    instr_format.hpp \
    instr.tab

nodist_libdarmips_la_SOURCES = instr_encoding.hpp instr_encoding.cpp

BUILT_SOURCES = instr_encoding.hpp instr_encoding.cpp
CLEANFILES = instr_encoding.hpp instr_encoding.cpp

EXTRA_DIST = instr2code

instr_encoding.cpp: instr.tab instr2code
	./instr2code instr.tab --cpp-file=$@

instr_encoding.hpp: instr.tab instr2code
	./instr2code instr.tab --hpp-file=$@

