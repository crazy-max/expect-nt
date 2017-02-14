# --------------------------------------------------------------------------
#
# watch.tcl --
#
#	A simple command tracing utility.  Uses the BLT "watch" command
#	to set up a command trace on all Tcl commands at or above a given
#	execution stack level.  If level is 0, then tracing is disabled.
#	To try out this demo, start "bltwish" and "source" this file;
#	then enable tracing by issuing the command "debug 100" and enter
#	arbitrary Tcl/Tk/BLT commands.
#
# --------------------------------------------------------------------------

############################################################################
#
# This procedure is called before a Tcl command is going to be executed.
#
############################################################################

proc bltBeforeWatch { level cmdStr argList } {
    set cmd [lindex $argList 0]
    puts stderr "$level $cmd => $cmdStr"
}

############################################################################
#
# This procedure is called right after a Tcl command has been executed.
#
############################################################################

proc bltAfterWatch { level cmdStr argList code results } {
    set cmd [lindex $argList 0]
    puts stderr "$level $cmd => $argList\n<= ($code) $results"
}

############################################################################
#
# This "wrapper" procedure enables or disables traces.
#
############################################################################

proc debug { level } {
    if { $level > 0 } {
	watch create DebugWatch \
	    -precmd bltBeforeWatch -postcmd bltAfterWatch -maxlevel $level 
    } else {
	catch {watch delete DebugWatch}
    }
}

############################################################################
#
# If BLT was compiled with namespace support, then we'll import the
# "watch" command, but only if this hasn't been done already.
#
############################################################################

if {    [info exists blt_useTclNamespaces] && $blt_useTclNamespaces
     && "[info commands ::blt::watch]" != "" } {
    rename ::blt::watch ::watch
}

### EOF ####################################################################
