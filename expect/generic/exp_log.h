/* exp_log.h */

#include "exp_printify.h"

/* special version of log for non-null-terminated strings which */
/* never need printf-style formatting. */
#define logn(buf,length)  { \
			  if (logfile) fwrite(buf,1,length,logfile); \
			  if (debugfile) fwrite(buf,1,length,debugfile); \
			  }

#define dprintify(x)	((is_debugging || debugfile)?exp_printify(x):0)
/* in circumstances where "debuglog(printify(...))" is written, call */
/* dprintify instead.  This will avoid doing any formatting that would */
/* occur before debuglog got control and decided not to do anything */
/* because (is_debugging || debugfile) was false. */

extern void exp_errorlog _ANSI_ARGS_(TCL_VARARGS(char *,fmt));
extern void exp_log _ANSI_ARGS_(TCL_VARARGS(int,force_stdout));
extern void exp_debuglog _ANSI_ARGS_(TCL_VARARGS(char *,fmt));
extern void exp_nflog _ANSI_ARGS_((char *buf, int force_stdout));
extern void exp_nferrorlog _ANSI_ARGS_((char *buf, int force_stdout));

#if defined(__WIN32__) && defined(__EXPECTLIBUSER__)
#define DLLIMP __declspec(dllimport)
#else
#define DLLIMP
#endif

EXTERN DLLIMP Tcl_Channel exp_debugfile;
EXTERN DLLIMP Tcl_Channel exp_logfile;
EXTERN DLLIMP int exp_logfile_all;
EXTERN DLLIMP int exp_loguser;
EXTERN DLLIMP int exp_is_debugging; /* useful to know for avoid debug calls */

#undef DLLIMP
