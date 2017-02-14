#package require expect
load expect52.dll Expect

# For now, have consoles visible
set exp_nt_debug 1

proc testchars {} {
    send "aAbBcCdDeEfFgG\r"
    send "1234567890\r"
    send "ls !@#\$%^&*()\r"
    send "ls abcdefghijklmnopqrstuvwxyz\r"
    send "ls ABCDEFGHIJKLMNOPQRSTUVWXYZ\r"
    send "ls ~\`!1@2#3\$4%5^6&7*8(9)0_-+=|\\\r"
    set z [format "%c" 26]
    send "ls 	$z\r"
    send "\[\{\]\};:\'\",<.>/?\r"
}

# The next bit of code doesn't seem to work in a procedure.. 
# Probably need to be setting some value globally.
proc zeego {} {
    # set timeout -1
    # set timeout 10000
    global spawn_id
    spawn ftp
    puts "spawn_id=$spawn_id"
    expect "ftp> "
    send "open zeego\r"
    expect "(none)):"
    send "test\r"
    expect "Password:"
    send "testPasswd\r"
    expect "logged in."
    # The following line is wrong on purpose
    # expect "logged in>"
    expect "ftp> "
    send "ls\r"
    expect "ftp> "
}

# The next bit of code doesn't seem to work in a procedure.. 
# Probably need to be setting some value globally.
proc pipe-zeego {} {
    # set timeout -1
    # set timeout 10000
    global spawn_id
    spawn -pipes ftp
    puts "spawn_id=$spawn_id"
    expect "ftp> "
    send "open zeego\r"
    expect "(none)):"
    send "test\r"
    expect "Password:"
    send "testPasswd\r"
    expect "logged in."
    # The following line is wrong on purpose
    # expect "logged in>"
    expect "ftp> "
    send "ls\r"
    expect "ftp> "
}

proc socket-zeego {} {
    # set timeout -1
    # set timeout 10000
    global spawn_id
    spawn -socket ftp
    puts "spawn_id=$spawn_id"
    expect "ftp> "
    send "open zeego\r"
    expect "(none)):"
    send "test\r"
    expect "Password:"
    send "testPasswd\r"
    expect "logged in."
    # The following line is wrong on purpose
    # expect "logged in>"
    expect "ftp> "
    send "ls\r"
    expect "ftp> "
}

proc testany {} {
    global id1 id2 any_spawn_id
    set timeout -1
    spawn ftp
    set id1 $spawn_id
    spawn ftp
    set id2 $spawn_id

    # -i "$id2" crap2
    expect {
	-i "$id1" crap1 {puts "crap1"}
	-i "$any_spawn_id" "ftp> " {puts "found ftp>"}
    }
}

proc test-open {} {
    global id1 id2 f
    set f [open "d:/mksnt/mksnt/ls.exe"]
    spawn -open $f
    set id1 $spawn_id
    spawn -open $f
    set id2 $spawn_id
}

proc test-telnet {} {
    set timeout 1000000
    global id1
    spawn telnet
    expect "telnet>"
    send "open zeego\r"
    expect "login:"
    send "test\r"
    expect "Password:"
    send "testPasswd\r"
    expect "zeego:"
    send "ls\r"
    expect "zeego:"
    set id1 $spawn_id
}

proc traptest {} {
    global trapcount
    set trapcount 0
    proc trapped {} {
	global trapcount
	incr trapcount
	if {$trapcount == 5} {
	    send_user "bye bye"
	    exit
	}
    }
    trap {
	puts "Caught SIGINT"
	trapped
    } SIGINT
    # Ctrl-Break on Windows NT
    trap {
	puts "Caught SIGQUIT"
	trapped
    } SIGQUIT
    trap {
	puts "Caught SIGTERM"
	trapped
    } SIGTERM
    trap {
	puts "Caught SIGABRT"
	trapped
    } SIGABRT

    global x
    set x 1
    vwait x
}

proc testkill2 {} {
    global spawn_id
    set exp_nt_debug 1
    spawn ftp
    puts "spawn_id=$spawn_id"
    puts "Trying to kill ftp with a Ctrl-C"
    kill 2
    puts "spawn_id=$spawn_id"
}

proc testkill3 {} {
    global spawn_id
    set exp_nt_debug 1
    spawn ftp
    puts "spawn_id=$spawn_id"
    puts "Trying to kill ftp with a Ctrl-Break"
    kill 3
    puts "spawn_id=$spawn_id"
}

proc subslave {} {
    global spawn_id
    spawn cmd /c d:\\mksnt\\mksnt\\ls.exe
}

# Bogus commands run under cmd are causing problems right now
proc subslave1 {} {
    global spawn_id
    spawn cmd /c ls
}

# XXX: I think there is a bug when spawn_id is not set as global.
# That needs to be tested.

proc cmdtest1 {} {
    global spawn_id
    spawn cmd
    expect crap
}

proc cmdtest2 {} {
    global spawn_id

    exp_internal -f dbg.log 1
    spawn cmd
    send "ls\r"
    # The last part of this goes into a busy wait.  Find out why.
    after 6000
    expect crap

    # This second ls illustrates a problem.  Not all of the data is
    # collected.  This seems to be a problem in Tcl core.  PipeInputProc
    # returns all the data, but when it goes through the translation code,
    # it doesn't all come out in a timely fashion.
    send "ls\r"
    after 6000
    expect crap
}

proc cmdtest3 {} {
    global spawn_id

    exp_internal -f dbg.log 1
    spawn cmd

    # Run ls and wait until it should have completed
    send "ls\r"
    after 6000

    set ok 0
    expect {
	-re crap {}
	timeout {
	    set ok 1
	}
    }
    expect *
    puts "Saw: $expect_out(buffer)"

    # This second ls illustrates a problem.  Not all of the data is
    # collected.  This seems to be a problem in Tcl core.  PipeInputProc
    # returns all the data, but when it goes through the translation code,
    # it doesn't all come out in a timely fashion.
    send "ls\r"
    after 6000

    set ok 0
    expect {
	-re crap {}
	timeout {
	    set ok 1
	}
    }
    expect *
    puts "Saw: $expect_out(buffer)"
}

proc cmdtest4 {} {
    spawn cmd
    expect crap
}

proc usertest1 {} {
    set timeout 1000
    puts "Please enter a string"
    expect_user -re "(.*)\n"
    puts "You entered: $expect_out(buffer)"
}

# Compare the execution speed between the first zeego run and the
# second zeego run.  There are lots of callbacks to ExpPairReadable
# going on.
proc busytest1 {} {
    global spawn_id
    zeego
    close
    wait
    puts ""
    puts ""
    usertest1
    zeego
    close
    wait
}

proc getpass {} {
    set oldmode [stty -echo -raw]
    set timeout -1
    send_user "Password: "
    expect_user -re "(.*)\n"
    send_user "\n"
    puts "Your password was: $expect_out(1,string)\n"
    eval stty $oldmode
}

proc eoftest {} {
    spawn cmd.exe /c echo yes
    set timeout -1
    expect {
	eof {send_user "Saw eof"}
    }
}

proc nettest {} {
    spawn net.exe
    expect VIEW
}

proc calctest {} {
    exec echo hello | testcalc.exe
}

proc serialtest {} {
    # exp_internal -f dbg.log 0

    set exp_nt_debug 1
    global serid serfile
    global spawn_id;
    set f [open com1: w+]
    set setfile $f
    fconfigure $f -blocking 0 -buffering none -translation binary
    if [catch {
	set timeout 3
	spawn -open $f
	set serid $spawn_id
	puts "Serial id: $serid"
	send -i $serid "test\r"
	puts "*********** Debug A ***********"
	expect {
	    "Password" {send "testPasswd\r"}
	    eof {error "Premature EOF 0"}
	    timeout {puts "Timeout 0"}
	}
	puts "*********** Debug B ***********"
	expect {
	    "zeego:" {}
	    eof {error "Premature EOF 1"}
	    timeout {puts "Timeout 1"}
	}
	set timeout 60
	send "\\ls\r"
	puts "*********** Debug C ***********"
	expect {
	    "zeego:" {}
	    eof {error "Premature EOF 2"}
	    timeout {puts "Timeout 2"}
	}
	send "cd /usr/bin\r"
	puts "*********** Debug D ***********"
	expect {
	    "zeego:" {}
	    eof {error "Premature EOF 3"}
	}
	send "\\ls -l\r"
	puts "*********** Debug E ***********"
	expect {
	    "zeego:" {}
	    eof {error "Premature EOF 4"}
	}
    } msg] {
	close $serid
	put "Error: $msg"
    }
}

proc modemtest {} {
    exp_internal -f dbg.log 1
    set timeout 10000
    set exp_nt_debug 1
    global serid;
    global spawn_id;
    set f [open com2: w+]
    fconfigure $f -blocking 0 -buffering none -translation binary
    if [catch {
	spawn -open $f
	set serid $spawn_id

	send "AT\r"
	expect {
	    timeout {error "Timeout 1"}
	    "OK" {}
	}
	
	send "AT\r"
	expect {
	    timeout {error "Timeout 1"}
	    "OK" {}
	}
    } msg] {
	close $serid
	puts "Error: $msg"
    }
}

proc consoletest {} {
    global x
    fileevent stdin readable {gets stdin line; puts "Read: $line"}
    set x 1
    vwait x
}

proc crashtest {} {
    set exp_nt_debug 1
    spawn testcrash.exe
}

proc emacstest {opt} {
    set exp_nt_debug 1

    eval "spawn $opt emacs -nw"
    send "\021"; send -null

    for {set i 1} {$i <= 26} {incr i} {
	set s [format "%03o" $i]
	send "\021$s"
    }

    expect {
	{\^@\^A\^B\^C\^D\^E\^F\^G\^H} {}
	eof {error "unexpected eof: did not see ^@^A^B^C^D^E^F^G^H"}
	timeout {set saw [expect *]; error "unexpected timeout: did not see ^@^A^B^C^D^E^F^G^H but saw $saw"}
    }

    expect {
	{\^K\^L\^M\^N\^O\^P\^Q\^R\^S\^T\^U\^V\^W\^X\^Y\^Z} {}
	eof {error "unexpected eof: did not see ^K^L^M^N^O^P^Q^R^S^T^U^V^W^X^Y^Z"}
	timeout {error "unexpected timeout: did not see ^K^L^M^N^O^P^Q^R^S^T^U^V^W^X^Y^Z"}
    }

    send "testing    1 2 3 4 5"
}

proc emacs1 {} {
    emacstest -socket
}
proc emacs2 {} {
    emacstest {}
}

proc k95 {} {
    set timeout 30
    global spawn_id

    exp_internal -f dbg.log 0
    log_user 0
    spawn k95beta.exe
    expect {
	"K-95" {set ok 1}
	eof {error "Unexpected eof"}
	timeout {error "Unexpected timeout: did not see k-95"}
    }
    send "telnet zeego\r"
    expect {
	"perspecta" {set ok 1}
	eof {error "Unexpected eof"}
	timeout {error "Unexpected timeout: did not see perspecta"}
    }
}

proc k95loop {} {
    set timeout 30000
    global spawn_id

    while {1} {
	catch {exec rm -f dbg.log}
	exp_internal -f dbg.log 0
	log_user 0
	spawn k95beta.exe
	expect {
	    "K-95" {set ok 1}
	    eof {error "Unexpected eof"}
	    timeout {error "Unexpected timeout: did not see k-95"}
	}
	send "telnet zeego\r"
	expect {
	    "perspecta" {set ok 1}
	    eof {error "Unexpected eof"}
	    timeout {error "Unexpected timeout: did not see perspecta"}
	}
	close
	wait
    }
}

#zeego

#spawn d:/mksnt/mksnt/ls.exe
#spawn testcat
#testany
#test-open
