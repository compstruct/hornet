CPPFLAGS += $(BOOST_CPPFLAGS)
LDFLAGS += $(BOOST_LDFLAGS)

noinst_LTLIBRARIES = libdarnet.la

# noinst_PROGRAMS = darnettest

libdarnet_la_SOURCES = \
    flow.hpp flow.cpp \
    flit.hpp flit.cpp \
    virtual_queue.hpp virtual_queue.cpp \
    node.hpp node.cpp \
    bridge.hpp bridge.cpp

# darnettest_SOURCES = darnettest.cpp
# darnettest_LDADD = \
#     $(BOOST_LDFLAGS) $(BOOST_PROGRAM_OPTIONS_LIB) \
#     $(top_builddir)/src/common/libdarcommon.la \
#     libdarnet.la

