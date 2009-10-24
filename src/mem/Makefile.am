AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
	      -I$(top_srcdir)/src/net \
	      -I$(top_srcdir)/src/pe
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Wno-uninitialized -Werror

noinst_LTLIBRARIES = libdarmem.la

libdarmem_la_SOURCES = \
    mem.hpp mem.cpp \
    main_mem.hpp main_mem.cpp

