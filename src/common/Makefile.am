CPPFLAGS += $(BOOST_CPPFLAGS)
LDFLAGS += $(BOOST_LDFLAGS) -static

AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarcommon.la

libdarcommon_la_SOURCES =  \
    config.hpp \
    bits.hpp \
    endian.hpp \
    cstdint.hpp \
    error.hpp error.cpp \
    logger.hpp logger.cpp \
    version.hpp version.cpp

