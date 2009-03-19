CPPFLAGS += $(BOOST_CPPFLAGS) \
           -I$(top_srcdir)/src/net \
           -I$(top_srcdir)/src/cpu \
           -I$(top_srcdir)/src/sys
LDFLAGS += $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB) @LTLIBOBJS@

AM_CXXFLAGS = -Wall -Werror

bin_PROGRAMS = darsim

darsim_SOURCES = darsim.cpp 
darsim_LDADD = \
    $(top_builddir)/src/sys/libdarsys.la \
    $(top_builddir)/src/net/libdarnet.la \
    $(top_builddir)/src/cpu/libdarmips.la \
    $(top_builddir)/src/common/libdarcommon.la

