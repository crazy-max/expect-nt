dnl AC_TRY_RUN_WITH_OUTPUT(VARIABLE, PROGRAM,)
AC_DEFUN(AC_TRY_RUN_WITH_OUTPUT,
[dnl
if test "$cross_compiling" = yes; then
  ifelse([$3], ,
    [errprint(__file__:__line__: warning: [AC_TRY_RUN_WITH_OUTPUT] called without default to
 allow cross compiling
)dnl
  AC_MSG_ERROR(can not run test program while cross compiling)],
  [$3])
else
cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
ifelse(AC_LANG, CPLUSPLUS, [#ifdef __cplusplus
extern "C" void exit(int);
#endif
])dnl
[$2]
EOF
eval $ac_link
if test -s conftest && (./conftest > ./conftest.stdout; exit) 2>/dev/null; then
   $1=`cat ./conftest.stdout`
else
   $1=""
fi
fi
rm -fr conftest*])
