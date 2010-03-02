AC_PREREQ(2.61)
AC_INIT([dar], [0.05])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([src/common/dconfig.h])
AM_INIT_AUTOMAKE([foreign])
PACKAGE_RELEASE_NAME="miyakowasure"
AC_DEFINE_UNQUOTED(PACKAGE_RELEASE_NAME, "$PACKAGE_RELEASE_NAME", [Release])
AC_SUBST([PACKAGE_RELEASE_NAME])
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_CXX
AC_C_BIGENDIAN
AX_BOOST_BASE([1.34])
AX_BOOST_DATE_TIME
AX_BOOST_THREAD
AX_BOOST_PROGRAM_OPTIONS
AC_CHECK_LIB([m],[ceil])
AM_PATH_PYTHON([2.5])
FANCY_PATH_PROG([QMTEST], [QMTest test infrastructure], [qmtest])
AC_ARG_ENABLE([mips-rts],
              AC_HELP_STRING([--enable-mips-rts],
                             [build MIPS runtime system [[default=no]]]),
              [case "${enableval}" in
               yes) build_mips_rts=true ;;
               on) build_mips_rts=true ;;
               true) build_mips_rts=true ;;
               no) build_mips_rts=false ;;
               false) build_mips_rts=false ;;
               off) build_mips_rts=false ;;
               *) AC_MSG_ERROR([bad arg to --enable-mips-rts: ${enableval}]) ;;
               esac], [build_mips_rts=false])
AM_CONDITIONAL([BUILD_MIPS_RTS], [test x$build_mips_rts = xtrue])
if test x$build_mips_rts = xtrue; then
    FANCY_PATH_PROG([MIPSLD], [MIPS linker], [mips-linux-gnu-ld], [mips-ld])
    FANCY_PATH_PROG([MIPSAS], [MIPS assembler], [mips-linux-gnu-as], [mips-as])
    FANCY_PATH_PROG([MIPSCC], [MIPS C compiler],
                    [mips-linux-gnu-gcc], [mips-cc])
    FANCY_PATH_PROG([MIPSAR], [MIPS ar], [mips-linux-gnu-ar], [mips-ar])
    FANCY_PATH_PROG([MIPSRANLIB], [MIPS ranlib],
                    [mips-linux-gnu-ranlib], [mips-ranlib])
fi
AC_OUTPUT(\
    Makefile \
    src/Makefile \
    src/common/Makefile \
    src/net/Makefile \
    src/pe/Makefile \
    src/cpu/Makefile \
    src/inject/Makefile \
    src/sys/Makefile \
    src/sim/Makefile \
    src/tools/Makefile \
    src/rts/Makefile \
    tests/Makefile \
    )

