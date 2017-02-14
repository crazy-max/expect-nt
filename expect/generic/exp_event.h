/* exp_event.h - event definitions */

extern int		exp_get_next_event _ANSI_ARGS_((Tcl_Interp *,
			    struct exp_f **, int, struct exp_f **, int, int));
extern int		exp_get_next_event_info _ANSI_ARGS_((Tcl_Interp *,
			    struct exp_f *, int));
extern int		exp_dsleep _ANSI_ARGS_((Tcl_Interp *, double));
extern void		exp_init_event _ANSI_ARGS_((void));
extern void		(*exp_event_exit) _ANSI_ARGS_((Tcl_Interp *));
extern void		exp_event_disarm _ANSI_ARGS_((struct exp_f *));
extern void		exp_arm_background_filehandler _ANSI_ARGS_((
			    struct exp_f *));
extern void		exp_disarm_background_filehandler _ANSI_ARGS_((
			    struct exp_f *));
extern void		exp_disarm_background_filehandler_force _ANSI_ARGS_((
			    struct exp_f *));
extern void		exp_unblock_background_filehandler _ANSI_ARGS_((
			    struct exp_f *));
extern void		exp_block_background_filehandler _ANSI_ARGS_((
			    struct exp_f *));
extern void		exp_background_filehandler _ANSI_ARGS_((ClientData,
			    int));

