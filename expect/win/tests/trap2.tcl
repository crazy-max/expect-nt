#
# trap2.tcl
#
#	Used in conjunction with trap.test.  This script traps
#	SIGINT and SIGQUIT.
#
# RCS: $Header: /disk/3/CVSROOT/Mitel/expect/win/tests/trap2.tcl,v 1.1 1997/11/13 12:16:36 chaffee Exp $

set exp_nt_debug 1
spawn telnet odie.cs.berkeley.edu

trap {
    puts "Caught SIGINT"
} SIGINT

trap {
    puts "Caught SIGQUIT"
    close
} SIGQUIT

set x 1
vwait x
