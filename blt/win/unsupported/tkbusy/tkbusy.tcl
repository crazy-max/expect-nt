#-------------------------------------------------------------------------
#
# Name:
#   tkbusy -- disable widgets while an application is busy
#
# Synopsis:
#   tkbusy cmd ?but?
#
# Description: 
#   If "cmd" is "on", then all widgets are disabled, unless "but"
#   is specified (see below). If "cmd" is "off", the previously
#   disabled widgets are re-enabled again. "but" is the name of
#   a button widget that should not be disabled. If specified
#   in addition to an "on" command, pressing ^C (Control-c) while
#   the application is busy will invoke this button. However, this
#   will only work if the application invokes Tcl's event loop by
#   either calling "Tcl_DoOneEvent()" or by issuing a "vwait"
#   command. An application that wants to use tkbusy just needs to
#   "source" this file. After that, it can immediately use the
#   command "tkbusy cmd ?but?" as described above. Note that tkbusy
#   requires Tcl/Tk 8.0b1 or better, because it uses namespaces.
#
# Side effects:
# - Creates an invisible button ".tkbusy"; "tkbusy off" deletes it.
# - Defines a persistant binding tag "TkBusy".
#
# Caveats:
# - An unrecoverable error occurs if a toplevel window is destroyed
#   while the application is busy. Therefore, toplevel windows should
#   be protected using "wm protocol $toplevel WM_DELETE_WINDOW bell".
#   If that's not possible, all commands in TkBusy must be "catch"ed.
# - Widgets that are created between "tkbusy on" and "tkbusy off" will
#   not be disabled (might come handy in certain cases).
# - The performance of "tkbusy" depends on the number of widgets that
#   have to be disabled: the more widgets, the slower "tkbusy".
# - Does not block bindings of canvas items.
# - Calls to "tkbusy" must not be nested.
# - If "tkbusy on" is called while the mouse cursor is over a highlighted
#   widget, the color won't be reset to normal state (not even after
#   "tkbusy off"). If that's a problem, tk_strictMotif should be set to 1.
#
# Author:
#   Michael Schumacher (mike@hightec.saarlink.de).
#
# Version:
#   1.0, 30-Mar-96	initial version
#   1.1, 04-Jan-97	fixed "grid" to conform to Tk 8.0; win support
#   1.2, 07-Jun-97	added namespaces
#
# Copyright:
#   Public domain.
#
#-------------------------------------------------------------------------

namespace TkBusy {
  namespace export tkbusy
  variable TkBusy
  variable cursor [expr { ($tcl_platform(platform) == "unix") \
		   ? "top_left_arrow" : "arrow" }]

  proc tkbusy { cmd {but ""} } {
    variable TkBusy
    variable cursor

    if { "$cmd" == "on" } {
      set TkBusy "focus [focus];destroy .tkbusy;"
      set script "button .tkbusy;focus .tkbusy;bindtags .tkbusy .tkbusy; \
	bind .tkbusy <FocusOut> \"focus .tkbusy\";"
      if { "$but" != "" } {
	append script "bindtags $but \"[bindtags $but]\";$but co \
	  -cu $cursor;bind .tkbusy <Control-c> \"$but in\";"
	bind TkBusy <Control-c> "$but in"
	focus $but
      } else {
	bind TkBusy <Control-c> { }
      }
      bind TkBusy <FocusIn> { focus .tkbusy }
      bind TkBusy <FocusOut> { focus .tkbusy }
      set w [info comm .*]
      foreach c $w {
	append TkBusy "$c con -cu \"[$c cg -cu]\"; \
	  bindtags $c \"[bindtags $c]\";"
	$c con -cu watch; bindtags $c TkBusy
	if { "[winfo cla $c]" == "Text" } {
	  foreach t [$c tag nam] {
            foreach b [$c tag bin $t] {
	      append TkBusy "$c tag bin $t $b \{[$c tag bin $t $b]\};"
	      $c tag bin $t $b { }
	    }
	  }
	}
      }
      eval $script
    } else {
      eval $TkBusy; unset TkBusy
    }
    update idle
  }
}
namespace import TkBusy::tkbusy
# EOF
