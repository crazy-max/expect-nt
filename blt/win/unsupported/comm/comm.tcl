#
# $Id: comm.tcl,v 1.1.1.1 1998/08/21 05:59:12 chaffee Exp $
# %%_OSF_COPYRIGHT_%%
#

package provide Comm 2.2

# USAGE:
#
#	comm works just like Tk's send, except that it uses sockets.
#	These commands work just like "send" and "winfo interps":
#
#		comm send ?-async? id cmd ?arg...?
#		comm interps
#
#	This is all you really need to know to use "comm".
#
# Semantics:
#
#	The semantics of "comm send" are intended to match Tk's send EXACTLY.
#	If you find that "comm send" doesn't work for a particular command,
#	try the same thing with Tk's send and see if the result is differnt.
#	If so, let me know.
#
#	e.g.,
#	I had one report that this:	comm send <id> llength {a b c}
#	failed with this error:		wrong # args: should be "llength list""
#	However, this does the same:	send <name> llength {a b c}
#
# Details:
#
#	comm allocates a port which it listens on for commands.
#	"comm self" returns this port.  You need another interpreter's
#	port in order to send it command.  "comm interps" merely lists
#	all the ports which you've connected to.  Unlike Tk's send,
#	comm doesn't implicitly know all the interps on the system -
#	you first need the port before you can talk to an interp.
#
#	"comm send" will automatically connect to the given port.  You
#	can force a connection to a port with "conn connect".  After that,
#	the remote port will appear in "comm interps".
#
# Basic Interface:
#
#	comm send ?-async? <otherid> <cmd> ?<arg> ...?
#	comm interps
#	comm self
#	comm connect <otherid>
#
# Multiple listeners:
#
#	You can create more than once instance of a comm interpeter in
#	each Tcl interpeter.  This allows you to have full and restricted
#	channels.  "comm new" creates a new channel with a given channel
#	name.  After that, a new command is created with that channel name.
#	This new channel command takes all the same arguments as "comm".
#
#	By default, each channel's listening port is a high-numbered id
#	(>10000).  Alternately, you can pass a value of 0 for a low-numbered
#	id (>1024) or some specific port.  In the last case, the channel
#	won't be initialized unless the specific port can be allocated.
#
#	"comm ids" lists all the channel ids allocated.  "comm init" allows
#	you to change the paramters of a channel.
#
#	comm new <ch> ?-port <port> -local 1?
#	comm ids
#	comm init ?-port <port> -local 1?
#
# Remote interpreters:
#	By default, each channel is restricted to the localhost.
#	You can override that by using "-local 0".  For such channels,
#	the <otherid> parameter takes the form "<id> <host>".
#	WARNING: You should always specify <host> in the same form.
#
# Shutdown:
#
#	These methods give control over closing connections.
#	comm shutdown <otherid>
#		This invokes a close of the connection to otherid, aborting
#		all outstanding commands in progress.  Note that nothing
#		prevents the connection from being reopened by another send.
#	comm abort
#		This invokes shutdown on all open connections.
#	comm destroy
#		This aborts all connections and then destroys the comm channel
# 		itself, including closing the listening socket.  Special
#		code allows you to close the default "comm" channel and then
#		recreate it with new ("comm destroy; comm new comm")
#
# Dead Peer Detection:
#
#	Right now, the (old-style) commLost proc is called when a remote
#	connection is lost.  It is defined as this, where "self" is the
#	name of the comm channel:
#
#		proc commLost {id reason} {upvar self self}
#
#	It is expected that applications will modify commLost as appropriate.
#	The correct way is to use the one implemented hook,
#
#		comm hook lost <cmd>
#
#	For example:
#
#		comm hook lost {
#		    global myvar
#		    if {$myvar(id) == $id} {
#		        myfunc
#			return
#		    }
#		}
#
#       (NOTE: this is NOT compatible with the callback hook described below.)
#
# Callbacks:
#
#	(UNIMPLEMENTED AS OF YET)
#	This is a mechanism for setting hooks for particular events:
#
#		comm hook <event> ?<otherid>? <cmd>
#
#	For each event, <cmd> is invoked with particular args appended:
#
#	<event>		args
#
#	connecting	ch id
#		Hook invoked before making a remote connection.
#		A return of 1 is required or else connection is not made.
#
#	connected	ch id fid
#		Hook invoked before after making a remote connection,
#		allowing arbitrary authentication over fid.
#		A return of 1 is required or else connection is closed.
#
#	incoming	ch fid addr remport
#		Hook invoked when receiving an incoming connection,
#		allowing arbitrary authentication over fid.
#		A return of 1 is required or else connection is closed.
#
#	eval		ch id cmd
#		Hook invoked after collecting a complete command.
#		A return of 0 indicates command refused (returning an
#		error'd result).  A return of 1 indicates command accepted.
#		A (to be deterimined) return will indicate the command has
#		been eval'd, handing back the return value.
#
#	lost		ch id reason
#		Hook invoked when connection to $id was lost.
#		Return value ignored.
#
# Unsupported:
#
#	comm remoteid
#		Returns the id of the last remote command eval'd on the channel
#		(must be called before any events are processed)
#
# Todo:
#	These are easily done with existing hooks:
#	- Allow use of a slave interp for actual command execution
#	  (especially when operating in "not local" mode).
#	- Maybe add cookie to initial handshake
#
#	Bugs/outstanding issues:
#	- Complete the "shutdown", "abort", and "destroy" methods.
#	- Add an interp discovery and name->port mapping.
#	- "<id> <host>" form is dependent upon canonical hostnames
#
# Windows Warning:
#
#	Tcl7.5 under Windows contains a bug that causes the interpreter to
#	hang when EOF is reached on non-blocking sockets.  This can be
#	triggered with this command:	comm send $other exit
#	Always make sure the channel is quiescent before closing/exiting.
#	Tcl7.5p1 fixes this problem.
#

###############################################################################
#
# Sample code to use for replacing "send" and "winfo interps"
#

if 0 {
#
# Remove the enclosing "if 0" if you want this to happen by default.
#
  catch {
    proc send {args} {
	eval comm send $args
    }
    rename winfo winfo_cmd
    proc winfo {cmd args} {
	if ![string match in* $cmd] {return [eval [list winfo_cmd $cmd] $args]}
	return [comm interps]
    }
  }
}

###############################################################################
#
# Public methods
#

proc comm {cmd args} {
    global comm
    set self comm
    switch -glob $cmd \
    send { return [eval commSend $args] } \
    conn* { return [eval commConnect $args] } \
    self { return $comm($self,port) } \
    interp* {
	upvar #0 comm($self,peers) peers
	return [concat $comm($self,port) [array names peers]]
    } \
    ids { return $comm(ids) } \
    new { return [eval commNew $args] } \
    init { return [eval commInit $args] } \
    shut* { return [eval commShutdown $args] } \
    abort { return [eval commAbort $args] } \
    destroy { return [eval commDestroy $args] } \
    hook {
	if [string match lost [lindex $args 0] {
	    return [commLostHook [lrange $args 1 end]]
	}
	error "Unimplemented hook invoked"
    } \
    remoteid {
	if [info exists comm($self,remoteid)] {
	    return $comm($self,remoteid)
	}
	error "No remote commands processed yet"
    } \
    error "bad option \"$cmd\": should be [join [lsort {send connect self interps ids new init shutdown abort destroy}] ", "]"
}

###############################################################################
#
# Private and internal methods
#
# Do not call or alter these procs or variables
#

if ![info exists comm] {
    set comm(debug) 0
    set comm(ids) comm
}

#
# Class variables:
#	lastport		saves last default listening port allocated 
#	debug			enable debug output
#	ids			list of allocated channels
#
# Instance variables:
# comm()
#	$ch,port		listening port (our id)
#	$ch,socket		listening socket
#	$ch,local		boolean to indicate if port is local
#	$ch,serial		next serial number for commands
#
#	$ch,buf,$fid		buffer to collect incoming data		
#	$ch,result,$serial	reply value set here to wake up sender
#	$ch,pending,$id		list of outstanding send serial numbers for id
#
#	$ch,peers($id)		open connections to peers; peers(id)=fid
#	$ch,fids($fid)		reverse mapping for peers; fids(fid)=id
#

proc commDebug arg {global comm; if $comm(debug) {uplevel 1 $arg}}

#
# See the Tk send(n) man page for details
#
# Usage: send ?-async? id cmd ?arg arg ...?
#
proc commSend {args} {
    upvar self self
    global comm

    if ![info exists comm($self,port)] {
	return -code error "comm channel $self not initialized"
    }

    set cmd send
    set i 0
    if [string match -async [lindex $args $i]] {
	set cmd async
	incr i
    }
    set id [lindex $args $i]
    incr i
    set args [lrange $args $i end]
    if ![info complete $args] {
	return -code error "Incomplete command"
    }
    if [string match "" $args] {
	return -code error "wrong # args: should be \"send ?-async? id arg ?arg ...?\""
    }

    set fid [commConnect $id]

    if {[incr comm($self,serial)] == 0x7fffffff} {set comm($self,serial) 0}
    set ser $comm($self,serial)

    commDebug {puts stderr "send <[list [list $cmd $ser $args]]>"}

    # The double list assures that the command is a single list when read.
    puts $fid [list [list $cmd $ser $args]]
    flush $fid

    # wait for reply if so requested
    if [string match send $cmd] {
	upvar comm($self,pending,$id) pending

	lappend pending $ser
	vwait comm($self,result,$ser)
	set pos [lsearch -exact $pending $ser]
	set pending [lreplace $pending $pos $pos]

	commDebug {puts stderr "result <$comm($self,result,$ser)>"}
	after idle unset comm($self,result,$ser)
	eval [lindex $comm($self,result,$ser) 0]
    }
}

###############################################################################
#
# Initialize by attaching to listening port
#
proc commNew {ch args} {
    global comm

    if {[lsearch -exact $comm(ids) $ch] >= 0} {
	error "Already existing channel: $ch"
    }
    if [string match comm $ch] {
	# allow comm to be recreated after destroy
    } elseif {![string compare $ch [info proc $ch]]} {
	error "Already existing command: $ch"
    } else {
	regsub "set self \[^\n\]*\n" [info body comm] "set self $ch\n" nbody
	proc $ch {cmd args} $nbody
    }
    lappend comm(ids) $ch
    set self $ch
    eval commInit $args
}

proc commInit {args} {
    upvar self self
    global comm
    upvar comm($self,port) port
    upvar comm($self,socket) socket
    upvar comm($self,local) local

    if ![info exists comm($self,serial)] {set comm($self,serial) 0}
    set local 1

    set opt 0
    foreach arg $args {
	incr opt
	if [info exists skip] {unset skip; continue}
	switch -exact -- $arg \
	-port	{
	    if {[regexp {[0-9]+} [lindex $args $opt]]} {
		set uport [lindex $args $opt]
	    }
	    set skip 1
	} \
	-local {
	    if {[string match 0 [lindex $args $opt]]} {
		set local 0
	    } else {
		set local 1
	    }
	    set skip 1
	}
    }

    # User is recycling object, possibly to change from insecure to secure
    if [info exists socket] {
	commAbort
	catch {close $socket}
    }

    if ![info exists uport] {
	if ![info exists comm(lastport)] {
	    set comm(lastport) [expr [pid] % 32768 + 10000]
	} else {
	    incr comm(lastport)
	}
	set port $comm(lastport)
    } else {
	set port $uport
    } 
    while 1 {
	set cmd [list socket -server [list commIncoming $self]]
	if $local {
	    lappend cmd -myaddr 127.0.0.1
	}
	lappend cmd $port
	if ![catch $cmd ret] {
	    break
	}
	if {[info exists uport] || ![string match "*already in use" $ret]} {
	    # don't erradicate the class
	    if ![string match comm $self] {
		proc $self {}
	    }
	    error $ret
	}
	set port [incr comm(lastport)]
    }
    set socket $ret

    # If port was 0, system allocated it for us
    if !$port {
	set port [lindex [fconfigure $socket -sockname] 2]
    }
    return $port
}

#
# Destroy the comm instance.
#
proc commDestroy {} {
    upvar self self
    global comm
    catch {close $comm($self,socket)}
    commAbort
    unset comm($self,port)
    unset comm($self,local)
    unset comm($self,socket)
    unset comm($self,serial)
    set pos [lsearch -exact $comm(ids) $self]
    set comm(ids) [lreplace $comm(ids) $pos $pos]
    if [string compare comm $self] {
	rename $self {}
    }
}

###############################################################################
#
# Called to connect to a remote interp
#
proc commConnect {id} {
    upvar self self
    global comm
    upvar comm($self,peers) peers

    commDebug {puts stderr "commConnect $id"}

    if [info exists peers($id)] {
	return $peers($id)
    }

    if {[llength $id] > 1} {
	set host [lindex $id 1]
    } else {
	set host 127.0.0.1
    }
    set port [lindex $id 0]
    set fid [socket $host $port]
    commNewConn $id $fid

    # send our id to identify ourselves to remote
    puts $fid $comm($self,port)
    flush $fid
    return $fid
}

#
# Called for an incoming new connection
#
proc commIncoming {self fid addr remport} {
    global comm

    commDebug {puts stderr "commIncoming $self $fid $addr $remport"}

    # remote Id is the first word of first line; rest of line ignored
    set id [lindex [gets $fid] 0]

    if [string compare 127.0.0.1 $addr] {
	set id "$id $addr"
    }

    upvar comm($self,peers) peers
    if {[info exist peers($id)] && $id != $comm($self,port)} {
	# this can happen when talking to ourself (ok) and
	# when two comms are connecting to each other simaltaneously (bad)
	puts stderr "commIncoming race condition: $id"
    }

    commNewConn $id $fid
}

#
# Common new connection processing
#
proc commNewConn {id fid} {
    upvar self self
    global comm
    upvar comm($self,peers) peers
    upvar comm($self,fids) fids

    commDebug {puts stderr "commNewConn $id $fid"}

    if ![info exists peers($id)] {
	# race condition
	set comm($self,pending,$id) {}
	set peers($id) $fid
    }
    set fids($fid) $id
    fconfigure $fid -trans binary -blocking 0
    fileevent $fid readable [list commCollect $self $fid]
}

###############################################################################
#
#
# Close down a peer connection.
#
proc commShutdown {id} {
    upvar self self
    global comm

    upvar comm($self,peers) peers
    if [info exists peers($id)] {
	commLostConn $peers($id) "Connection shutdown by request"
    }
}

#
# Close down all peer connections
#
proc commAbort {} {
    upvar self self
    global comm
    upvar comm($self,peers) peers

    foreach id [array names peers] {
	commLostConn $peers($id) "Connection aborted by request"
    }
}

# Called to tidy up a lost connection, including aborting ongoing sends
# Each send should clean themselves up in pending/result.
#
proc commLostConn {fid {reason "target application died or connection lost"}} {
    upvar self self
    global comm

    commDebug {puts stderr "commLostConn $fid $reason"}

    catch {close $fid}

    upvar comm($self,peers) peers
    upvar comm($self,fids) fids

    set id $fids($fid)

    catch {commLost $id $reason}

    foreach s $comm($self,pending,$id) {
	set comm($self,result,$s) [list [list return -code error $reason]]
    }
    unset fids($fid)
    catch {unset peers($id)}		;# race condition
    catch {unset comm($self,buf,$fid)}

    return $reason
}

#
# Override or append to this function to catch when a remote dies.
# This will be replaced by "comm hook".
#
proc commLost {id reason} {upvar self self}

proc commLostHook cmd {
    if ![string match *$cmd* [info body commLost]] {
	proc_body_append commLost $cmd
    }
}

###############################################################################
#
# Called from the fileevent to read from fid and append to the buffer.
# This continues until we get a whole command, which we then invoke.
#
proc commCollect {self fid} {
    global comm
    upvar #0 comm($self,buf,$fid) data

    set nbuf [read $fid]
    if [eof $fid] {
	fileevent $fid readable {}		;# be safe
	commLostConn $fid
	return
    }
    append data $nbuf

    commDebug {puts stderr "collect <$data>"}

    # If data contains at least one complete command, we will
    # be able to take off the first element, which is a list holding
    # the command.  This is true even if data isn't a well-formed
    # list overall, with unmatched open braces.  This works because
    # each command in the protocol ends with a newline, this allowing
    # lindex and lreplace to work.
    while {![catch {set cmd [lindex $data 0]}]} {
	commDebug {puts stderr "cmd <$data>"}
	if [string match "" $cmd] break
	if [info complete $cmd] {
	    set data [lreplace $data 0 0]
	    after idle [list commExec $self $fid $cmd]
	}
    }
}

#
# Recv and execute a remote command, returning the result and/or error
#
# buffer should contain:
#	send # {cmd}		execute cmd and send reply with serial #
#	async # {cmd}		execute cmd but send no reply
#	reply # {cmd}		execute cmd as reply to serial #
#
# Unknown commands are silently discarded
#
proc commExec {self fid buf} {
    commDebug {puts stderr "exec <$buf>"}

    set cmd [lindex $buf 0]
    set ser [lindex $buf 1]
    set buf [lrange $buf 2 end]
    switch -- $cmd \
	reply {
	    global comm
	    set comm($self,result,$ser) $buf
	    return
	} \
	send - async {} \
	default return

    commDebug {puts stderr "exec2 <$buf>"}

    # Only valid when immediately retrieved
    global comm
    upvar comm($self,fids) fids
    set comm($self,remoteid) $fids($fid)

    # exec command
    set err [catch [concat uplevel #0 [lindex $buf 0]] ret]

    commDebug {puts stderr "res <$err,$ret>"}

    # The double list assures that the command is a single list when read.
    if [string match send $cmd] {
	# catch return in case we just lost target.  consider:
	#	comm send $other comm send [comm self] exit
	catch {
	    # send error or result
	    if {$err == 1} {
		global errorInfo
		puts $fid [list [list reply $ser [list return \
				    -code $err -errorinfo $errorInfo $ret]]]
	    } else {
		puts $fid [list [list reply $ser [list return $ret]]]
	    }
	    flush $fid
	}
    }
}

###############################################################################
#
# Finish creating "comm" using the default port for this interp.
#
comm init

#eof
