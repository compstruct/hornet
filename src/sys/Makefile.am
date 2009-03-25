AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
              -I$(top_srcdir)/src/net \
              -I$(top_srcdir)/src/pe \
              -I$(top_srcdir)/src/cpu \
              -I$(top_srcdir)/src/inject
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarsys.la

libdarsys_la_SOURCES = \
    sys.hpp sys.cpp \
    event_parser.hpp event_parser.cpp

