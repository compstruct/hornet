AM_CPPFLAGS = $(BOOST_CPPFLAGS)
AM_LDFLAGS = $(BOOST_LDFLAGS) -static
AM_CXXFLAGS = -Wall -Werror

noinst_LTLIBRARIES = libdarcommon.la

libdarcommon_la_SOURCES =  \
    config.hpp \
    bits.hpp \
    endian.hpp \
    cstdint.hpp \
    random.hpp random.cpp \
    error.hpp error.cpp \
    logger.hpp logger.cpp \
    version.hpp version.cpp

