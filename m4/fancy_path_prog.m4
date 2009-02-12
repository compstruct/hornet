dnl FANCY_PATH_PROG([variable], [description], [command])
dnl
dnl like AC_PATH_PROG, except dies if command cannot be run
dnl also allows [variable] to be set via --with-[command]=SOMEPATH

AC_DEFUN([FANCY_PATH_PROG],[
    AC_ARG_WITH($4,[  --with-$4=EXEC	run $2 as EXEC],[[$1]="$withval"])
    if test -z "$[$1]"; then
        AC_PATH_PROG($1,$3)
    else
        AC_MSG_NOTICE([using user-provided $4: $[$1]])
    fi
    if test -z "$[$1]"; then
        AC_MSG_ERROR([$2 ($3) not found; try --help])
    fi
    AC_SUBST($1)
])

