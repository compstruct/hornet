AM_CPPFLAGS = -I$(top_srcdir)/src/power \
	      -I$(top_srcdir)/src/net \
	      -I$(top_srcdir)/src/pe \
	      $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarexec.la

libdarexec_la_SOURCES = \
    message.hpp message.cpp \
    memoryRequest.hpp memoryRequest.cpp \
    memory.hpp memory.cpp \
    core.hpp core.cpp \
    memtraceCore.hpp memtraceCore.cpp \
    memtraceThread.hpp memtraceThread.cpp
