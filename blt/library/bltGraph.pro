%
% PostScript prolog file of the BLT graph widget.
%
% Copyright 1989-1992 Regents of the University of California.
% Permission to use, copy, modify, and distribute this
% software and its documentation for any purpose and without
% fee is hereby granted, provided that the above copyright
% notice appear in all copies.  The University of California
% makes no representations about the suitability of this
% software for any purpose.  It is provided "as is" without
% express or implied warranty.
%
% Copyright 1991-1997 by AT&T Bell Laboratories.
% Permission to use, copy, modify, and distribute this software
% and its documentation for any purpose and without fee is hereby
% granted, provided that the above copyright notice appear in all
% copies and that both that the copyright notice and warranty
% disclaimer appear in supporting documentation, and that the
% names of AT&T Bell Laboratories any of their entities not be used
% in advertising or publicity pertaining to distribution of the
% software without specific, written prior permission.
%
% AT&T disclaims all warranties with regard to this software, including
% all implied warranties of merchantability and fitness.  In no event
% shall AT&T be liable for any special, indirect or consequential
% damages or any damages whatsoever resulting from loss of use, data
% or profits, whether in an action of contract, negligence or other
% tortuous action, arising out of or in connection with the use or
% performance of this software.
%

200 dict begin

/BaseRatio 1.3467736870885982 def	% Ratio triangle base / symbol size
/BgColorProc 0 def			% Background color routine (symbols)
/DrawSymbolProc 0 def			% Routine to draw symbol outline/fill
/StippleProc 0 def			% Stipple routine (bar segments)
/DashesProc 0 def			% Dashes routine (line segments)

/encoding {
  ISOLatin1Encoding
} def
systemdict /encodefont known {
  /realsetfont /setfont load def
  /setfont { 
    encoding encodefont realsetfont 
  } def
} if

/Stroke {
  gsave
    stroke
  grestore
} def

/Fill {
  gsave
    fill
  grestore
} def

/SetFont { 	
  % Stack: pointSize fontName
  findfont exch scalefont setfont
} def

/Box {
  % Stack: x y width height
  newpath
  exch 4 2 roll moveto
  dup 0 rlineto
  exch 0 exch rlineto
  neg 0 rlineto
  closepath
} def

/SetFgColor {
  % Stack: red green blue
  CL 0 eq { 
    pop pop pop 0 0 0 
  } if
  setrgbcolor
  CL 1 eq { 
    currentgray setgray 
  } if
} def

/SetBgColor {
  % Stack: red green blue
  CL 0 eq { 
    pop pop pop 1 1 1 
  } if
  setrgbcolor
  CL 1 eq { 
    currentgray setgray 
  } if
} def

% The next two definitions are taken from "$tk_library/prolog.ps"

% desiredSize EvenPixels closestSize
%
% The procedure below is used for stippling.  Given the optimal size
% of a dot in a stipple pattern in the current user coordinate system,
% compute the closest size that is an exact multiple of the device's
% pixel size.  This allows stipple patterns to be displayed without
% aliasing effects.

/EvenPixels {
  % Compute exact number of device pixels per stipple dot.
  dup 0 matrix currentmatrix dtransform
  dup mul exch dup mul add sqrt

  % Round to an integer, make sure the number is at least 1, and compute
  % user coord distance corresponding to this.
  dup round dup 1 lt {pop 1} if
  exch div mul
} bind def

% width height string filled StippleFill --
%
% Given a path and other graphics information already set up, this
% procedure will fill the current path in a stippled fashion.  "String"
% contains a proper image description of the stipple pattern and
% "width" and "height" give its dimensions.  If "filled" is true then
% it means that the area to be stippled is gotten by filling the
% current path (e.g. the interior of a polygon); if it's false, the
% area is gotten by stroking the current path (e.g. a wide line).
% Each stipple dot is assumed to be about one unit across in the
% current user coordinate system.

/StippleFill {
  % Turn the path into a clip region that we can then cover with
  % lots of images corresponding to the stipple pattern.  Warning:
  % some Postscript interpreters get errors during strokepath for
  % dashed lines.  If this happens, turn off dashes and try again.

  gsave
    {eoclip}
    {{strokepath} stopped {grestore gsave [] 0 setdash strokepath} if clip}
    ifelse

    % Change the scaling so that one user unit in user coordinates
    % corresponds to the size of one stipple dot.
    1 EvenPixels dup scale

    % Compute the bounding box occupied by the path (which is now
    % the clipping region), and round the lower coordinates down
    % to the nearest starting point for the stipple pattern.

    pathbbox
    4 2 roll
    5 index div cvi 5 index mul 4 1 roll
    6 index div cvi 6 index mul 3 2 roll

    % Stack now: width height string y1 y2 x1 x2
    % Below is a doubly-nested for loop to iterate across this area
    % in units of the stipple pattern size, going up columns then
    % across rows, blasting out a stipple-pattern-sized rectangle at
    % each position

    6 index exch {
      2 index 5 index 3 index {
	% Stack now: width height string y1 y2 x y

	gsave
	  1 index exch translate
	  5 index 5 index true matrix {3 index} imagemask
	grestore
      } for
      pop
    } for
    pop pop pop pop pop
  grestore
  newpath
} bind def

/Segment {	% Stack: x1 y1 x2 y2
  newpath 4 2 roll moveto lineto stroke
} def

/EndText {
  %Stack :
  grestore
} def

/BeginText {
  %Stack :  w h theta centerX centerY
  gsave
    % Translate the origin to the center of bounding box and rotate
    translate neg rotate
    % Translate back to the origin of the text region
    -0.5 mul exch -0.5 mul exch translate
} def

/DrawLine {
  %Stack : str strWidth x y
  moveto				% Go to the text position
  exch dup dup 4 2 roll

  % Adjust character widths to get desired overall string width
  % adjust X = (desired width - real width)/#chars

  stringwidth pop sub exch
  length div
  0 3 -1 roll

  % Flip back the scale so that the string is not drawn in reverse

  gsave
    1 -1 scale
    ashow
  grestore
} def

/DrawBitmap {
  % Stack: ?bgColorProc? boolean centerX centerY width height theta imageStr
  gsave
    6 -2 roll translate			% Translate to center of bounding box
    4 1 roll neg rotate			% Rotate by theta
    
    % Find upperleft corner of bounding box
    
    2 copy -.5 mul exch -.5 mul exch translate
    2 copy scale			% Make pixel unit scale
    newpath
    0 0 moveto 0 1 lineto 1 1 lineto 1 0 lineto
    closepath
    
    % Fill rectangle with background color
    
    4 -1 roll { 
      gsave 
	4 -1 roll exec fill 
      grestore 
    } if
    
    % Paint the image string into the unit rectangle
    
    2 copy true 3 -1 roll 0 0 5 -1 roll 0 0 6 array astore 5 -1 roll
    imagemask
  grestore
}def

% Symbols:

% Skinny-cross
/Sc {
  % Stack: x y symbolSize
  gsave
    3 -2 roll translate 45 rotate
    0 0 3 -1 roll Sp
  grestore
} def

% Skinny-plus
/Sp {
  % Stack: x y symbolSize
  gsave
    3 -2 roll translate
    2 idiv
    dup 2 copy
    newpath neg 0 moveto 0 lineto
    DrawSymbolProc
    newpath neg 0 exch moveto 0 exch lineto
    DrawSymbolProc
  grestore
} def

% Cross
/Cr {
  % Stack: x y symbolSize
  gsave
    3 -2 roll translate 45 rotate
    0 0 3 -1 roll Pl
  grestore
} def

% Plus
/Pl {
  % Stack: x y symbolSize
  gsave
    3 -2 roll translate
    dup 2 idiv
    exch 6 idiv

    %
    %          2   3		The plus/cross symbol is a
    %				closed polygon of 12 points.
    %      0   1   4    5	The diagram to the left
    %           x,y		represents the positions of
    %     11  10   7    6	the points which are computed
    %				below.
    %          9   8
    %

    newpath
    2 copy exch neg exch neg moveto dup neg dup lineto
    2 copy neg exch neg lineto 2 copy exch neg lineto
    dup dup neg lineto 2 copy neg lineto 2 copy lineto
    dup dup lineto 2 copy exch lineto 2 copy neg exch lineto
    dup dup neg exch lineto exch neg exch lineto
    closepath
    DrawSymbolProc
  grestore
} def

% Circle
/Ci {
  % Stack: x y symbolSize
  3 copy pop
  moveto newpath
  2 div 0 360 arc
  closepath DrawSymbolProc
} def

% Square
/Sq {
  % Stack: x y symbolSize
  dup dup 2 div dup
  6 -1 roll exch sub exch
  5 -1 roll exch sub 4 -2 roll Box
  DrawSymbolProc
} def

% Line
/Li {
  % Stack: x y symbolSize
  3 1 roll exch 3 -1 roll 2 div 3 copy
  newpath
  sub exch moveto add exch lineto
  stroke
} def

% Diamond
/Di {
  % Stack: x y symbolSize
  gsave
    3 1 roll translate 45 rotate 0 0 3 -1 roll Sq
  grestore
} def
    
% Triangle
/Tr {
  % Stack: x y symbolSize
  gsave
    3 -2 roll translate
    BaseRatio mul 0.5 mul		% Calculate 1/2 base
    dup 0 exch 30 cos mul		% h1 = height above center point
    neg					% b2 0 -h1
    newpath moveto			% point 1;  b2
    dup 30 sin 30 cos div mul		% h2 = height below center point
    2 copy lineto			% point 2;  b2 h2
    exch neg exch lineto		% 
    closepath
    DrawSymbolProc
  grestore
} def
%%BeginSetup
gsave					% Save the graphics state

% Default line style parameters

1 setlinewidth				% width
1 setlinejoin				% join
0 setlinecap				% cap
[] 0 setdash				% dashes

