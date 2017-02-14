catch {file delete testwstation.debug}
set f [open testwstation.debug w]
puts $f starting
close $f

proc telnet-test {} {
    exp_internal -f testwstation.log 1
    if [catch {
	set timeout 100
	spawn telnet
	expect {
	    "telnet> " {}
	    eof {error "unable to spawn telnet"}
	    timeout {error "no response seen from telnet"}
	}
	send "open odie.cs.berkeley.edu\r"
	expect {
	    "login: " {}
	    eof {error "unexpected eof"}
	    timeout {error "timeout connecting to odie.cs.berkeley.edu"}
	}
	send "bogus\r"
	expect {
	    "Password:" {}
	    eof {error "unexpected eof"}
	    timeout {error "unexpected timeout"}
	}
	send "none\r"
	expect {
	    "Login incorrect" {}
	    eof {error "unexpected eof"}
	    timeout {error "unexpected timeout"}
	}
    } msg] {} else {set msg 1}
    catch {close}
    catch {wait}
    set msg
}

telnet-test
