# Visual C++ 2.x and 4.0 makefile
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# Copyright (c) 1995-1996 Sun Microsystems, Inc.
# SCCS: @(#) makefile.vc 1.67 97/01/21 11:23:05

# Does not depend on the presence of any environment variables in
# order to compile expect; all needed information is derived from 
# location of the compiler directories.

#
# Project directories
#
# ROOT    = top of source tree
#
# TMPDIR  = location where .obj files should be stored during build
#
# TOOLS32 = location of VC++ 32-bit development tools. Note that the
#	    VC++ 2.0 header files are broken, so you need to use the
#	    ones that come with the developer network CD's, or later
#	    versions of VC++.
#

ROOT		= ..
TMPDIR		= .

!IFNDEF TOOLS32
TOOLS32		= c:\msdev
!ENDIF

!IFNDEF TCL_ROOT_DIR
TCL_ROOT_DIR	= ..\..\tcl8.0
!ENDIF

!IFNDEF TCL_GENERIC_DIR
TCL_GENERIC_DIR	= $(TCL_ROOT_DIR)\generic
!ENDIF

DEBUG_INFO=1
!IFNDEF DEBUG_INFO
NODEBUG=1
!ENDIF

# Set this to the appropriate value of /MACHINE: for your platform
MACHINE	= IX86

# uncomment one of the following lines to compile with TCL_MEM_DEBUG, 
# TCL_MEM_DEBUG, or TCL_COMPILE_DEBUG
#DEBUGDEFINES	= -DTCL_MEM_DEBUG
#DEBUGDEFINES	= -DTCL_MEM_DEBUG -DTCL_COMPILE_DEBUG
#DEBUGDEFINES	= -DTCL_MEM_DEBUG -DTCL_COMPILE_STATS
#DEBUGDEFINES	= -DTCL_MEM_DEBUG -DTCL_COMPILE_DEBUG -DTCL_COMPILE_STATS

######################################################################
# Do not modify below this line
######################################################################

EXPECTLIB	= expect52.lib
EXPECTDLL	= expect52.dll
EXPECT		= expect.exe
EXPECTTEST	= expecttest.exe
SLAVEDRV	= slavedrv.exe
DUMPEXTS	= dumpexts.exe
TELNET		= telnet.exe
TESTCAT		= testcat.exe
TESTSIG		= testsig.exe
TESTCALC	= testcalc.exe
TESTCONSOUT	= testconsout.exe
TESTCLIB	= testclib.exe
TESTA1		= testa1.exe
TESTA2		= testa2.exe
TESTMODEM	= testmodem.exe
TESTWSTATION	= testwstation.exe
TESTWPROG	= testwprog.exe
DEBUGGER	= debugger.exe
LIBEXPECT	= expectlib52.lib
LIBEXPECTDLL	= expectlib52.dll

TEST_APPS = $(TESTCALC) $(TESTCONSOUT) $(TESTSIG) $(TESTCLIB) \
	$(TESTA2) $(TESTMODEM)
TEST_OTHERS = $(TESTWSTATION) $(TESTWPROG)

EXPECTOBJS = \
	$(TMPDIR)\exp_main_exp.obj

EXP_DLL_OBJS = \
	$(TMPDIR)\expCommand.obj \
	$(TMPDIR)\expWinCommand.obj \
	$(TMPDIR)\expWinLog.obj \
	$(TMPDIR)\expWinProcess.obj \
	$(TMPDIR)\expWinTty.obj \
	$(TMPDIR)\exp_log.obj \
	$(TMPDIR)\exp_glob.obj \
	$(TMPDIR)\Dbg.obj \
	$(TMPDIR)\exp_closetcl.obj \
	$(TMPDIR)\exp_regexp.obj \
	$(TMPDIR)\exp_printify.obj \
	$(TMPDIR)\getopt.obj \
	$(TMPDIR)\exp_main_sub.obj \
	$(TMPDIR)\exp_inter.obj \
	$(TMPDIR)\expect.obj \
	$(TMPDIR)\exp_event.obj \
	$(TMPDIR)\exp_strf.obj \
	$(TMPDIR)\expSpawnChan.obj \
	$(TMPDIR)\expWinSpawnChan.obj \
	$(TMPDIR)\expChan.obj \
	$(TMPDIR)\expTrap.obj

LIBEXPECT_OBJS = \
	$(TMPDIR)\exp_clib.obj

#	$(TMPDIR)\exp_noevent.obj \
#	$(TMPDIR)\expect.obj \
#	$(TMPDIR)\$(PTY).obj \
#	$(TMPDIR)\exp_inter.obj \
#	$(TMPDIR)\exp_tty.obj \
#	$(TMPDIR)\exp_pty.obj \
#	$(TMPDIR)\exp_trap.obj \
#	$(TMPDIR)\exp_console.obj \
#	$(TMPDIR)\exp_win.obj \
#	$(TMPDIR)\exp_clib.obj \
#	$(TMPDIR)\exp_memmove.obj \
#	$(TMPDIR)\exp_tty_comm.obj \
#	$(TMPDIR)\exp_$(EVENT_TYPE).obj \
#	$(TMPDIR)\exp_$(EVENT_ABLE).obj

SLAVE_OBJS = \
	$(TMPDIR)\expWinSlaveDrv.obj \
	$(TMPDIR)\expWinSlaveKey.obj \
	$(TMPDIR)\expWinSlaveDbg.obj \
	$(TMPDIR)\expWinSlaveDbgW.obj \
	$(TMPDIR)\expWinProcess.obj \
	$(TMPDIR)\expWinLog.obj \
	$(TMPDIR)\expDString.obj \
	$(TMPDIR)\expAlloc.obj \
	$(TMPDIR)\tclHash.obj \
	$(TMPDIR)\panic.obj

TELNET_OBJS = \
	$(TELNETDIR)\commands.obj \
	$(TELNETDIR)\environ.obj \
	$(TELNETDIR)\genget.obj \
	$(TELNETDIR)\bsdgetopt.obj \
	$(TELNETDIR)\main.obj \
	$(TELNETDIR)\netlink.obj \
	$(TELNETDIR)\network.obj \
	$(TELNETDIR)\ring.obj \
	$(TELNETDIR)\sys_bsd.obj \
	$(TELNETDIR)\telnet.obj \
	$(TELNETDIR)\terminal.obj \
	$(TELNETDIR)\utilities.obj \
	$(TELNETDIR)\wsa_strerror.obj

DEBUGGER_OBJS = \
	$(TMPDIR)\debugger.obj \
	$(TMPDIR)\expWinProcess.obj \
	$(TMPDIR)\expWinLog.obj \
	$(TMPDIR)\expDString.obj \
	$(TMPDIR)\expAlloc.obj \

cc32		= $(TOOLS32)\bin\cl.exe
link32		= $(TOOLS32)\bin\link.exe
rc32		= $(TOOLS32)\bin\rc.exe
include32	= -I$(TOOLS32)\include

WINDIR          = $(ROOT)\win
GENERICDIR	= $(ROOT)\generic

TCL_INCLUDES	= -I$(TCL_ROOT_DIR)\win -I$(TCL_ROOT_DIR)\generic
TCL_LIBS	= $(TCL_ROOT_DIR)\win\tcl80.lib
EXPECT_INCLUDES	= -I$(WINDIR) -I$(GENERICDIR) $(TCL_INCLUDES)
EXPECT_DEFINES	= -D__WIN32__ -DUSE_TCLALLOC=1 $(DEBUGDEFINES)

EXPECT_CFLAGS	= $(cdebug) $(cflags) $(cvarsdll) $(include32) \
			$(EXPECT_INCLUDES) $(EXPECT_DEFINES) 

TELNETDIR	= $(ROOT)\win\bsdtelnet
TELNET_INCLUDES = -I$(TELNETDIR)
TELNET_DEFINES	= -D__WIN32__ $(DEBUGDEFINES)
TELNET_CFLAGS	= $(cdebug) $(cflags) $(cvars) $(include32) \
		  	$(TELNET_INCLUDES) $(TELNET_DEFINES)

CON_CFLAGS	= $(cdebug) $(cflags) $(cvars) $(include32) -DCONSOLE
DOS_CFLAGS	= $(cdebug) $(cflags) $(include16) -AL 
DLL16_CFLAGS	= $(cdebug) $(cflags) $(include16) -ALw

######################################################################
# Link flags
######################################################################

!IFDEF NODEBUG
ldebug = /RELEASE
!ELSE
ldebug = -debug:full -debugtype:cv
!ENDIF

# declarations common to all linker options
lcommon = /NODEFAULTLIB /RELEASE /NOLOGO

# declarations for use on Intel i386, i486, and Pentium systems
!IF "$(MACHINE)" == "IX86"
DLLENTRY = @12
lflags   = $(lcommon) -align:0x1000 /MACHINE:$(MACHINE)
!ELSE
lflags   = $(lcommon) /MACHINE:$(MACHINE)
!ENDIF

conlflags = $(lflags) -subsystem:console -entry:mainCRTStartup
guilflags = $(lflags) -subsystem:windows -entry:WinMainCRTStartup
dlllflags = $(lflags) -entry:_DllMainCRTStartup$(DLLENTRY) -dll \
	-base:0x12000000

!IF "$(MACHINE)" == "PPC"
libc = libc.lib
libcdll = crtdll.lib
!ELSE
libc = libc.lib oldnames.lib
!IFDEF NODEBUG
libcdll = msvcrt.lib oldnames.lib
!ELSE
#libcdll = msvcrtd.lib oldnames.lib
libcdll = msvcrt.lib oldnames.lib
!ENDIF
!ENDIF

baselibs   = kernel32.lib $(optlibs) advapi32.lib
winlibs    = $(baselibs) user32.lib gdi32.lib comdlg32.lib winspool.lib

guilibs	   = $(libc) $(winlibs)
conlibs	   = $(libc) $(baselibs)
guilibsdll = $(libcdll) $(winlibs)
conlibsdll = $(libcdll) $(baselibs)

######################################################################
# Compile flags
######################################################################

!IFDEF NODEBUG
cdebug = -Ox
!ELSE
cdebug = -Z7 -Od -WX
!ENDIF

# declarations common to all compiler options
ccommon = -c -W3 -nologo -YX -Dtry=__try -Dexcept=__except

!IF "$(MACHINE)" == "IX86"
cflags = $(ccommon) -D_X86_=1
!ELSE
!IF "$(MACHINE)" == "MIPS"
cflags = $(ccommon) -D_MIPS_=1
!ELSE
!IF "$(MACHINE)" == "PPC"
cflags = $(ccommon) -D_PPC_=1
!ELSE
!IF "$(MACHINE)" == "ALPHA"
cflags = $(ccommon) -D_ALPHA_=1
!ENDIF
!ENDIF
!ENDIF
!ENDIF

cvars      = -DWIN32 -D_WIN32
cvarsmt    = $(cvars) -D_MT
cvarsdll   = $(cvarsmt) -D_DLL

######################################################################
# Project specific targets
######################################################################

all:	release $(TEST_APPS)
release: $(DUMPEXTS) $(TELNET) $(SLAVEDRV) $(DEBUGGER) \
	$(EXPECTDLL) $(LIBEXPECTDLL) $(EXPECT)
test:	all $(EXPECTTEST)
	$(EXPECTTEST) <<
		cd ../tests
		source all
<<

$(DUMPEXTS): $(WINDIR)\winDumpExts.c
	$(cc32) $(CON_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(guilibs) -out:$@ \
		$(TMPDIR)\winDumpExts.obj 

$(DEBUGGER): $(DEBUGGER_OBJS)
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) \
		$(conlibsdll) $(guilibsdll) -out:$@ $(DEBUGGER_OBJS)

$(TELNET): $(TELNET_OBJS)
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(guilibs) wsock32.lib -out:$@ \
		$(TELNET_OBJS)

specialconlflags = $(lflags) -subsystem:console -entry:specialCRTStartup
$(TESTCAT): $(WINDIR)\testcat.c
	$(cc32) $(CON_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(specialconlflags) $(guilibs) -out:$@ \
		$(TMPDIR)\testcat.obj 

$(TESTSIG): $(WINDIR)\testsig.c
	$(cc32) $(CON_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(conlibsdll) -out:$@ \
		$(TMPDIR)\testsig.obj 

$(TESTCALC): $(WINDIR)\testcalc.c
	$(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(conlibsdll) -out:$@ \
		$(TMPDIR)\testcalc.obj 

specialconlflags = $(lflags) -subsystem:console -entry:specialCRTStartup
$(TESTCONSOUT): $(WINDIR)\testconsout.c
	$(cc32) $(CON_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(specialconlflags) $(guilibs) -out:$@ \
		$(TMPDIR)\testconsout.obj 

$(TESTA1): $(WINDIR)\testa1.c
	$(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(conlibsdll) -out:$@ \
		$(TMPDIR)\testa1.obj 

$(TESTA2): $(WINDIR)\testa2.c
	$(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(conlibsdll) -out:$@ \
		$(TMPDIR)\testa2.obj 

$(TESTMODEM): $(WINDIR)\testmodem.c
	$(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(conlibsdll) -out:$@ \
		$(TMPDIR)\testmodem.obj 

$(TESTWSTATION): $(WINDIR)\testwstation.c
	$(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(conlibsdll) $(guilibs) -out:$@ \
		$(TMPDIR)\testwstation.obj 

$(TESTWPROG): $(WINDIR)\testwprog.c
	$(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(guilflags) $(guilibsdll) -out:$@ \
		$(TMPDIR)\testwprog.obj 

$(TESTCLIB): $(WINDIR)\testclib.c
	$(cc32) $(CON_CFLAGS) $(EXPECT_INCLUDES) -Fo$(TMPDIR)\ $?
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(LIBEXPECT) $(guilibs) \
		-out:$@ $(TMPDIR)\testclib.obj 

$(SLAVEDRV): $(SLAVE_OBJS)
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) -out:$@ $(conlibsdll) \
		$(guilibsdll) $(SLAVE_OBJS)
#	$(link32) $(ldebug) $(conlflags) $(guilibs) -out:$@ $(SLAVE_OBJS)

$(EXPECTDLL): $(EXP_DLL_OBJS) $(TMPDIR)\expect.def $(TMPDIR)\expect.res
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(dlllflags) $(TCL_LIBS) -def:$(TMPDIR)\expect.def \
		-out:$@ $(TMPDIR)\expect.res $(guilibsdll) @<<
$(EXP_DLL_OBJS)
<<

$(LIBEXPECTDLL): $(LIBEXPECT_OBJS) $(TMPDIR)\expectlib.def $(TMPDIR)\expectlib.res
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(dlllflags) $(EXPECTLIB) $(TCL_LIBS) \
		-def:$(TMPDIR)\expectlib.def \
		-out:$@ $(TMPDIR)\expectlib.res $(guilibsdll) @<<
$(LIBEXPECT_OBJS)
<<

$(EXPECT): $(EXPECTOBJS) $(EXPECTLIB) $(TMPDIR)\expect.res
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(TMPDIR)\expect.res \
		-out:$@ $(conlibsdll) $(TCL_LIBS) $(EXPECTLIB) $(EXPECTOBJS)

$(EXPECTTEST): $(EXPECTTESTOBJS) $(EXPECTLIB) $(TMPDIR)\expect.res
	set LIB=$(TOOLS32)\lib
	$(link32) $(ldebug) $(conlflags) $(TMPDIR)\expect.res -stack:1572864 \
		 -out:$@ $(conlibsdll) $(EXPECTLIB) $(EXPECTTESTOBJS)

$(TMPDIR)\expect.def: $(DUMPEXTS) $(EXP_DLL_OBJS)
	$(DUMPEXTS) -o $@ $(EXPECTDLL) @<<
$(EXP_DLL_OBJS)
<<
	echo exp_cmdfilename >> $@
	echo exp_cmdfile >> $@
	echo exp_debugfile >> $@
	echo exp_interactive >> $@
	echo exp_logfile >> $@
	echo exp_logfile_all >> $@
	echo exp_loguser >> $@
	echo exp_is_debugging >> $@

#
# Implicit rules
#

{$(WINDIR)}.c{$(TMPDIR)}.obj:
    $(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $<

{$(GENERICDIR)}.c{$(TMPDIR)}.obj:
    $(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $<

{$(ROOT)\compat}.c{$(TMPDIR)}.obj:
    $(cc32) $(EXPECT_CFLAGS) -Fo$(TMPDIR)\ $<

{$(WINDIR)}.rc{$(TMPDIR)}.res:
	$(rc32) -fo $@ -r -i $(GENERICDIR) -i $(WINDIR) -D__WIN32__ \
		$(EXPECT_DEFINES) $<

{$(TELNETDIR)}.cpp{$(TELNETDIR)}.obj:
    $(cc32) $(TELNET_CFLAGS) -Fo$(TELNETDIR)\ $<


clean:
        -@del *.exp 
	-@del *.lib 
	-@del *.log
	-@del *.dll 
	-@del *.exe
        -@del $(TMPDIR)\*.obj
        -@del $(TMPDIR)\*.res
	-@del $(TMPDIR)\*.pch
	-@del $(TMPDIR)\*.pdb
	-@del $(TMPDIR)\*.mdp
	-@del $(TMPDIR)\*.ilk
        -@del $(TMPDIR)\expect.def
        -@del $(TELNETDIR)\*.obj
