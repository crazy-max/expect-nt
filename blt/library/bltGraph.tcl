package provide BLT 8.0

proc Blt_ActiveLegend { graph } {
    global bltActiveEntry

    set bltActiveEntry($graph) "-1"
    bind bltActiveLegend <Motion>  {
	BltActivateLegend %W %x %y
    }    
    BltAddBindTag $graph bltActiveLegend 
}

proc Blt_Crosshairs { graph } {
    bind bltCrosshairs <Any-Motion>   {
	%W crosshairs configure -position @%x,%y 
    }
    $graph crosshairs configure -color red
    bind $graph <Enter> [format {
	BltAddBindTag %s bltCrosshairs  
	%s crosshairs on
    } $graph $graph]
    bind $graph <Leave> [format {
	BltRemoveBindTag %s bltCrosshairs  
	%s crosshairs off
    } $graph $graph]
}

proc Blt_ZoomStack { graph } {
    global bltZoom

    set bltZoom($graph,A,x) {}
    set bltZoom($graph,A,y) {}
    set bltZoom($graph,B,x) {}
    set bltZoom($graph,B,y) {}
    set bltZoom($graph,stack) {}
    set bltZoom($graph,corner) A

    bind $graph <1> { 
	BltSetZoomPoint %W %x %y 
    }
    bind $graph <ButtonPress-3> {
	BltResetZoom %W 
    }
}

proc Blt_PrintKey { graph } {
    bind bltPrintGraph <Shift-ButtonRelease-3>  {
	%W postscript output "out.ps"  -landscape 1 -maxpect 1 -decorations 0
	puts stdout "wrote file \"out.ps\"."
	flush stdout
	break
    }
    BltAddBindTag $graph bltPrintGraph 
}

proc Blt_ClosestPoint { graph } {
    bind bltClosestPoint <Control-ButtonPress-1>  {
	BltFindElement %W %x %y
	break
    }
    BltAddBindTag $graph bltClosestPoint 
}

proc BltAddBindTag { graph name } {
    set oldtags [bindtags $graph]
    if { [lsearch $oldtags $name] < 0 } {
	bindtags $graph [concat $name $oldtags]
    }
}

proc BltRemoveBindTag { graph name } {
    set tagList {}
    foreach tag [bindtags $graph] {
	if { $tag != $name } {
	    lappend tagList $tag
	}
    }
    bindtags $graph $tagList
}

proc BltActivateLegend { graph x y } {
    global bltActiveEntry

    set old $bltActiveEntry($graph)
    set new [$graph legend get @$x,$y]
    if { $old != $new } {
	if { $old != "-1" } {
	    $graph legend deactivate $old
	    $graph element deactivate $old
	}
	if { $new != "" } {
	    $graph legend activate $new
	    $graph element activate $new
	}
    }
    set bltActiveEntry($graph) $new
}


proc BltFindElement { graph x y } {
    if ![$graph element closest $x $y info -interpolate 1] {
	bell
	return
    }
    # --------------------------------------------------------------
    # find(name)		- element Id
    # find(index)		- index of closest point
    # find(x) find(y)		- coordinates of closest point
    #				  or closest point on line segment.
    # find(dist)		- distance from sample coordinate
    # --------------------------------------------------------------
    set markerName "bltClosest_$info(name)"
    $graph marker delete $markerName
    $graph marker create text -coords { $info(x) $info(y) } \
	-name $markerName \
	-text "$info(name): $info(dist)\nindex $info(index)" \
	-font *lucida*-r-*-10-* \
	-anchor center -justify left \
	-yoffset 0 -bg {}
    BltFlashPoint $graph $info(name) $info(index) 10
}

proc BltFlashPoint { graph name index count } {
    if { $count & 1 } {
        $graph element deactivate $name 
    } else {
        $graph element activate $name $index
    }
    incr count -1
    if { $count > 0 } {
	after 200 BltFlashPoint $graph $name $index $count
	update
    } else {
	$graph marker delete "bltClosest_*"
    }
}

proc BltGetCoords { graph x y index } {

    set coords [$graph invtransform $x $y]
    set x [lindex $coords 0]
    set y [lindex $coords 1]

    scan [$graph xaxis limits] "%s %s" xmin xmax
    scan [$graph yaxis limits] "%s %s" ymin ymax

    if { $x > $xmax } { 
	set x $xmax 
    } elseif { $x < $xmin } { 
	set x $xmin 
    }

    if { $y > $ymax } { 
	set y $ymax 
    } elseif { $y < $ymin } { 
	set y $ymin 
    }
    global bltZoom
    set bltZoom($graph,$index,x) $x
    set bltZoom($graph,$index,y) $y
}

proc BltMarkPoint { graph index } {
    global bltZoom
    set x $bltZoom($graph,$index,x)
    set y $bltZoom($graph,$index,y)

    set marker "bltZoom_text_$index"
    set text [format "x=%.4g\ny=%.4g" $x $y] 

    if [$graph marker exists $marker] {
     	$graph marker configure $marker -coords { $x $y } -text $text 
    } else {
    	$graph marker create text -coords { $x $y } -name $marker \
   	    -font *lucida*-r-*-10-* \
	    -text $text -anchor center -bg {} -justify left
    }
}

proc BltPopZoom { graph } {
    global bltZoom

    set zoomStack $bltZoom($graph,stack)
    if { [llength $zoomStack] > 0 } {
	set cmd [lindex $zoomStack 0]
	set bltZoom($graph,stack) [lrange $zoomStack 1 end]
	eval $cmd
	BltZoomTitleLast $graph
	busy hold $graph
	update
	set cmd [format {
	    if { $bltZoom(%s,corner) == "A" } {
		%s marker delete "bltZoom_title"
	    }
	} $graph $graph ]
	after 2000 $cmd
	busy release $graph
    } else {
	$graph marker delete "bltZoom_title"
    }
}

# Push the old axis limits on the stack and set the new ones

proc BltPushZoom { graph } {
    $graph marker delete "bltZoom_*" 

    global bltZoom
    set x1 $bltZoom($graph,A,x)
    set y1 $bltZoom($graph,A,y)
    set x2 $bltZoom($graph,B,x)
    set y2 $bltZoom($graph,B,y)

    if { ($x1 == $x2) && ($y1 == $y2) } { 
	# No delta, revert to start
	return
    }

    set cmd [format {
	%s xaxis configure -min "%s" -max "%s"
	%s yaxis configure -min "%s" -max "%s"
    } $graph [$graph xaxis cget -min] [$graph xaxis cget -max] \
		 $graph [$graph yaxis cget -min] [$graph yaxis cget -max] ]

    if { $x1 > $x2 } { 
	$graph xaxis configure -min $x2 -max $x1 
    } elseif { $x1 < $x2 } {
	$graph xaxis configure -min $x1 -max $x2
    } 
    if { $y1 > $y2 } { 
	$graph yaxis configure -min $y2 -max $y1
    } elseif { $y1 < $y2 } {
	$graph yaxis configure -min $y1 -max $y2
    } 
    set bltZoom($graph,stack) [linsert $bltZoom($graph,stack) 0 $cmd]

    busy hold $graph
    update
    busy release $graph
}

proc BltResetZoom { graph } {
    global bltZoom

    $graph marker delete "bltZoom_*" 
    if { $bltZoom($graph,corner) == "A" } {
	# Reset the whole axis
	BltPopZoom $graph
    } else {
	set bltZoom($graph,corner) A
	bind $graph <Motion> { }
    }
}

proc BltZoomTitleNext { graph } {
    global bltZoom

    set level [expr [llength $bltZoom($graph,stack)] + 1]
    set title "Zoom #$level"
    $graph marker create text -name "bltZoom_title" -text $title \
	    -coords {-Inf Inf} -anchor nw -bg {} 
}

proc BltZoomTitleLast { graph } {
    global bltZoom

    set level [llength $bltZoom($graph,stack)]
    if { $level > 0 } {
	set title "Zoom #$level"
     	$graph marker create text -name "bltZoom_title" -text $title \
		-coords {-Inf Inf} -anchor nw -bg {} 
    }
}

proc BltSetZoomPoint { graph x y } {
    global bltZoom

    BltGetCoords $graph $x $y $bltZoom($graph,corner)
    if { $bltZoom($graph,corner) == "A" } {
	# First corner selected, start watching motion events

	#BltMarkPoint $graph A
	BltZoomTitleNext $graph 
	bind $graph <Any-Motion> { 
	    BltGetCoords %W %x %y B
    	    #BltMarkPoint $graph B
	    BltBox %W
	}
	set bltZoom($graph,corner) B
    } else {
	bind $graph <Any-Motion> { }
	BltPushZoom $graph 
	set bltZoom($graph,corner) A
    }
}

proc BltBox { graph } {
    global bltZoom

    if { $bltZoom($graph,A,x) > $bltZoom($graph,B,x) } { 
	set x1 $bltZoom($graph,B,x)
	set x2 $bltZoom($graph,A,x) 
	set y1 $bltZoom($graph,B,y)
	set y2 $bltZoom($graph,A,y) 
    } else {
	set x1 $bltZoom($graph,A,x)
	set x2 $bltZoom($graph,B,x) 
	set y1 $bltZoom($graph,A,y)
	set y2 $bltZoom($graph,B,y) 
    }
    set coords {
	$x1 $y1 $x1 $y2 $x1 $y1 $x2 $y1 $x2 $y1 $x2 $y2 $x1 $y2 $x2 $y2 
    }
    if [$graph marker exists "bltZoom_outline"] {
	$graph marker configure "bltZoom_outline" -coords $coords
    } else {
	$graph marker create line -coords $coords -name "bltZoom_outline" \
	    -dashes { 4 2 }
    }
    $graph marker before "bltZoom_outline"
}


