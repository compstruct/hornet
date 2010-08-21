AM_CPPFLAGS = -I$(top_srcdir)/src/power \
              -I$(top_srcdir)/src/net \
              $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS) -static
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarpe.la

libdarpe_la_SOURCES = pe.hpp pe.cpp
