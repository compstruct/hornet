AM_CPPFLAGS = -I$(top_srcdir)/src/power \
	      -I$(top_srcdir)/src/net \
	      -I$(top_srcdir)/src/pe \
				-I$(top_srcdir)/src/cpu \
				-I$(top_srcdir)/src/sys \
	      $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarexec.la

libdarexec_la_SOURCES = sharedMemory.hpp \
                        memory_types.hpp memory_types.cpp \
                        messages.hpp messages.cpp \
                        memory.hpp memory.cpp  \
                        cat.hpp cat.cpp \
                        cache.hpp cache.cpp \
                        dramController.hpp dramController.cpp \
                        core.hpp core.cpp \
                        memtraceCore.hpp memtraceCore.cpp \
                        memtraceThread.hpp memtraceThread.cpp \
                        threadStats.hpp threadStats.cpp \
                        memtraceThreadStats.hpp memtraceThreadStats.cpp \
                        privateSharedMSI.hpp privateSharedMSI.cpp \
                        privateSharedPTI.hpp privateSharedPTI.cpp \
                        sharedSharedEMRA.hpp sharedSharedEMRA.cpp \
                        sharedSharedLCC.hpp sharedSharedLCC.cpp \
                        memStats.hpp memStats.cpp \
					    mcpu.hpp mcpu.cpp \
                        privateSharedMSIStats.hpp privateSharedMSIStats.cpp \
                        privateSharedPTIStats.hpp privateSharedPTIStats.cpp \
                        sharedSharedEMRAStats.hpp sharedSharedEMRAStats.cpp \
                        sharedSharedLCCStats.hpp sharedSharedLCCStats.cpp
