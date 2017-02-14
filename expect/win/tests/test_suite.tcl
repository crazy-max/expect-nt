proc runtest {version body answer} {
    global interactive
    if {$interactive} {
	puts -nonewline stdout "Run test $version? "
	gets stdin yn
	if {[string range $yn 0 1] == "n"} {
	    return
	}
    }
    if [catch {set result [eval $body]} error] {
	global errorInfo
	puts "    - expectnt test $version failed: $error $errorInfo"
    } elseif {[string compare $result $answer] != 0} {
	puts "    - expectnt test $version failed"
	puts "      Result was:"
	puts "      --------------------------------------------------"
	puts "      $result"
	puts "      Result should have been:"
	puts "      --------------------------------------------------"
	puts "      $answer"
	puts ""
    } else {
	puts "    + expectnt test $version succeeded"
    }
}

# XXX: Create a procedure to recursively delete a directory.

#for {set run 1} {$run <= $runs} {incr run} {
#    puts ""
#    puts "Iteration #$run of $runs"
#}



# Tests using cmd
runtest 1.0 {
    file 
    spawn cmd
    send "dir\r"
}    
