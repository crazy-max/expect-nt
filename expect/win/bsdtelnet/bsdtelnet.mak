# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

!IF "$(CFG)" == ""
CFG=bsdtelnet - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to bsdtelnet - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "bsdtelnet - Win32 Release" && "$(CFG)" !=\
 "bsdtelnet - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "bsdtelnet.mak" CFG="bsdtelnet - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "bsdtelnet - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "bsdtelnet - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "bsdtelnet - Win32 Debug"
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\bsdtelnet.exe"

CLEAN : 
	-@erase "$(INTDIR)\bsdgetopt.obj"
	-@erase "$(INTDIR)\commands.obj"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\genget.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\netlink.obj"
	-@erase "$(INTDIR)\network.obj"
	-@erase "$(INTDIR)\ring.obj"
	-@erase "$(INTDIR)\sys_bsd.obj"
	-@erase "$(INTDIR)\telnet.obj"
	-@erase "$(INTDIR)\terminal.obj"
	-@erase "$(INTDIR)\utilities.obj"
	-@erase "$(INTDIR)\wsa_strerror.obj"
	-@erase "$(OUTDIR)\bsdtelnet.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /W3 /GX /O2 /I "." /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "__WIN32__" /YX /c
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D\
 "__WIN32__" /Fp"$(INTDIR)/bsdtelnet.pch" /YX /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/bsdtelnet.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib wsock32.lib /nologo /subsystem:console /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib\
 wsock32.lib /nologo /subsystem:console /incremental:no\
 /pdb:"$(OUTDIR)/bsdtelnet.pdb" /machine:I386 /out:"$(OUTDIR)/bsdtelnet.exe" 
LINK32_OBJS= \
	"$(INTDIR)\bsdgetopt.obj" \
	"$(INTDIR)\commands.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\genget.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\netlink.obj" \
	"$(INTDIR)\network.obj" \
	"$(INTDIR)\ring.obj" \
	"$(INTDIR)\sys_bsd.obj" \
	"$(INTDIR)\telnet.obj" \
	"$(INTDIR)\terminal.obj" \
	"$(INTDIR)\utilities.obj" \
	"$(INTDIR)\wsa_strerror.obj"

"$(OUTDIR)\bsdtelnet.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\bsdtelnet.exe"

CLEAN : 
	-@erase "$(INTDIR)\bsdgetopt.obj"
	-@erase "$(INTDIR)\commands.obj"
	-@erase "$(INTDIR)\environ.obj"
	-@erase "$(INTDIR)\genget.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\netlink.obj"
	-@erase "$(INTDIR)\network.obj"
	-@erase "$(INTDIR)\ring.obj"
	-@erase "$(INTDIR)\sys_bsd.obj"
	-@erase "$(INTDIR)\telnet.obj"
	-@erase "$(INTDIR)\terminal.obj"
	-@erase "$(INTDIR)\utilities.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(INTDIR)\wsa_strerror.obj"
	-@erase "$(OUTDIR)\bsdtelnet.exe"
	-@erase "$(OUTDIR)\bsdtelnet.ilk"
	-@erase "$(OUTDIR)\bsdtelnet.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /YX /c
# ADD CPP /nologo /W3 /Gm /GX /Zi /Od /I "." /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "__WIN32__" /YX /c
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /Zi /Od /I "." /D "_DEBUG" /D "WIN32" /D\
 "_CONSOLE" /D "__WIN32__" /Fp"$(INTDIR)/bsdtelnet.pch" /YX /Fo"$(INTDIR)/"\
 /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/bsdtelnet.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib\
 wsock32.lib /nologo /subsystem:console /incremental:yes\
 /pdb:"$(OUTDIR)/bsdtelnet.pdb" /debug /machine:I386\
 /out:"$(OUTDIR)/bsdtelnet.exe" 
LINK32_OBJS= \
	"$(INTDIR)\bsdgetopt.obj" \
	"$(INTDIR)\commands.obj" \
	"$(INTDIR)\environ.obj" \
	"$(INTDIR)\genget.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\netlink.obj" \
	"$(INTDIR)\network.obj" \
	"$(INTDIR)\ring.obj" \
	"$(INTDIR)\sys_bsd.obj" \
	"$(INTDIR)\telnet.obj" \
	"$(INTDIR)\terminal.obj" \
	"$(INTDIR)\utilities.obj" \
	"$(INTDIR)\wsa_strerror.obj"

"$(OUTDIR)\bsdtelnet.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "bsdtelnet - Win32 Release"
# Name "bsdtelnet - Win32 Debug"

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=.\commands.cpp

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

DEP_CPP_COMMA=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\environ.h"\
	".\externs.h"\
	".\genget.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ptrarray.h"\
	".\ring.h"\
	".\types.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\commands.obj" : $(SOURCE) $(DEP_CPP_COMMA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

DEP_CPP_COMMA=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\environ.h"\
	".\externs.h"\
	".\genget.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ptrarray.h"\
	".\ring.h"\
	".\termios.h"\
	".\types.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\commands.obj" : $(SOURCE) $(DEP_CPP_COMMA) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\environ.cpp

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

DEP_CPP_ENVIR=\
	".\arpa/telnet.h"\
	".\array.h"\
	".\defines.h"\
	".\environ.h"\
	".\externs.h"\
	".\ring.h"\
	

"$(INTDIR)\environ.obj" : $(SOURCE) $(DEP_CPP_ENVIR) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

DEP_CPP_ENVIR=\
	".\arpa/telnet.h"\
	".\array.h"\
	".\defines.h"\
	".\environ.h"\
	".\externs.h"\
	".\ring.h"\
	".\termios.h"\
	

"$(INTDIR)\environ.obj" : $(SOURCE) $(DEP_CPP_ENVIR) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\genget.cpp
DEP_CPP_GENGE=\
	".\genget.h"\
	

"$(INTDIR)\genget.obj" : $(SOURCE) $(DEP_CPP_GENGE) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\main.cpp

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

DEP_CPP_MAIN_=\
	".\defines.h"\
	".\externs.h"\
	".\getopt.h"\
	".\proto.h"\
	".\ring.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

DEP_CPP_MAIN_=\
	".\defines.h"\
	".\externs.h"\
	".\getopt.h"\
	".\proto.h"\
	".\ring.h"\
	".\termios.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\main.obj" : $(SOURCE) $(DEP_CPP_MAIN_) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\netlink.cpp

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

DEP_CPP_NETLI=\
	".\externs.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ring.h"\
	

"$(INTDIR)\netlink.obj" : $(SOURCE) $(DEP_CPP_NETLI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

DEP_CPP_NETLI=\
	".\externs.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ring.h"\
	".\termios.h"\
	

"$(INTDIR)\netlink.obj" : $(SOURCE) $(DEP_CPP_NETLI) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\network.cpp
DEP_CPP_NETWO=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\externs.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ring.h"\
	".\termios.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\network.obj" : $(SOURCE) $(DEP_CPP_NETWO) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\ring.cpp
DEP_CPP_RING_=\
	".\ring.h"\
	

"$(INTDIR)\ring.obj" : $(SOURCE) $(DEP_CPP_RING_) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\sys_bsd.cpp

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

DEP_CPP_SYS_B=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\externs.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ring.h"\
	".\terminal.h"\
	".\types.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\sys_bsd.obj" : $(SOURCE) $(DEP_CPP_SYS_B) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

DEP_CPP_SYS_B=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\externs.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ring.h"\
	".\terminal.h"\
	".\termios.h"\
	".\types.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\sys_bsd.obj" : $(SOURCE) $(DEP_CPP_SYS_B) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\telnet.cpp

!IF  "$(CFG)" == "bsdtelnet - Win32 Release"

DEP_CPP_TELNE=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\environ.h"\
	".\externs.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ptrarray.h"\
	".\ring.h"\
	".\terminal.h"\
	".\types.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\telnet.obj" : $(SOURCE) $(DEP_CPP_TELNE) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "bsdtelnet - Win32 Debug"

DEP_CPP_TELNE=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\environ.h"\
	".\externs.h"\
	".\netlink.h"\
	".\proto.h"\
	".\ptrarray.h"\
	".\ring.h"\
	".\terminal.h"\
	".\termios.h"\
	".\types.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\telnet.obj" : $(SOURCE) $(DEP_CPP_TELNE) "$(INTDIR)"


!ENDIF 

# End Source File
################################################################################
# Begin Source File

SOURCE=.\terminal.cpp
DEP_CPP_TERMI=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\externs.h"\
	".\proto.h"\
	".\ring.h"\
	".\terminal.h"\
	".\termios.h"\
	".\types.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\terminal.obj" : $(SOURCE) $(DEP_CPP_TERMI) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\utilities.cpp
DEP_CPP_UTILI=\
	".\arpa/telnet.h"\
	".\defines.h"\
	".\externs.h"\
	".\proto.h"\
	".\ring.h"\
	".\terminal.h"\
	".\termios.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\utilities.obj" : $(SOURCE) $(DEP_CPP_UTILI) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\bsdgetopt.cpp
DEP_CPP_BSDGE=\
	".\getopt.h"\
	

"$(INTDIR)\bsdgetopt.obj" : $(SOURCE) $(DEP_CPP_BSDGE) "$(INTDIR)"


# End Source File
################################################################################
# Begin Source File

SOURCE=.\wsa_strerror.cpp

"$(INTDIR)\wsa_strerror.obj" : $(SOURCE) "$(INTDIR)"


# End Source File
# End Target
# End Project
################################################################################
