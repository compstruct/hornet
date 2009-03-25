AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
	      -I$(top_srcdir)/src/net \
	      -I$(top_srcdir)/src/pe
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarinject.la

libdarinject_la_SOURCES = \
    injector.hpp injector.cpp
