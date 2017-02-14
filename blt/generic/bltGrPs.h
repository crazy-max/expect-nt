/*
 * bltGrPs.h --
 *
 *	This file contains the prologue needed to generate a printable
 *	PostScript (tm) copy of several BLT widgets. If your compiler
 *	doesn't support large strings, the prolog will be taken from
 *	$blt_library/bltGraph.pro at runtime (the "configure" script
 *	will find out automagically whether your compiler can handle
 *	large strings, and will set up the necessary "#define"s).
 *
 * Copyright (c) 1993-1997 by AT&T Bell Laboratories.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _BLTGRPS_H
#define _BLTGRPS_H

#ifndef NULL
#   ifdef __cplusplus
#	define NULL 0
#   else
#	define NULL (void *)0
#   endif
#endif

static char *postScriptPrologue[] = {
"\
%\n\
% PostScript prolog file of the BLT graph widget.\n\
%\n\
% Copyright 1989-1992 Regents of the University of California.\n\
% Permission to use, copy, modify, and distribute this\n\
% software and its documentation for any purpose and without\n\
% fee is hereby granted, provided that the above copyright\n\
% notice appear in all copies.  The University of California\n\
% makes no representations about the suitability of this\n\
% software for any purpose.  It is provided \"as is\" without\n\
% express or implied warranty.\n\
%\n\
% Copyright 1991-1996 by AT&T Bell Laboratories.\n\
% Permission to use, copy, modify, and distribute this software\n\
% and its documentation for any purpose and without fee is hereby\n\
% granted, provided that the above copyright notice appear in all\n\
% copies and that both that the copyright notice and warranty\n\
% disclaimer appear in supporting documentation, and that the\n\
% names of AT&T Bell Laboratories any of their entities not be used\n\
% in advertising or publicity pertaining to distribution of the\n\
% software without specific, written prior permission.\n\
%\n\
% AT&T disclaims all warranties with regard to this software, including\n\
% all implied warranties of merchantability and fitness.  In no event\n\
% shall AT&T be liable for any special, indirect or consequential\n\
% damages or any damages whatsoever resulting from loss of use, data\n\
% or profits, whether in an action of contract, negligence or other\n\
% tortuous action, arising out of or in connection with the use or\n\
% performance of this software.\n\
%\n\n\
200 dict begin\n\n\
/BaseRatio 1.3467736870885982 def	% Ratio triangle base / symbol size\n\
/BgColorProc 0 def			% Background color routine (symbols)\n\
/DrawSymbolProc 0 def			% Routine to draw symbol outline/fill\n\
/StippleProc 0 def			% Stipple routine (bar segments)\n\
/DashesProc 0 def			% Dashes routine (line segments)\n\n\
/encoding {\n  ISOLatin1Encoding\n} def\n",
"systemdict /encodefont known {\n\
  /realsetfont /setfont load def\n\
  /setfont { \n    encoding encodefont realsetfont \n  } def\n\
} if\n\n\
/Stroke {\n  gsave\n    stroke\n  grestore\n} def\n\n\
/Fill {\n  gsave\n    fill\n  grestore\n} def\n\n\
/SetFont { 	\n  % Stack: pointSize fontName\n\
  findfont exch scalefont setfont\n} def\n\n\
/Box {\n  % Stack: x y width height\n\
  newpath\n  exch 4 2 roll moveto\n  dup 0 rlineto\n\
  exch 0 exch rlineto\n  neg 0 rlineto\n  closepath\n\
} def\n\n\
/SetFgColor {\n  % Stack: red green blue\n\
  CL 0 eq { \n    pop pop pop 0 0 0 \n  } if\n  setrgbcolor\n\
  CL 1 eq { \n    currentgray setgray \n  } if\n\
} def\n\n\
/SetBgColor {\n  % Stack: red green blue\n\
  CL 0 eq { \n    pop pop pop 1 1 1 \n  } if\n  setrgbcolor\n\
  CL 1 eq { \n    currentgray setgray \n  } if\n\
} def\n\n\
% The next two definitions are taken from \"$tk_library/prolog.ps\"\n\n\
% desiredSize EvenPixels closestSize\n%\n\
% The procedure below is used for stippling.  Given the optimal size\n\
% of a dot in a stipple pattern in the current user coordinate system,\n\
% compute the closest size that is an exact multiple of the device's\n\
% pixel size.  This allows stipple patterns to be displayed without\n\
% aliasing effects.\n\n\
/EvenPixels {\n",
"  % Compute exact number of device pixels per stipple dot.\n\
  dup 0 matrix currentmatrix dtransform\n\
  dup mul exch dup mul add sqrt\n\n\
  % Round to an integer, make sure the number is at least 1, and compute\n\
  % user coord distance corresponding to this.\n\
  dup round dup 1 lt {pop 1} if\n\
  exch div mul\n\
} bind def\n\n\
% width height string filled StippleFill --\n\
%\n\
% Given a path and other graphics information already set up, this\n\
% procedure will fill the current path in a stippled fashion.  \"String\"\n\
% contains a proper image description of the stipple pattern and\n\
% \"width\" and \"height\" give its dimensions.  If \"filled\" is true then\n\
% it means that the area to be stippled is gotten by filling the\n\
% current path (e.g. the interior of a polygon); if it's false, the\n\
% area is gotten by stroking the current path (e.g. a wide line).\n\
% Each stipple dot is assumed to be about one unit across in the\n\
% current user coordinate system.\n\n\
/StippleFill {\n",
"  % Turn the path into a clip region that we can then cover with\n\
  % lots of images corresponding to the stipple pattern.  Warning:\n\
  % some Postscript interpreters get errors during strokepath for\n\
  % dashed lines.  If this happens, turn off dashes and try again.\n\n\
  gsave\n\
    {eoclip}\n\
    {{strokepath} stopped {grestore gsave [] 0 setdash strokepath} if clip}\n\
    ifelse\n\n\
    % Change the scaling so that one user unit in user coordinates\n\
    % corresponds to the size of one stipple dot.\n\
    1 EvenPixels dup scale\n\n\
    % Compute the bounding box occupied by the path (which is now\n\
    % the clipping region), and round the lower coordinates down\n\
    % to the nearest starting point for the stipple pattern.\n\n\
    pathbbox\n\
    4 2 roll\n\
    5 index div cvi 5 index mul 4 1 roll\n\
    6 index div cvi 6 index mul 3 2 roll\n\n\
    % Stack now: width height string y1 y2 x1 x2\n\
    % Below is a doubly-nested for loop to iterate across this area\n",
"    % in units of the stipple pattern size, going up columns then\n\
    % across rows, blasting out a stipple-pattern-sized rectangle at\n\
    % each position\n\n\
    6 index exch {\n      2 index 5 index 3 index {\n\
	% Stack now: width height string y1 y2 x y\n\n\
	gsave\n	  1 index exch translate\n\
	  5 index 5 index true matrix {3 index} imagemask\n\
	grestore\n      } for\n      pop\n    } for\n\
    pop pop pop pop pop\n\
  grestore\n  newpath\n} bind def\n\n\
/Segment {	% Stack: x1 y1 x2 y2\n\
  newpath 4 2 roll moveto lineto stroke\n} def\n\n\
/EndText {\n  %Stack :\n  grestore\n} def\n\n\
/BeginText {\n  %Stack :  w h theta centerX centerY\n  gsave\n\
    % Translate the origin to the center of bounding box and rotate\n\
    translate neg rotate\n\
    % Translate back to the origin of the text region\n\
    -0.5 mul exch -0.5 mul exch translate\n\
} def\n\n",
"/DrawLine {\n  %Stack : str strWidth x y\n\
  moveto				% Go to the text position\n\
  exch dup dup 4 2 roll\n\n\
  % Adjust character widths to get desired overall string width\n\
  % adjust X = (desired width - real width)/#chars\n\n\
  stringwidth pop sub exch\n\
  length div\n\
  0 3 -1 roll\n\n\
  % Flip back the scale so that the string is not drawn in reverse\n\n\
  gsave\n    1 -1 scale\n    ashow\n  grestore\n\
} def\n\n\
/DrawBitmap {\n\
  % Stack: ?bgColorProc? boolean centerX centerY width height theta imageStr\n\
  gsave\n\
    6 -2 roll translate			% Translate to center of bounding box\n\
    4 1 roll neg rotate			% Rotate by theta\n\
    \n\
    % Find upperleft corner of bounding box\n\
    \n",
"    2 copy -.5 mul exch -.5 mul exch translate\n\
    2 copy scale			% Make pixel unit scale\n\
    newpath\n\
    0 0 moveto 0 1 lineto 1 1 lineto 1 0 lineto\n\
    closepath\n\
    \n\
    % Fill rectangle with background color\n\
    \n\
    4 -1 roll { \n\
      gsave \n\
	4 -1 roll exec fill \n\
      grestore \n\
    } if\n\
    \n\
    % Paint the image string into the unit rectangle\n\
    \n\
    2 copy true 3 -1 roll 0 0 5 -1 roll 0 0 6 array astore 5 -1 roll\n\
    imagemask\n\
  grestore\n\
}def\n\
\n",
"% Symbols:\n\n\
% Skinny-cross\n\
/Sc {\n  % Stack: x y symbolSize\n\
  gsave\n    3 -2 roll translate 45 rotate\n\
    0 0 3 -1 roll Sp\n  grestore\n} def\n\n\
% Skinny-plus\n\
/Sp {\n  % Stack: x y symbolSize\n\
  gsave\n\
    3 -2 roll translate\n\
    2 idiv\n\
    dup 2 copy\n\
    newpath neg 0 moveto 0 lineto\n\
    DrawSymbolProc\n\
    newpath neg 0 exch moveto 0 exch lineto\n\
    DrawSymbolProc\n\
  grestore\n\
} def\n\n\
% Cross\n\
/Cr {\n  % Stack: x y symbolSize\n\
  gsave\n    3 -2 roll translate 45 rotate\n\
    0 0 3 -1 roll Pl\n  grestore\n\
} def\n\n\
% Plus\n\
/Pl {\n  % Stack: x y symbolSize\n\
  gsave\n    3 -2 roll translate\n",
"    dup 2 idiv\n\
    exch 6 idiv\n\n\
    %\n\
    %          2   3		The plus/cross symbol is a\n\
    %				closed polygon of 12 points.\n\
    %      0   1   4    5	The diagram to the left\n\
    %           x,y		represents the positions of\n\
    %     11  10   7    6	the points which are computed\n\
    %				below.\n\
    %          9   8\n\
    %\n\n\
    newpath\n\
    2 copy exch neg exch neg moveto dup neg dup lineto\n\
    2 copy neg exch neg lineto 2 copy exch neg lineto\n\
    dup dup neg lineto 2 copy neg lineto 2 copy lineto\n\
    dup dup lineto 2 copy exch lineto 2 copy neg exch lineto\n\
    dup dup neg exch lineto exch neg exch lineto\n\
    closepath\n\
    DrawSymbolProc\n\
  grestore\n\
} def\n",
"\n\
% Circle\n\
/Ci {\n  % Stack: x y symbolSize\n\
  3 copy pop\n  moveto newpath\n  2 div 0 360 arc\n\
  closepath DrawSymbolProc\n} def\n\n\
% Square\n\
/Sq {\n  % Stack: x y symbolSize\n\
  dup dup 2 div dup\n  6 -1 roll exch sub exch\n\
  5 -1 roll exch sub 4 -2 roll Box\n  DrawSymbolProc\n} def\n\n\
% Line\n\
/Li {\n  % Stack: x y symbolSize\n\
  3 1 roll exch 3 -1 roll 2 div 3 copy\n  newpath\n\
  sub exch moveto add exch lineto\n  stroke\n} def\n\n\
% Diamond\n\
/Di {\n  % Stack: x y symbolSize\n\
  gsave\n    3 1 roll translate 45 rotate 0 0 3 -1 roll Sq\n\
  grestore\n} def\n    \n\
% Triangle\n\
/Tr {\n  % Stack: x y symbolSize\n\
  gsave\n    3 -2 roll translate\n",
"    BaseRatio mul 0.5 mul		% Calculate 1/2 base\n\
    dup 0 exch 30 cos mul		% h1 = height above center point\n\
    neg					% b2 0 -h1\n\
    newpath moveto			% point 1;  b2\n\
    dup 30 sin 30 cos div mul		% h2 = height below center point\n\
    2 copy lineto			% point 2;  b2 h2\n\
    exch neg exch lineto		% \n\
    closepath\n\
    DrawSymbolProc\n\
  grestore\n\
} def\n\
%%BeginSetup\n\
gsave					% Save the graphics state\n\n\
% Default line style parameters\n\n\
1 setlinewidth				% width\n\
1 setlinejoin				% join\n\
0 setlinecap				% cap\n\
[] 0 setdash				% dashes\n\n",
NULL
};

#endif /* !_BLTGRPS_H */
