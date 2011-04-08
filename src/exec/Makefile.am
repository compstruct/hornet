AM_CPPFLAGS = -I$(top_srcdir)/src/power \
	      -I$(top_srcdir)/src/net \
	      -I$(top_srcdir)/src/pe \
				-I$(top_srcdir)/src/cpu \
				-I$(top_srcdir)/src/sys \
	      $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarexec.la

libdarexec_la_SOURCES = messages.hpp messages.cpp \
                        memory.hpp memory.cpp  \
                        cache.hpp cache.cpp \
                        dramController.hpp dramController.cpp \
                        core.hpp core.cpp \
                        memtraceCore.hpp memtraceCore.cpp \
                        memtraceThread.hpp memtraceThread.cpp \
                        p1S2MSI.hpp p1S2MSI.cpp 

