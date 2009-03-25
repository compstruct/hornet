AM_CPPFLAGS = $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS)
AM_CXXFLAGS = -Wall -Wno-uninitialized -Werror

noinst_LTLIBRARIES = libdarnet.la

libdarnet_la_SOURCES = \
    flow_id.hpp flow_id.cpp \
    flit.hpp flit.cpp \
    node_id.hpp node_id.cpp \
    virtual_queue_id.hpp virtual_queue_id.cpp \
    router.hpp router.cpp \
    channel_alloc.hpp channel_alloc.cpp \
    pressure_tracker.hpp pressure_tracker.cpp \
    virtual_queue.hpp virtual_queue.cpp \
    ingress.hpp ingress.cpp \
    egress.hpp egress.cpp \
    crossbar.hpp crossbar.cpp \
    node.hpp node.cpp \
    dma.hpp dma.cpp \
    bridge.hpp bridge.cpp \
    arbiter.hpp arbiter.cpp \
    static_router.hpp static_router.cpp \
    static_channel_alloc.hpp static_channel_alloc.cpp \
    dynamic_channel_alloc.hpp dynamic_channel_alloc.cpp \
    statistics.hpp statistics.cpp

