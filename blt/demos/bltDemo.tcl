#
# This is so you can test the distribution without installing it.
#
if { ![info exists blt_library] } {
    if [file exists ../library] {
	set blt_library ../library
    }
}

lappend auto_path $blt_library
package require BLT 8.0

#
# Try to import the blt namespace into the global scope.  If it
# fails, we'll assume BLT was loaded into the global scope.
#
catch { 
    namespace import blt::*
}

#
# Convenient "quit" key.
#
bind all <Control-KeyPress-c> { exit 0 } 
focus .

#
# Replace Tk widgets with tiling widgets.
#

set tileCmds { button checkbutton radiobutton label toplevel frame scrollbar }

if { ![string compare $tcl_platform(platform) unix] } {
    if { $blt_useTclNamespaces } {
	namespace eval blt {
	    foreach cmd $tileCmds  {
		if { "[info commands ::blt::tile$cmd]" == "::blt::tile$cmd" } {
		    rename ::$cmd ::tk::$cmd
		    rename ::blt::tile$cmd ::$cmd
		}
	    }
	}
    } else {
	foreach cmd $tileCmds  {
	    if { "[info commands tile$cmd]" == "tile$cmd" } {
		rename $cmd blt_tk_$cmd
		rename tile$cmd $cmd
	    }
	}
    }
} else {
    #
    # Provide a dummy proc for "busy" on platforms which don't support it yet.
    #
    proc busy { args } {
	# ignore
    }
}
