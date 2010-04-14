AM_CPPFLAGS = $(BOOST_CPPFLAGS) \
              -I$(top_srcdir)/src/net \
              -I$(top_srcdir)/src/pe \
              -I$(top_srcdir)/src/cpu \
              -I$(top_srcdir)/src/inject \
              -I$(top_srcdir)/src/sys
AM_LDFLAGS = $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB) $(BOOST_THREAD_LIB) \
             @LTLIBOBJS@
AM_CXXFLAGS = -Wall -Werror

bin_PROGRAMS = darsim

darsim_SOURCES = \
    sim.hpp sim.cpp \
    darsim.cpp 
darsim_LDADD = \
    $(top_builddir)/src/sys/libdarsys.la \
    $(top_builddir)/src/inject/libdarinject.la \
    $(top_builddir)/src/cpu/libdarmips.la \
    $(top_builddir)/src/pe/libdarpe.la \
    $(top_builddir)/src/net/libdarnet.la \
    $(top_builddir)/src/common/libdarcommon.la
