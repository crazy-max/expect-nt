#define STTY_INIT	"stty_init"

/* zero out the wait status field */
static void
exp_wait_zero(status)
    WAIT_STATUS_TYPE *status;
{
    int i;

    for (i=0;i<sizeof(WAIT_STATUS_TYPE);i++) {
	((char *)status)[i] = 0;
    }
}

/* prevent an fd from being allocated */
void
exp_busy(fd)
    int fd;
{
    int x = open("/dev/null",0);
    if (x != fd) {
	fcntl(x,F_DUPFD,fd);
	close(x);
    }
}


/*
 *----------------------------------------------------------------------
 *
 * exp_f_new_platform --
 *
 *	Platform specific initialization of exp_f structure
 *
 * Results:
 *	TRUE if successful, FALSE if unsuccessful.
 *
 * Side Effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
exp_f_new_platform(f)
    exp_f *f;
{
    f->tclPid = f->pid;
    return TRUE;
}

void
exp_close_on_exec(fd)
int fd;
{
    (void) fcntl(fd,F_SETFD,1);
}

/*
 *----------------------------------------------------------------------
 *
 * exp_getpidproc --
 *
 *	Return the process id for this process
 *
 * Results:
 *	A process id
 *
 *----------------------------------------------------------------------
 */

int
exp_getpidproc()
{
    return getpid();
}

/*
 *----------------------------------------------------------------------
 *
 * Exp_SpawnCmd --
 *
 *	Creates a new expect process id.  It normally does this
 *	by creating a new process, but it may choose to open a
 *	Tcl file id.
 *
 * Results:
 *	A standard Tcl result
 *
 * Side Effects:
 *
 * Notes:
 *	arguments are passed verbatim to execvp()
 *----------------------------------------------------------------------
 */

/*ARGSUSED*/
static int
Exp_SpawnCmd(clientData,interp,argc,argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int slave;
	int pid;
	char **a;
	/* tell Saber to ignore non-use of ttyfd */
	/*SUPPRESS 591*/
	int errorfd;	/* place to stash fileno(stderr) in child */
			/* while we're setting up new stderr */
	int ttyfd;
	int master;
	int write_master;	/* write fd of Tcl-opened files */
	int ttyinit = TRUE;
	int ttycopy = TRUE;
	int echo = TRUE;
	int console = FALSE;
	int pty_only = FALSE;

#ifdef FULLTRAPS
				/* Allow user to reset signals in child */
				/* The following array contains indicates */
				/* whether sig should be DFL or IGN */
				/* ERR is used to indicate no initialization */
	RETSIGTYPE (*traps[NSIG])();
#endif
	int ignore[NSIG];	/* if true, signal in child is ignored */
				/* if false, signal gets default behavior */
	int i;			/* trusty overused temporary */

	char *argv0 = argv[0];
	char *openarg = 0;
	int leaveopen = FALSE;
	FILE *readfilePtr;
	FILE *writefilePtr;
	int rc, wc;
	char *stty_init;
	int slave_write_ioctls = 1;
		/* by default, slave will be write-ioctled this many times */
	int slave_opens = 3;
		/* by default, slave will be opened this many times */
		/* first comes from initial allocation */
		/* second comes from stty */
		/* third is our own signal that stty is done */

	int sync_fds[2];
	int sync2_fds[2];
	int status_pipe[2];
	int child_errno;
	char sync_byte;

	char buf[4];		/* enough space for a string literal */
				/* representing a file descriptor */
	Tcl_DString dstring;
	Tcl_DStringInit(&dstring);

#ifdef FULLTRAPS
	init_traps(&traps);
#endif
	/* don't ignore any signals in child by default */
	for (i=1;i<NSIG;i++) {
		ignore[i] = FALSE;
	}

	argc--; argv++;

	for (;argc>0;argc--,argv++) {
		if (streq(*argv,"-nottyinit")) {
			ttyinit = FALSE;
			slave_write_ioctls--;
			slave_opens--;
		} else if (streq(*argv,"-nottycopy")) {
			ttycopy = FALSE;
		} else if (streq(*argv,"-noecho")) {
			echo = FALSE;
		} else if (streq(*argv,"-console")) {
			console = TRUE;
		} else if (streq(*argv,"-pty")) {
			pty_only = TRUE;
		} else if (streq(*argv,"-open")) {
			if (argc < 2) {
				exp_error(interp,"usage: -open file-identifier");
				return TCL_ERROR;
			}
			openarg = argv[1];
			argc--; argv++;
		} else if (streq(*argv,"-leaveopen")) {
			if (argc < 2) {
				exp_error(interp,"usage: -open file-identifier");
				return TCL_ERROR;
			}
			openarg = argv[1];
			leaveopen = TRUE;
			argc--; argv++;
		} else if (streq(*argv,"-ignore")) {
			int sig;

			if (argc < 2) {
				exp_error(interp,"usage: -ignore signal");
				return TCL_ERROR;
			}
			sig = exp_string_to_signal(interp,argv[1]);
			if (sig == -1) {
				exp_error(interp,"usage: -ignore %s: unknown signal name",argv[1]);
				return TCL_ERROR;
			}
			ignore[sig] = TRUE;
			argc--; argv++;
#ifdef FULLTRAPS
		} else if (streq(*argv,"-trap")) {
			/* argv[1] is action */
			/* argv[2] is list of signals */

			RETSIGTYPE (*sig_handler)();
			int n;		/* number of signals in list */
			char **list;	/* list of signals */

			if (argc < 3) {
				exp_error(interp,"usage: -trap siglist SIG_DFL or SIG_IGN");
				return TCL_ERROR;
			}

			if (0 == strcmp(argv[2],"SIG_DFL")) {
				sig_handler = SIG_DFL;
			} else if (0 == strcmp(argv[2],"SIG_IGN")) {
				sig_handler = SIG_IGN;
			} else {
				exp_error(interp,"usage: -trap siglist SIG_DFL or SIG_IGN");
				return TCL_ERROR;
			}

			if (TCL_OK != Tcl_SplitList(interp,argv[1],&n,&list)) {
				errorlog("%s\r\n",interp->result);
				exp_error(interp,"usage: -trap {siglist} ...");
				return TCL_ERROR;
			}
			for (i=0;i<n;i++) {
				int sig = exp_string_to_signal(interp,list[i]);
				if (sig == -1) {
					ckfree((char *)&list);
					return TCL_ERROR;
				}
				traps[sig] = sig_handler;
			}
			ckfree((char *)&list);

			argc--; argv++;
			argc--; argv++;
#endif /*FULLTRAPS*/
		} else break;
	}

	if (openarg && (argc != 0)) {
		exp_error(interp,"usage: -[leave]open [fileXX]");
		return TCL_ERROR;
	}

	if (!pty_only && !openarg && (argc == 0)) {
		exp_error(interp,"usage: spawn [spawn-args] program [program-args]");
		return(TCL_ERROR);
	}

	stty_init = exp_get_var(interp,STTY_INIT);
	if (stty_init) {
		slave_write_ioctls++;
		slave_opens++;
	}

/* any extraneous ioctl's that occur in slave must be accounted for
when trapping, see below in child half of fork */
#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hp9000s300)
	slave_write_ioctls++;
	slave_opens++;
#endif

	exp_pty_slave_name = 0;

	Tcl_ReapDetachedProcs();

	if (!openarg) {
		if (echo) {
			exp_log(0,"%s ",argv0);
			for (a = argv;*a;a++) {
				exp_log(0,"%s ",*a);
			}
			exp_nflog("\r\n",0);
		}

		if (0 > (master = getptymaster())) {
			/*
			 * failed to allocate pty, try and figure out why
			 * so we can suggest to user what to do about it.
			 */

			int count;
			int testfd;

			if (exp_pty_error) {
				exp_error(interp,"%s",exp_pty_error);
				return TCL_ERROR;
			}

			count = 0;
			for (i=3;i<=exp_fd_max;i++) {
				count += exp_fs[i].valid;
			}
			if (count > 10) {
				exp_error(interp,"The system only has a finite number of ptys and you have many of them in use.  The usual reason for this is that you forgot (or didn't know) to call \"wait\" after closing each of them.");
				return TCL_ERROR;
			}

			testfd = open("/",0);
			close(testfd);

			if (testfd != -1) {
				exp_error(interp,"The system has no more ptys.  Ask your system administrator to create more.");
			} else {
				exp_error(interp,"- You have too many files are open.  Close some files or increase your per-process descriptor limit.");
			}
			return(TCL_ERROR);
		}
#ifdef PTYTRAP_DIES
		if (!pty_only) exp_slave_control(master,1);
#endif /* PTYTRAP_DIES */

		Tcl_SetVar2(interp,EXP_SPAWN_OUT,"slave,name",exp_pty_slave_name,0);
	} else {
		Tcl_Channel chan;
		int mode;
		Tcl_File tclReadFile, tclWriteFile;
		int rfd, wfd;

		if (echo) exp_log(0,"%s [open ...]\r\n",argv0);

#if TCL7_4
		rc = Tcl_GetOpenFile(interp,openarg,0,1,&readfilePtr);
		wc = Tcl_GetOpenFile(interp,openarg,1,1,&writefilePtr);

		/* fail only if both descriptors are bad */
		if (rc == TCL_ERROR && wc == TCL_ERROR) {
			return TCL_ERROR;		
		}

		master = fileno((rc == TCL_OK)?readfilePtr:writefilePtr);

		/* make a new copy of file descriptor */
		if (-1 == (write_master = master = dup(master))) {
			exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
			return TCL_ERROR;
		}

		/* if writefilePtr is different, dup that too */
		if ((rc == TCL_OK) && (wc == TCL_OK) && (fileno(writefilePtr) != fileno(readfilePtr))) {
			if (-1 == (write_master = dup(fileno(writefilePtr)))) {
				exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
				return TCL_ERROR;
			}
			exp_close_on_exec(write_master);
		}

#endif
		if (!(chan = Tcl_GetChannel(interp,openarg,&mode))) {
			return TCL_ERROR;
		}
		if (!mode) {
			exp_error(interp,"channel is neither readable nor writable");
			return TCL_ERROR;
		}
		if (mode & TCL_READABLE) {
			tclReadFile = Tcl_GetChannelFile(chan, TCL_READABLE);
			rfd = (int)Tcl_GetFileInfo(tclReadFile, (int *)0);
		}
		if (mode & TCL_WRITABLE) {
			tclWriteFile = Tcl_GetChannelFile(chan, TCL_WRITABLE);
			wfd = (int)Tcl_GetFileInfo(tclWriteFile, (int *)0);
		}

		master = ((mode & TCL_READABLE)?rfd:wfd);

		/* make a new copy of file descriptor */
		if (-1 == (write_master = master = dup(master))) {
			exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
			return TCL_ERROR;
		}

		/* if writefilePtr is different, dup that too */
		if ((mode & TCL_READABLE) && (mode & TCL_WRITABLE) && (wfd != rfd)) {
			if (-1 == (write_master = dup(wfd))) {
				exp_error(interp,"fdopen: %s",Tcl_PosixError(interp));
				return TCL_ERROR;
			}
			exp_close_on_exec(write_master);
		}

		/*
		 * It would be convenient now to tell Tcl to close its
		 * file descriptor.  Alas, if involved in a pipeline, Tcl
		 * will be unable to complete a wait on the process.
		 * So simply remember that we meant to close it.  We will
		 * do so later in our own close routine.
		 */
	}

	/* much easier to set this, than remember all masters */
	exp_close_on_exec(master);

	if (openarg || pty_only) {
		struct exp_f *f;

		f = fd_new(master,EXP_NOPID);

		if (openarg) {
			/* save file# handle */
			f->tcl_handle = ckalloc(strlen(openarg)+1);
			strcpy(f->tcl_handle,openarg);

			f->tcl_output = write_master;
#if 0
			/* save fd handle for output */
			if (wc == TCL_OK) {
/*				f->tcl_output = fileno(writefilePtr);*/
				f->tcl_output = write_master;
			} else {
				/* if we actually try to write to it at some */
				/* time in the future, then this will cause */
				/* an error */
				f->tcl_output = master;
			}
#endif

			f->leaveopen = leaveopen;
		}

		if (exp_pty_slave_name) set_slave_name(f,exp_pty_slave_name);

		/* make it appear as if process has been waited for */
		f->sys_waited = TRUE;
		exp_wait_zero(&f->wait);

		/* tell user id of new process */
		sprintf(buf,"%d",master);
		Tcl_SetVar(interp,EXP_SPAWN_ID_VARNAME,buf,0);

		if (!openarg) {
			char value[20];
			int dummyfd1, dummyfd2;

			/*
			 * open the slave side in the same process to support
			 * the -pty flag.
			 */

			/* Start by working around a bug in Tcl's exec.
			   It closes all the file descriptors from 3 to it's
			   own fd_max which inappropriately closes our slave
			   fd.  To avoid this, open several dummy fds.  Then
			   exec's fds will fall below ours.
			   Note that if you do something like pre-allocating
			   a bunch before using them or generating a pipeline,
			   then this code won't help.
			   Instead you'll need to add the right number of
			   explicit Tcl open's of /dev/null.
			   The right solution is fix Tcl's exec so it is not
			   so cavalier.
			 */

			dummyfd1 = open("/dev/null",0);
			dummyfd2 = open("/dev/null",0);

			if (0 > (f->slave_fd = getptyslave(ttycopy,ttyinit,
					stty_init))) {
				exp_error(interp,"open(slave pty): %s\r\n",Tcl_PosixError(interp));
				return TCL_ERROR;
			}

			close(dummyfd1);
			close(dummyfd2);

			exp_slave_control(master,1);

			sprintf(value,"%d",f->slave_fd);
			Tcl_SetVar2(interp,EXP_SPAWN_OUT,"slave,fd",value,0);
		}
		sprintf(interp->result,"%d",EXP_NOPID);
		debuglog("spawn: returns {%s}\r\n",interp->result);

		return TCL_OK;
	}

	if (NULL == (argv[0] = Tcl_TildeSubst(interp,argv[0],&dstring))) {
		goto parent_error;
	}

	if (-1 == pipe(sync_fds)) {
		exp_error(interp,"too many programs spawned?  could not create pipe: %s",Tcl_PosixError(interp));
		goto parent_error;
	}

	if (-1 == pipe(sync2_fds)) {
		close(sync_fds[0]);
		close(sync_fds[1]);
		exp_error(interp,"too many programs spawned?  could not create pipe: %s",Tcl_PosixError(interp));
		goto parent_error;
	}

	if (-1 == pipe(status_pipe)) {
		close(sync_fds[0]);
		close(sync_fds[1]);
		close(sync2_fds[0]);
		close(sync2_fds[1]);
	}

	if ((pid = fork()) == -1) {
		exp_error(interp,"fork: %s",Tcl_PosixError(interp));
		goto parent_error;
	}

	if (pid) { /* parent */
		struct exp_f *f;

		close(sync_fds[1]);
		close(sync2_fds[0]);
		close(status_pipe[1]);

		f = fd_new(master,pid);

		if (exp_pty_slave_name) set_slave_name(f,exp_pty_slave_name);

#ifdef CRAY
		setptypid(pid);
#endif


#if PTYTRAP_DIES
#ifdef HAVE_PTYTRAP

		while (slave_opens) {
			int cc;
			cc = exp_wait_for_slave_open(master);
#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hp9000s300)
			if (cc == TIOCSCTTY) slave_opens = 0;
#endif
			if (cc == TIOCOPEN) slave_opens--;
			if (cc == -1) {
				exp_error(interp,"failed to trap slave pty");
				goto parent_error;
			}
		}

#if 0
		/* trap initial ioctls in a feeble attempt to not block */
		/* the initially.  If the process itself ioctls */
		/* /dev/tty, such blocks will be trapped later */
		/* during normal event processing */

		/* initial slave ioctl */
		while (slave_write_ioctls) {
			int cc;

			cc = exp_wait_for_slave_open(master);
#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hp9000s300)
			if (cc == TIOCSCTTY) slave_write_ioctls = 0;
#endif
			if (cc & IOC_IN) slave_write_ioctls--;
			else if (cc == -1) {
				exp_error(interp,"failed to trap slave pty");
				goto parent_error;
			}
		}
#endif /*0*/

#endif /* HAVE_PTYTRAP */
#endif /* PTYTRAP_DIES */

		/*
		 * wait for slave to initialize pty before allowing
		 * user to send to it
		 */ 

		debuglog("parent: waiting for sync byte\r\n");
		while (((rc = read(sync_fds[0],&sync_byte,1)) < 0) && (errno == EINTR)) {
			/* empty */;
		}
		if (rc == -1) {
			errorlog("parent sync byte read: %s\r\n",Tcl_ErrnoMsg(errno));
			exit(-1);
		}

		/* turn on detection of eof */
		exp_slave_control(master,1);

		/*
		 * tell slave to go on now now that we have initialized pty
		 */

		debuglog("parent: telling child to go ahead\r\n");
		wc = write(sync2_fds[1]," ",1);
		if (wc == -1) {
			errorlog("parent sync byte write: %s\r\n",Tcl_ErrnoMsg(errno));
			exit(-1);
		}

		debuglog("parent: now unsynchronized from child\r\n");
		close(sync_fds[0]);
		close(sync2_fds[1]);

		/* see if child's exec worked */
	retry:
		switch (read(status_pipe[0],&child_errno,sizeof child_errno)) {
		case -1:
			if (errno == EINTR) goto retry;
			/* well it's not really the child's errno */
			/* but it can be treated that way */
			child_errno = errno;
			break;
		case 0:
			/* child's exec succeeded */
			child_errno = 0;
			break;
		default:
			/* child's exec failed; err contains exec's errno  */
			waitpid(pid, NULL, 0);
			/* in order to get Tcl to set errorcode, we must */
			/* hand set errno */
			errno = child_errno;
			exp_error(interp, "couldn't execute \"%s\": %s",
				argv[0],Tcl_PosixError(interp));
			goto parent_error;
		}
		close(status_pipe[0]);


		/* tell user id of new process */
		sprintf(buf,"%d",master);
		Tcl_SetVar(interp,EXP_SPAWN_ID_VARNAME,buf,0);

		sprintf(interp->result,"%d",pid);
		debuglog("spawn: returns {%s}\r\n",interp->result);

		Tcl_DStringFree(&dstring);
		return(TCL_OK);
parent_error:
		Tcl_DStringFree(&dstring);
		return TCL_ERROR;
	}

	/* child process - do not return from here!  all errors must exit() */

	close(sync_fds[0]);
	close(sync2_fds[1]);
	close(status_pipe[0]);
	exp_close_on_exec(status_pipe[1]);

	if (exp_dev_tty != -1) {
		close(exp_dev_tty);
		exp_dev_tty = -1;
	}

#ifdef CRAY
	(void) close(master);
#endif

/* ultrix (at least 4.1-2) fails to obtain controlling tty if setsid */
/* is called.  setpgrp works though.  */
#if defined(POSIX) && !defined(ultrix)
#define DO_SETSID
#endif
#ifdef __convex__
#define DO_SETSID
#endif

#ifdef DO_SETSID
	setsid();
#else
#ifdef SYSV3
#ifndef CRAY
	setpgrp();
#endif /* CRAY */
#else /* !SYSV3 */
#ifdef MIPS_BSD
	/* required on BSD side of MIPS OS <jmsellen@watdragon.waterloo.edu> */
#	include <sysv/sys.s>
	syscall(SYS_setpgrp);
#endif
	setpgrp(0,0);
/*	setpgrp(0,getpid());*/	/* make a new pgrp leader */

/* Pyramid lacks this defn */
#ifdef TIOCNOTTY
	ttyfd = open("/dev/tty", O_RDWR);
	if (ttyfd >= 0) {
		(void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
		(void) close(ttyfd);
	}
#endif /* TIOCNOTTY */

#endif /* SYSV3 */
#endif /* DO_SETSID */

	/* save stderr elsewhere to avoid BSD4.4 bogosity that warns */
	/* if stty finds dev(stderr) != dev(stdout) */

#ifndef WIN32_XXX
	/* save error fd while we're setting up new one */
	errorfd = fcntl(2,F_DUPFD,3);
#endif
	/* and here is the macro to restore it */
#ifndef WIN32_XXX
#define restore_error_fd {close(2);fcntl(errorfd,F_DUPFD,2);}
#else
#define restore_error_fd
#endif

	close(0);
	close(1);
	close(2);

	/* since we closed fd 0, open of pty slave must return fd 0 */

	/* since getptyslave may have to run stty, (some of which work on fd */
	/* 0 and some of which work on 1) do the dup's inside getptyslave. */

	if (0 > (slave = getptyslave(ttycopy,ttyinit,stty_init))) {
		restore_error_fd
		errorlog("open(slave pty): %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}
	/* sanity check */
	if (slave != 0) {
		restore_error_fd
		errorlog("getptyslave: slave = %d but expected 0\n",slave);
		exit(-1);
	}

/* The test for hpux may have to be more specific.  In particular, the */
/* code should be skipped on the hp9000s300 and hp9000s720 (but there */
/* is no documented define for the 720!) */

/*#if defined(TIOCSCTTY) && !defined(CIBAUD) && !defined(sun) && !defined(hpux)*/
#if defined(TIOCSCTTY) && !defined(sun) && !defined(hpux)
	/* 4.3+BSD way to acquire controlling terminal */
	/* according to Stevens - Adv. Prog..., p 642 */
	/* Oops, it appears that the CIBAUD is on Linux also */
	/* so let's try without... */
#ifdef __QNX__
	if (tcsetct(0, getpid()) == -1) {
#else
	if (ioctl(0,TIOCSCTTY,(char *)0) < 0) {
#endif
		restore_error_fd
		errorlog("failed to get controlling terminal using TIOCSCTTY");
		exit(-1);
	}
#endif

#ifdef CRAY
 	(void) setsid();
 	(void) ioctl(0,TCSETCTTY,0);
 	(void) close(0);
 	if (open("/dev/tty", O_RDWR) < 0) {
		restore_error_fd
 		errorlog("open(/dev/tty): %s\r\n",Tcl_ErrnoMsg(errno));
 		exit(-1);
 	}
 	(void) close(1);
 	(void) close(2);
 	(void) dup(0);
 	(void) dup(0);
	setptyutmp();	/* create a utmp entry */

	/* _CRAY2 code from Hal Peterson <hrp@cray.com>, Cray Research, Inc. */
#ifdef _CRAY2
	/*
	 * Interpose a process between expect and the spawned child to
	 * keep the slave side of the pty open to allow time for expect
	 * to read the last output.  This is a workaround for an apparent
	 * bug in the Unicos pty driver on Cray-2's under Unicos 6.0 (at
	 * least).
	 */
	if ((pid = fork()) == -1) {
		restore_error_fd
		errorlog("second fork: %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}

	if (pid) {
 		/* Intermediate process. */
		int status;
		int timeout;
		char *t;

		/* How long should we wait? */
		if (t = exp_get_var(interp,"pty_timeout"))
			timeout = atoi(t);
		else if (t = exp_get_var(interp,"timeout"))
			timeout = atoi(t)/2;
		else
			timeout = 5;

		/* Let the spawned process run to completion. */
 		while (wait(&status) < 0 && errno == EINTR)
			/* empty body */;

		/* Wait for the pty to clear. */
		sleep(timeout);

		/* Duplicate the spawned process's status. */
		if (WIFSIGNALED(status))
			kill(getpid(), WTERMSIG(status));

		/* The kill may not have worked, but this will. */
 		exit(WEXITSTATUS(status));
	}
#endif /* _CRAY2 */
#endif /* CRAY */

	if (console) exp_console_set();

#ifdef FULLTRAPS
	for (i=1;i<NSIG;i++) {
		if (traps[i] != SIG_ERR) {
			signal(i,traps[i]);
		}
	}
#endif /* FULLTRAPS */

	for (i=1;i<NSIG;i++) {
		signal(i,ignore[i]?SIG_IGN:SIG_DFL);
	}

#if 0
	/* avoid fflush of cmdfile since this screws up the parents seek ptr */
	/* There is no portable way to fclose a shared read-stream!!!! */
	if (exp_cmdfile && (exp_cmdfile != stdin))
		(void) close(fileno(exp_cmdfile));
	if (logfile) (void) fclose(logfile);
	if (debugfile) (void) fclose(debugfile);
#endif
	/* (possibly multiple) masters are closed automatically due to */
	/* earlier fcntl(,,CLOSE_ON_EXEC); */

	/* tell parent that we are done setting up pty */
	/* The actual char sent back is irrelevant. */

	/* debuglog("child: telling parent that pty is initialized\r\n");*/
	wc = write(sync_fds[1]," ",1);
	if (wc == -1) {
		restore_error_fd
		errorlog("child: sync byte write: %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}
	close(sync_fds[1]);

	/* wait for master to let us go on */
	/* debuglog("child: waiting for go ahead from parent\r\n"); */

/*	close(master);	/* force master-side close so we can read */

	while (((rc = read(sync2_fds[0],&sync_byte,1)) < 0) && (errno == EINTR)) {
		/* empty */;
	}

	if (rc == -1) {
		restore_error_fd
		errorlog("child: sync byte read: %s\r\n",Tcl_ErrnoMsg(errno));
		exit(-1);
	}
	close(sync2_fds[0]);

	/* debuglog("child: now unsynchronized from parent\r\n"); */

	/* So much for close-on-exec.  Tcl doesn't mark its files that way */
	/* everything has to be closed explicitly. */
	if (exp_close_in_child) (*exp_close_in_child)();

        (void) execvp(argv[0],argv);
#if 0
	/* Unfortunately, by now we've closed fd's to stderr, logfile and
		debugfile.
	   The only reasonable thing to do is to send back the error as
	   part of the program output.  This will be picked up in an
	   expect or interact command.
	*/
	errorlog("%s: %s\r\n",argv[0],Tcl_ErrnoMsg(errno));
#endif
	/* if exec failed, communicate the reason back to the parent */
	write(status_pipe[1], &errno, sizeof errno);
	exit(-1);
	/*NOTREACHED*/
}


/* Describe the processes created with Expect's fork.
This allows us to wait on them later.

This is maintained as a linked list.  As additional procs are forked,
new links are added.  As procs disappear, links are marked so that we
can reuse them later.
*/

struct forked_proc {
	int pid;
	WAIT_STATUS_TYPE wait_status;
	enum {not_in_use, wait_done, wait_not_done} link_status;
	struct forked_proc *next;
} *forked_proc_base = 0;

void
fork_clear_all()
{
	struct forked_proc *f;

	for (f=forked_proc_base;f;f=f->next) {
		f->link_status = not_in_use;
	}
}

void
fork_init(f,pid)
struct forked_proc *f;
int pid;
{
	f->pid = pid;
	f->link_status = wait_not_done;
}

/* make an entry for a new proc */
void
fork_add(pid)
int pid;
{
	struct forked_proc *f;

	for (f=forked_proc_base;f;f=f->next) {
		if (f->link_status == not_in_use) break;
	}

	/* add new entry to the front of the list */
	if (!f) {
		f = (struct forked_proc *)ckalloc(sizeof(struct forked_proc));
		f->next = forked_proc_base;
		forked_proc_base = f;
	}
	fork_init(f,pid);
}

/* Provide a last-chance guess for this if not defined already */
#ifndef WNOHANG
#define WNOHANG WNOHANG_BACKUP_VALUE
#endif

/* wait returns are a hodgepodge of things
 If wait fails, something seriously has gone wrong, for example:
   bogus arguments (i.e., incorrect, bogus spawn id)
   no children to wait on
   async event failed
 If wait succeeeds, something happened on a particular pid
   3rd arg is 0 if successfully reaped (if signal, additional fields supplied)
   3rd arg is -1 if unsuccessfully reaped (additional fields supplied)
*/

/*ARGSUSED*/
static int
Exp_ForkCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int rc;
	if (argc > 1) {
		exp_error(interp,"usage: fork");
		return(TCL_ERROR);
	}

	rc = fork();
	if (rc == -1) {
		exp_error(interp,"fork: %s",Tcl_PosixError(interp));
		return TCL_ERROR;
	} else if (rc == 0) {
		/* child */
		exp_forked = TRUE;
		exp_getpid = exp_getpidproc();
		fork_clear_all();
	} else {
		/* parent */
		fork_add(rc);
	}

	/* both child and parent follow remainder of code */
	sprintf(interp->result,"%d",rc);
	debuglog("fork: returns {%s}\r\n",interp->result);
	return(TCL_OK);
}

/*ARGSUSED*/
static int
Exp_DisconnectCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	/* tell Saber to ignore non-use of ttyfd */
	/*SUPPRESS 591*/
	int ttyfd;

	if (argc > 1) {
		exp_error(interp,"usage: disconnect");
		return(TCL_ERROR);
	}

	if (exp_disconnected) {
		exp_error(interp,"already disconnected");
		return(TCL_ERROR);
	}
	if (!exp_forked) {
		exp_error(interp,"can only disconnect child process");
		return(TCL_ERROR);
	}
	exp_disconnected = TRUE;

	/* ignore hangup signals generated by testing ptys in getptymaster */
	/* and other places */
	signal(SIGHUP,SIG_IGN);

	/* reopen prevents confusion between send/expect_user */
	/* accidentally mapping to a real spawned process after a disconnect */
	if (exp_fs[0].pid != EXP_NOPID) {
		exp_close(interp,0);
		open("/dev/null",0);
		exp_f_new(0, EXP_NOPID);
	}
	if (exp_fs[1].pid != EXP_NOPID) {
		exp_close(interp,1);
		open("/dev/null",1);
		exp_f_new(1, EXP_NOPID);
	}
	if (exp_fs[2].pid != EXP_NOPID) {
		/* reopen stderr saves error checking in error/log routines. */
		exp_close(interp,2);
		open("/dev/null",1);
		exp_f_new(2, EXP_NOPID);
	}

	Tcl_UnsetVar(interp,"tty_spawn_id",TCL_GLOBAL_ONLY);

#ifdef DO_SETSID
	setsid();
#else
#ifdef SYSV3
	/* put process in our own pgrp, and lose controlling terminal */
#ifdef sysV88
	/* With setpgrp first, child ends up with closed stdio */
	/* according to Dave Schmitt <daves@techmpc.csg.gss.mot.com> */
	if (fork()) exit(0);
	setpgrp();
#else
	setpgrp();
	/*signal(SIGHUP,SIG_IGN); moved out to above */
	if (fork()) exit(0);	/* first child exits (as per Stevens, */
	/* UNIX Network Programming, p. 79-80) */
	/* second child process continues as daemon */
#endif
#else /* !SYSV3 */
#ifdef MIPS_BSD
	/* required on BSD side of MIPS OS <jmsellen@watdragon.waterloo.edu> */
#	include <sysv/sys.s>
	syscall(SYS_setpgrp);
#endif
	setpgrp(0,0);
/*	setpgrp(0,getpid());*/	/* put process in our own pgrp */

/* Pyramid lacks this defn */
#ifdef TIOCNOTTY
	ttyfd = open("/dev/tty", O_RDWR);
	if (ttyfd >= 0) {
		/* zap controlling terminal if we had one */
		(void) ioctl(ttyfd, TIOCNOTTY, (char *)0);
		(void) close(ttyfd);
	}
#endif /* TIOCNOTTY */

#endif /* SYSV3 */
#endif /* DO_SETSID */
	return(TCL_OK);
}

/*ARGSUSED*/
static int
Exp_OverlayCmd(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int newfd, oldfd;
	int dash_name = 0;
	char *command;

	argc--; argv++;
	while (argc) {
		if (*argv[0] != '-') break;	/* not a flag */
		if (streq(*argv,"-")) {		/* - by itself */
			argc--; argv++;
			dash_name = 1;
			continue;
		}
		newfd = atoi(argv[0]+1);
		argc--; argv++;
		if (argc == 0) {
			exp_error(interp,"overlay -# requires additional argument");
			return(TCL_ERROR);
		}
		oldfd = atoi(argv[0]);
		argc--; argv++;
		debuglog("overlay: mapping fd %d to %d\r\n",oldfd,newfd);
		if (oldfd != newfd) (void) dup2(oldfd,newfd);
		else debuglog("warning: overlay: old fd == new fd (%d)\r\n",oldfd);
	}
	if (argc == 0) {
		exp_error(interp,"need program name");
		return(TCL_ERROR);
	}
	command = argv[0];
	if (dash_name) {
		argv[0] = ckalloc(1+strlen(command));
		sprintf(argv[0],"-%s",command);
	}

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
        (void) execvp(command,argv);
	exp_error(interp,"execvp(%s): %s\r\n",argv[0],Tcl_PosixError(interp));
	return(TCL_ERROR);
}
