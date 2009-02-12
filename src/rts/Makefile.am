LD=@MIPSLD@
AS=@MIPSAS@
CC=@MIPSCC@
AR=@MIPSAR@
RANLIB=@MIPSRANLIB@
CFLAGS=-fno-PIC -march=mips32r2 -mno-mips16 -static -mno-shared -mno-abicalls -nostdlib
LDFLAGS=-nostdlib -static -fno-PIC

rtsdir=$(libdir)/rts
rtsincludedir=$(rtsdir)/include
rtslibdir=$(rtsdir)/lib

bin_SCRIPTS = dar-rts-config
rtslib_LIBRARIES = librts.a
rtsinclude_HEADERS = rts.h

librts_a_SOURCES = rts.h rts.c

EXTRA_DIST = dar-rts-config.in

cfg_subst = sed -e 's,[@]VERSION[@],$(PACKAGE_VERSION),g' \
                -e 's,[@]RELEASE[@],$(PACKAGE_RELEASE_NAME),g' \
                -e 's,[@]RTSLIB[@],rts,g' \
                -e 's,[@]RTSLIBDIR[@],$(rtslibdir),g' \
                -e 's,[@]RTSINCDIR[@],$(rtsincludedir),g'

dar-rts-config: dar-rts-config.in Makefile
	$(cfg_subst) < $(srcdir)/dar-rts-config.in > dar-rts-config

