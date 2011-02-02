AM_CPPFLAGS = -I$(top_srcdir)/src/power \
	      -I$(top_srcdir)/src/net \
	      -I$(top_srcdir)/src/pe \
				-I$(top_srcdir)/src/cpu \
				-I$(top_srcdir)/src/sys \
	      $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarexec.la

libdarexec_la_SOURCES = \
    message.hpp \
    memoryRequest.hpp memoryRequest.cpp \
    memory.hpp memory.cpp \
    cache.hpp cache.cpp \
    remoteMemory.hpp remoteMemory.cpp \
    dramController.hpp dramController.cpp \
    core.hpp core.cpp \
    homeCache.hpp homeCache.cpp \
    awayCache.hpp awayCache.cpp \
    coreMessageQueue.hpp coreMessageQueue.cpp \
		mcpu.hpp mcpu.cpp \
		memtraceCore.hpp memtraceCore.cpp \
    memtraceThread.hpp memtraceThread.cpp \
    memtraceThreadPool.hpp memtraceThreadPool.cpp 
