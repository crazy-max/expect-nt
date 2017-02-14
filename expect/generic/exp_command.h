/*
 * exp_command.h --
 *
 *	Definitions for expect commands
 *
 * Written by: Don Libes, NIST, 2/6/90
 *
 * Design and implementation of this program was paid for by U.S. tax
 * dollars.  Therefore it is public domain.  However, the author and NIST
 * would appreciate credit if this program or parts of it are used.
 */

#ifndef _EXP_COMMAND_H
#define _EXP_COMMAND_H

EXTERN struct exp_f *	exp_update_master _ANSI_ARGS_((Tcl_Interp *,int,int));
EXTERN char *		exp_get_var _ANSI_ARGS_((Tcl_Interp *,char *));

EXTERN int exp_default_match_max;
EXTERN int exp_default_parity;
EXTERN int exp_default_rm_nulls;

EXTERN int		exp_one_arg_braced _ANSI_ARGS_((char *));
EXTERN int		exp_eval_with_one_arg _ANSI_ARGS_((ClientData,
				Tcl_Interp *,char **));
EXTERN void		exp_lowmemcpy _ANSI_ARGS_((char *,char *,int));

EXTERN int exp_flageq_code _ANSI_ARGS_((char *,char *,int));

#define exp_flageq(flag,string,minlen) \
(((string)[0] == (flag)[0]) && (exp_flageq_code(((flag)+1),((string)+1),((minlen)-1))))

/* exp_flageq for single char flags */
#define exp_flageq1(flag,string) \
	((string[0] == flag) && (string[1] == '\0'))

/*
 * The type of the status returned by wait varies from UNIX system
 * to UNIX system.  The macro below defines it:
 * (stolen from tclUnix.h)
 */

#define WAIT_STATUS_TYPE int
#if 0
#ifdef AIX
#   define WAIT_STATUS_TYPE pid_t
#else
#ifndef NO_UNION_WAIT
#   define WAIT_STATUS_TYPE union wait
#else
#   define WAIT_STATUS_TYPE int
#endif
#endif /* AIX */

#endif /* 0 */

/* These macros are suggested by the autoconf documentation. */

#undef WIFEXITED
#ifndef WIFEXITED
#   define WIFEXITED(stat)  (((stat) & 0xff) == 0)
#endif

#undef WEXITSTATUS
#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat) (((stat) >> 8) & 0xff)
#endif

#undef WIFSIGNALED
#ifndef WIFSIGNALED
#   define WIFSIGNALED(stat) ((stat) && ((stat) == ((stat) & 0x00ff)))
#endif

#undef WTERMSIG
#ifndef WTERMSIG
#   define WTERMSIG(stat)    ((stat) & 0x7f)
#endif

#undef WIFSTOPPED
#ifndef WIFSTOPPED
#   define WIFSTOPPED(stat)  (((stat) & 0xff) == 0177)
#endif

#undef WSTOPSIG
#ifndef WSTOPSIG
#   define WSTOPSIG(stat)    (((stat) >> 8) & 0xff)
#endif



#define EXP_SPAWN_ID_VARNAME		"spawn_id"
#define EXP_SPAWN_OUT			"spawn_out"

#define EXP_SPAWN_ID_ANY_VARNAME	"any_spawn_id"
#define EXP_SPAWN_ID_ANY		"exp_any"

#define EXP_SPAWN_ID_ERROR		"stderr"
#define EXP_SPAWN_ID_USER		"exp_user"

#define EXP_NOPID	0	/* Used when there is no associated pid to */
				/* wait for.  For example: */
				/* 1) When fd opened by someone else, e.g., */
				/* Tcl's open */
				/* 2) When entry not in use */
				/* 3) To tell user pid of "spawn -open" */
				/* 4) stdin, out, error */

#define EXP_NOFD	-1

/* these are occasionally useful to distinguish between various expect */
/* commands and are also used as array indices into the per-fd eg[] arrays */
#define EXP_CMD_BEFORE	0
#define EXP_CMD_AFTER	1
#define EXP_CMD_BG	2
#define EXP_CMD_FG	3

/* each process is associated with a 'struct exp_f'.  An array of these */
/* ('exp_fs') keeps track of all processes.  They are indexed by the true fd */
/* to the master side of the pty */
struct exp_f {
	char *spawnId;	/* Spawn identifier name */
	Tcl_HashEntry *hashPtr;	/* The hash entry with this structure */
	Tcl_Interp *interp;
	int pid;	/* pid or EXP_NOPID if no pid */
	Tcl_Pid tclPid;	/* The pid that tcl wants */
	char *buffer;	/* input buffer */
	char *lower;	/* input buffer in lowercase */
	int size;	/* current size of data */
	int msize;	/* size of buffer (true size is one greater
			 * for trailing null) */
	int umsize;	/* user view of size of buffer */
	int rm_nulls;	/* if nulls should be stripped before pat matching */
	int valid;	/* if any of the other fields should be believed */
	int user_closed;/* if user has issued "close" command or close has */
			/* occurred implicitly */
	int user_waited;/* if user has issued "wait" command */
	int sys_waited;	/* if wait() (or variant) has been called */
	WAIT_STATUS_TYPE wait;	/* raw status from wait() */
	int parity;	/* strip parity if false */
	int printed;	/* # of characters written to stdout (if logging on) */
			/* but not actually returned via a match yet */
	int echoed;	/* additional # of chars (beyond "printed" above) */
			/* echoed back but not actually returned via a match */
			/* yet.  This supports interact -echo */
	int key;	/* unique id that identifies what command instance */
			/* last touched this buffer */
	int force_read;	/* force read to occur (even if buffer already has */
			/* data).  This supports interact CAN_MATCH */
	int fg_armed;	/* If Tk_CreateFileHandler is active for responding */
			/* to foreground events */
#ifdef _WIN32
	OVERLAPPED over;	/* Overlapped result */
#endif
	Tcl_Channel channel;	/* Tcl channel */
	Tcl_Channel Master;	/* corresponds to master fd */
	/*
	 *  explicit fds aren't necessary now, but since the code is already
	 *  here from before Tcl required TclFile, we'll continue using
	 *  the old fds.  If we ever port this code to a non-UNIX system,
	 *  we'll dump the fds totally.
	 */
	   
	int slave_fd;	/* slave fd if "spawn -pty" used */
#ifdef HAVE_PTYTRAP
	char *slave_name;/* Full name of slave, i.e., /dev/ttyp0 */
#endif /* HAVE_PTYTRAP */
	int leaveopen;	/* If we should not call Tcl's close when we close -
			 * only relevant if Tcl does the original open.  It
			 * also serves as a ref count to how many times this
			 * channel has been opened with spawn -leaveopen */
	int alwaysopen;	/* Set if this is identifier that should always exist */
	Tcl_Interp *bg_interp;	/* interp to process the bg cases */
	int bg_ecount;		/* number of background ecases */
	enum {
		blocked,	/* blocked because we are processing the */
				/* file handler */
		armed,		/* normal state when bg handler in use */
		unarmed,	/* no bg handler in use */
		disarm_req_while_blocked	/* while blocked, a request */
				/* was received to disarm it.  Rather than */
				/* processing the request immediately, defer */
				/* it so that when we later try to unblock */
				/* we will see at that time that it should */
				/* instead be disarmed */
	} bg_status;

	int matched;		/* Chars matched.  Used by expectlib */
	Tcl_ChannelProc *event_proc; /* Currently installed channel handler */
	ClientData event_data; /* Argument that was installed */
};

#define EXP_TEMPORARY	1	/* expect */
#define EXP_PERMANENT	2	/* expect_after, expect_before, expect_bg */

#define EXP_DIRECT	1
#define EXP_INDIRECT	2

/*
 * Table of struct exp_f
 */
EXTERN Tcl_HashTable *exp_f_table;

EXTERN struct exp_f *exp_f_any;

EXTERN struct exp_f *	exp_chan2f _ANSI_ARGS_((Tcl_Interp *,char *,int,int,char *));
EXTERN int		exp_fcheck _ANSI_ARGS_((Tcl_Interp *, struct exp_f *,
			    int,int,char *));
EXTERN void		exp_adjust _ANSI_ARGS_((struct exp_f *));
EXTERN void		exp_buffer_shuffle _ANSI_ARGS_((Tcl_Interp *,struct exp_f *,int,char *,char *));
EXTERN int		exp_close_fd _ANSI_ARGS_((Tcl_Interp *,int));
EXTERN int		exp_close _ANSI_ARGS_((Tcl_Interp *,struct exp_f *));
EXTERN void		exp_close_all _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_ecmd_remove_f_direct_and_indirect 
				_ANSI_ARGS_((Tcl_Interp *,struct exp_f *));
EXTERN void		exp_trap_on _ANSI_ARGS_((int));
EXTERN int		exp_trap_off _ANSI_ARGS_((char *));

EXTERN void		exp_strftime();

#define exp_deleteProc ((Tcl_CmdDeleteProc *) NULL)

EXTERN int expect_key;
EXTERN int exp_configure_count;	/* # of times descriptors have been closed */
				/* or indirect lists have been changed */
EXTERN int exp_nostack_dump;	/* TRUE if user has requested unrolling of */
				/* stack with no trace */

EXTERN void		exp_init_pty _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_pty_exit _ANSI_ARGS_((void));
EXTERN void		exp_init_tty _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_init_stdio _ANSI_ARGS_((void));
/*EXTERN void		exp_init_expect _ANSI_ARGS_((Tcl_Interp *));*/
EXTERN void		exp_init_spawn_ids _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_init_spawn_id_vars _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_init_trap _ANSI_ARGS_((void));
EXTERN void		exp_init_unit_random _ANSI_ARGS_((void));
EXTERN void		exp_init_sig _ANSI_ARGS_((void));

EXTERN int		exp_tcl2_returnvalue _ANSI_ARGS_((int));
EXTERN int		exp_2tcl_returnvalue _ANSI_ARGS_((int));

EXTERN void		exp_rearm_sigchld _ANSI_ARGS_((Tcl_Interp *));
EXTERN int		exp_string_to_signal _ANSI_ARGS_((Tcl_Interp *,char *));

EXTERN char *exp_onexit_action;

#define exp_new(x)	(x *)malloc(sizeof(x))

struct exp_fs_list {
	struct exp_f *f;
	struct exp_fs_list *next;
};

/* describes a -i flag */
struct exp_i {
	int cmdtype;	/* EXP_CMD_XXX.  When an indirect update is */
			/* triggered by Tcl, this helps tell us in what */
			/* exp_i list to look in. */
	int direct;	/* if EXP_DIRECT, then the spawn ids have been given */
			/* literally, else indirectly through a variable */
	int duration;	/* if EXP_PERMANENT, char ptrs here had to be */
			/* malloc'd because Tcl command line went away - */
			/* i.e., in expect_before/after */
	char *variable;
	char *value;	/* if type == direct, this is the string that the */
			/* user originally supplied to the -i flag.  It may */
			/* lose relevance as the fs_list is manipulated */
			/* over time.  If type == direct, this is  the */
			/* cached value of variable use this to tell if it */
			/* has changed or not, and ergo whether it's */
			/* necessary to reparse. */

	int ecount;	/* # of ecases this is used by */

	struct exp_fs_list *fs_list;
	struct exp_i *next;
};

EXTERN struct exp_i *	exp_new_i_complex _ANSI_ARGS_((Tcl_Interp *,
			    char *, int, Tcl_VarTraceProc *, char *));
EXTERN struct exp_i *	exp_new_i_simple _ANSI_ARGS_((struct exp_f *,int));
EXTERN struct exp_fs_list *exp_new_fs _ANSI_ARGS_((struct exp_f *));
EXTERN void		exp_free_i _ANSI_ARGS_((Tcl_Interp *,struct exp_i *,
			    Tcl_VarTraceProc *));
EXTERN void		exp_free_fs _ANSI_ARGS_((struct exp_fs_list *));
EXTERN void		exp_free_fs_single _ANSI_ARGS_((struct exp_fs_list *));
EXTERN void		exp_i_update _ANSI_ARGS_((Tcl_Interp *,
			    struct exp_i *));

/*
 * definitions for creating commands
 */

#define EXP_NOPREFIX	1	/* don't define with "exp_" prefix */
#define EXP_REDEFINE	2	/* stomp on old commands with same name */

struct exp_cmd_data {
	char		*name;
	Tcl_CmdProc	*proc;
	ClientData	data;
	int 		flags;
};

EXTERN int		ExpPlatformSpawnOutput _ANSI_ARGS_((
			    ClientData instanceData, char *bufPtr,
			    int toWrite, int *errorPtr));
EXTERN void		exp_create_commands _ANSI_ARGS_((Tcl_Interp *,
			    struct exp_cmd_data *));
EXTERN void		exp_init_main_cmds _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_init_expect_cmds _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_init_most_cmds _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_init_trap_cmds _ANSI_ARGS_((Tcl_Interp *));
EXTERN void		exp_init_interact_cmds _ANSI_ARGS_((Tcl_Interp *));
EXTERN int		exp_init_tty_cmds _ANSI_ARGS_((Tcl_Interp *));
EXTERN int		exp_getpidproc _ANSI_ARGS_((void));
EXTERN void		exp_busy _ANSI_ARGS_((int));
EXTERN int		exp_exact_write _ANSI_ARGS_((struct exp_f *,
			    char *, int));
EXTERN void		exp_sys_close _ANSI_ARGS_((int, struct exp_f *));
EXTERN struct exp_f *	exp_f_find _ANSI_ARGS_ ((Tcl_Interp *, char *));
EXTERN struct exp_f *	exp_f_new _ANSI_ARGS_((Tcl_Interp *, Tcl_Channel,
			    char *, int));
EXTERN int		exp_f_new_platform _ANSI_ARGS_((struct exp_f *));
EXTERN void		exp_f_free _ANSI_ARGS_((struct exp_f *));
EXTERN void		exp_f_free_platform _ANSI_ARGS_((struct exp_f *));

EXTERN Tcl_Channel	ExpCreatePairChannel _ANSI_ARGS_((Tcl_Interp *,
			    char *, char *, char *chanName));
EXTERN int		ExpSpawnOpen _ANSI_ARGS_((Tcl_Interp *, char *, int));

EXTERN int		Exp_CloseCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_DebugCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_DisconnectCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExitCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExpContinueCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExpContinueDeprecatedCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExpInternalCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExpPidCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExpectCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExpectGlobalCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ExpVersionCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ForkCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_GetpidDeprecatedCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_InterReturnCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_InterpreterCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_KillCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_LogFileCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_LogUserCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_MatchMaxCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_OpenCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_OverlayCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_ParityCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_Prompt1Cmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_Prompt2Cmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_RemoveNullsCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SendCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SendCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SendCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SendCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SendCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SendLogCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SleepCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SpawnCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_StraceCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SttyCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_SystemCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_TimestampCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_TrapCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
EXTERN int		Exp_WaitCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));

#endif /* _EXP_COMMAND_H */
