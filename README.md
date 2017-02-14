# Expect for Windows NT

This repository is a copy of [Gordon Chaffee](https://github.com/gordonchaffee)'s Expect for Windows NT.<br />
Comes from the [Wayback Machine snapshot on 29, from Apr 1999](https://web.archive.org/web/20090429043100/http://bmrc.berkeley.edu/people/chaffee/expectnt.html).

## Introduction to Expect on Windows NT

Expect is a program that performs programmed dialogue with other interactive programs. For more information about Expect, try

* [The Expect home page (NSIT)](https://www.nist.gov/services-resources/software/expect)
* [The Expect home page (Sourceforge)](http://expect.sourceforge.net/)

This is Expect 5 for Tcl 8.0 and Tk 8.0. It works with no other versions of Tcl or Tk. This distribution comes with the code for both Tcl 8.0 and Tk 8.0. The Tcl 8.0 distribution has been modified from the original Tcl 8.0 distribution. It adds support for event driven I/O over pipes and event driven console input. This distribution gets information from a different registry key than the Sun distribution, so it can be installed safely with a Tcl 8.0 distribution as long as you use separate install directories.

The distribution comes in both source and binary forms. It comes with Tcl 8.0, Tk 8.0, and Expect 5.21.

You can also get these current snapshots distributions that have many fixes and improvements over Beta 1. In general, these have been more stable than the Beta 1 release, but the fixes may introduce new problems. The first snapshot past Beta 1 is Snapshot 16. Snapshot 19 was supposed to support Windows 95, but it does not function properly. Some of the problems were fixed, but one very significant problem remains.

This port comes with a console based telnet application that can be used with expect. This telnet application is not meant as a standalone telnet although it will work as such. It only acts a dumb terminal, however, so it is not generally useful.

## Questions and Answers

### Is this an official release of Expect sanctioned by Don Libes?

No, this is an independent port that was funded by Mitel Corporation. It has been released under essentially the same terms as Sun's Tcl distribution. Although Don Libes and I have communicated and he gave me plenty of assistance through the porting period, he has not sanctioned this port.

### Does this port run on Windows 95?

Eric Youngdale provided socket support that was required to work on Windows 95, but there is still one major problem. On NT, one can set a breakpoint in a system DLL. Shared DLLs are set copy on write (COW), so setting a breakpoint in a DLL in one process causes that process to get a private page without affecting other processes. Windows 95 doesn't do this, so it protects the system DLLs and does not allow one to set breakpoints on them. The fix requires rather significant fixes--functions can no longer be intercepted at their entry points but need to be instead intercepted at their calling points. This is not as useful and harder to get right.

### I've installed the Expect distribution, but the Expect executable does not work interactively. How can I use Expect interactively on Windows NT? How does Expect work on Windows NT?

There is an expect.exe, but it is not interactive currently. It can run scripts that are given as command line arguments. If you want to run Expect interactively, load it as an extension into the Tcl interpreter. If you run the tclsh80.exe that comes with the Expect NT distribution, it will automatically load the Expect extension. You can see this in $ExpectRoot/lib/tcl/init.tcl.

### On Unix, the first line of our script is `#!/usr/local/bin/expect -f`. This allows us to run the script directly. Is there a way to run the script without directly invoking tclsh?

There are a couple solutions to this problem. The first is to construct a .bat file that will then get rerun as a tclsh file. Here is an example:

```
::catch {};#\
@echo off
::catch {};#\
@"C:\tcl\bin\tclsh.exe" %0 %1 %2
::catch {};#\
@goto eof
puts "argv0='$argv0' argc=$argc argv='$argv' pwd='[pwd]'"
#your code here
#\
:eof
```

The second solution is to use associated file type. You can associate the .exp extension with tclsh80.exe or expect.exe. For more information about this technique, see How do I associate Perl scripts with perl?. From the command line, you can type:

```
ASSOC .exp=ExpectScript FTYPE ExpectScript=c:\program files\expect-5.21\bin\tclsh80.exe %1 %* 
```

You can also execute shell scripts directly with Tcl's exec command and Expect's spawn command. The process execution code has been patched to check files for #! at the beginning of the file.

### What commands are supported?

These are the commands that are fully working (in theory, at least):

* close: -onexec does nothing.
* exit
* exp_continue
* exp_open
* exp_pid
* exp_version
* expect
* expect_after
* expect_before
* expect_tty
* expect_user getpid
* inter_return
* log_file: All options should work, but not tested much
* log_user
* match_max
* parity
* prompt1
* prompt2
* remove_nulls
* send: -break currently does nothing
* send_error: same problem as send
* send_user: same problem as send
* send_tty: same problem as send
* sleep
* spawn: spawning a process implemented
  * -open, -leaveopen, -noecho also implemented
  * -console, -ignore, -nottycopy, -nottyinit, -pty not implemented
* stty: echo/-echo, raw/-raw supported
* trap
* wait

These commands should work, but they haven't really been tested:

* debug
* exp_internal
* expect_background
* strace

These command are not currently implemented, and I'm not currently working on them:

* interact
* interpreter
* system

These commands are not likely to ever be implemented

* disconnect: Doesn't make sense on NT
* fork: There is no concept of forking a process on Windows NT. NT can create an entirely new process, but not a process that is an identical copy of its parent.
* overlay: I could make this work, but I'm not sure I see the point.

### I'm having trouble reading output from a certain spawned process. Nothing seems to be read. Do you know why?

If you are having trouble reading from a subprocess, try using spawn -pipes instead of just spawn. Normally, Expect tries to read the output from the subprocess through function call interception. While this is the most flexible way when it is fully functional, there may still be some bugs associated with it. It should work for almost all programs, but there may be a few exceptions. If you still have trouble, try the -pipes option to spawn since it does not use function call interception and may work. The primary drawback is that for interactive processes, the spawned process needs to flush its output after every write to stdout/stderr. If you own the code to the subprocess or you do not need truly interactive performance or the subprocess already flushes its output, this option may work adequately for you. This was the technique that was used in the Alpha1 release. One benefit that it has is that it is faster than the current function call interception technique.

### I have a script that works on Unix that does a send string\n, but it does not have the same behavior on Windows NT--the \n does not get recognized by the spawned process as a carriage return. Why not?

Even though the script works on some, most, or all Unices, the script is not correct. This is according to Don Libes. It should be using send string\r instead. The reason it works on Unix is that the pseudo-terminal layer translates \n to \r. There is no guarantee that it will on all Unices, so this makes it non-portable. On Don's recommendation, \n will not be translated to \r on Windows NT, so your script will need to be changed. It seems that quite a few Dejagnu scripts are written with \n instead of \r, so those scripts will fail.

### Is there a way I can see my controlled process running?

Yes. If you set the variable exp_nt_debug to 1, it will display a console when a subprocess is spawned. This can be useful when you are trying to debug the interaction between expect and your spawned process.

### One of my spawned processes is crashing, but there does not seem to be a way of attaching a debugger to it. Why not?

Expect runs spawned processes under control of a simple debugger. The Win32 API does not allow either two debuggers to attach to the same process, and it doesn't allow a debugger to detach from a process. This means that you cannot run the Visual C++ debugger, Purify, or any other debugger on a program under the control of Expect. To aid in debugging problems that only show up when running under Expect, there is a simple traceback facility beginning in Snapshot 17. To make use of it, you need to set the variable exp_nt_debug to 1 before spawning the process. If a crash occurs, a traceback will be printed in the console where the crashed occurred, and it will wait for 10 seconds. Additionally, the traceback will be logged in the application event log. This can be viewed with the Event Viewer in the Administrative Tools group. You will need to select the application event log.

### Are there any example scripts for Expect on Windows NT?

You can find some sample scripts in the source distribution. In the expect-5.21/win/tests/ directory, there is a test suite suite. Additionally, expect-5.21/win/etest.tcl has some test procedures.