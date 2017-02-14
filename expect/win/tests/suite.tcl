load expect52.dll Expect

proc zeego1 {} {
    global spawn_id
    global timeout
    spawn ftp
    puts "spawn_id=$spawn_id"
    expect "ftp> "
    send "open zeego\r"
    expect "(none)):"
    send "test\r"
    expect "for test."
    send "testPasswd\r"
    expect "logged in."
    # The following line is wrong on purpose
    # expect "logged in>"
    expect "ftp> "
    send "ls\r"
    expect "ftp> "
}

proc zeego2 {} {
    global timeout
    set timeout -1
    zeego1
}

proc zeego3 {} {
    global timeout
    set timeout 10000
    zeego1
}

proc closetest {} {
    global spawn_id
    puts "closetest 1"
    set f [open "|d:/mksnt/mksnt/ls.exe"]
    if {! [catch {close -i $f} msg]} {
	puts "close -i $f should have failed"
    }
    close

    puts "closetest 2"
    set f [open "|d:/mksnt/mksnt/ls.exe"]
    close $f

    puts "closetest 3"
    spawn ftp
    close -i $spawn_id

    puts "closetest 4"
    spawn ftp
    close $spawn_id

    puts "closetest 5"
    spawn ftp
    close

    puts "closetest 6"
    close -i stderr

    puts "closetest 7"
    close -i stdout

    puts "closetest 8"
    close -i stdin
}

proc test1 {} {
    for {set i 0} {$i < 5} {incr i} {
	global spawn_id
	zeego1
	set id1 $spawn_id
	zeego2
	set id2 $spawn_id
	zeego3
	set id3 $spawn_id
	close
	close $id1
	close -i $id2
    }
}

closetest


