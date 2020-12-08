AC_PREREQ(2.61)
AC_INIT([hornet], [1.0])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([src/common/dconfig.h])
AM_INIT_AUTOMAKE([foreign])
PACKAGE_RELEASE_NAME="suisen"
AC_DEFINE_UNQUOTED(PACKAGE_RELEASE_NAME, "$PACKAGE_RELEASE_NAME", [Release])
AC_SUBST([PACKAGE_RELEASE_NAME])
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_CXX
AC_C_BIGENDIAN
AX_BOOST_BASE([1.34])
AX_BOOST_DATE_TIME
AX_BOOST_THREAD
AX_BOOST_SYSTEM
AX_BOOST_PROGRAM_OPTIONS
AC_CHECK_LIB([m],[ceil])
AM_PATH_PYTHON([2.5])
AC_ARG_ENABLE([testsuite],
              AC_HELP_STRING([--enable-testsuite],
                             [build testsuite using QMTest [default=yes]]),
              [case "${enableval}" in
               yes) enable_testsuite=true ;;
               on) enable_testsuite=true ;;
               true) enable_testsuite=true ;;
               no) enable_testsuite=false ;;
               false) enable_testsuite=false ;;
               off) enable_testsuite=false ;;
               *) AC_MSG_ERROR([bad arg to --enable-testsuite: ${enableval}]) ;;
               esac], [enable_testsuite=true])
AM_CONDITIONAL([ENABLE_TESTSUITE], [test x$enable_testsuite = xtrue])
if test x$enable_testsuite = xtrue; then
FANCY_PATH_PROG([QMTEST], [QMTest test infrastructure], [qmtest], [qmtest])
fi
AC_ARG_ENABLE([docs],
              AC_HELP_STRING([--enable-docs],
                             [build documentation [default=no]]),
              [case "${enableval}" in
               yes) enable_docs=true ;;
               on) enable_docs=true ;;
               true) enable_docs=true ;;
               no) enable_docs=false ;;
               false) enable_docs=false ;;
               off) enable_docs=false ;;
               *) AC_MSG_ERROR([bad arg to --enable-docs: ${enableval}]) ;;
               esac], [enable_docs=false])
AM_CONDITIONAL([ENABLE_DOCS], [test x$enable_docs = xtrue])
if test x$enable_docs = xtrue; then
FANCY_PATH_PROG([DBLATEX], [DBLaTeX DocBook processor], [dblatex], [dblatex])
FANCY_PATH_PROG([XMLTO], [XMLto DocBook processor], [xmlto], [xmlto])
fi
AC_ARG_ENABLE([mips-rts],
              AC_HELP_STRING([--enable-mips-rts],
                             [build MIPS runtime system [default=no]]),
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
    src/power/Makefile \
    src/net/Makefile \
    src/pe/Makefile \
    src/cpu/Makefile \
    src/inject/Makefile \
    src/exec/Makefile \
    src/sys/Makefile \
    src/sim/Makefile \
    src/tools/Makefile \
    src/rts/Makefile \
    src/boost.task-0.4.2/Makefile \
    tests/Makefile \
    doc/Makefile \
    doc/design/Makefile \
    doc/manual/Makefile \
    )

