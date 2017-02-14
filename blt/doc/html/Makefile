################################################################################
#
# ./doc/html/Makefile --
#
#	This Makefile produces HTML versions of BLT's man pages.
#
# Created on 06JUL97 by Michael Schumacher (mike@hightec.saarlink.de)
#
################################################################################

RMAN	= /usr/sbin/rman
RM	= rm -f
CHMOD	= chmod
MANPATH	= ..
TROFF	= groff -man -Tascii

MEN	= BLT.man bitmap.man dragdrop.man spline.man vector.man \
	  barchart.man bltdebug.man graph.man table.man watch.man \
	  bgexec.man busy.man htext.man tile.man winop.man piechart.man

HTMLS	= ${MEN:.man=.html}

VPATH	= $(MANPATH)

.SUFFIXES: .man .html

.man.html:
	$(RM) $@
	(cd $(MANPATH); $(TROFF) $*.man) | $(RMAN) -f HTML > $@
	$(CHMOD) 444 $@

all: $(HTMLS)

clean:
	$(RM) $(HTMLS)
