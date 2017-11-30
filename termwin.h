/* termwin.h
 * Copyright (c) 1997 David Cole
 *
 * Manage the terminal window line array
 */
#ifndef __termwin_h
#define __termwin_h

/* Example for Buffer, Terminal and Window:

						Relevant variables:
	+--0-------[Buffer-125lines]-----+
	|  1 every data (including	 |
	|  2 history) we store	 	 |
	|  3 				 |
	|... 				 |
	| 34         +------------------+|	term.select
	| 35  +------+	                ||
	| 36  | select range   	     +--+|
	| 37  +----------------------+	 |
	|...				 |
	| 60  +-0--[Window-25lines]-----+|	term.topVisibleLine
	| 61  | 1 what the user sees,	||
	| 62  | 2 often, but not always,||
	| 63  | 3 equals to Terminal	||
	|...  |				||
	| 84  +24-----------------------+|	+ term.winSize.cy - 1
	|...				 |
	|100  +-0--[Terminal-25lines]---+|	min (term.linebuff.used - term.winSize.cy, 0)
	|101  | 1 what the server knows	||
	|102  | 2 about,		||
	|103  | 3 often, but not always,||
	|104  | 4 equals to Window	||
	|...  |				||
	|115  |+---[Scroll Range]------+||	term.scroll
	|116  || (often the whole      |||
	|117  ||  Terminal)	       |||
	|118  |+-----------------------+||
	|...  |				||
	|...  |				||
	+124--+24-----------------------++	term.linebuff.used

 */
/* Note: POINT is predefined by Windows,
   but differently in Win16 and Win32 */

#ifdef _WIN32
    typedef LONG LineNo;
    typedef LONG NLines;
    typedef LONG Column;
    typedef LONG NColumns;
    typedef LONG NChars;
    #define LINENODEF(typebase)		\
    typedef LONG typebase##LineNo
#else
    typedef int LineNo;
    typedef int NLines;
    typedef int Column;
    typedef int NColumns;
    typedef int NChars;
    #define LINENODEF(typebase)		\
    typedef int typebase##LineNo
#endif

#define POINTDEF(typebase)		\
typedef struct typebase##Point {	\
    Column x;				\
    typebase##LineNo y;			\
} typebase##Point

				/* Name		 min max        unit  refers to	*/
LINENODEF (Pixel);		/* PixelLineNo     0 windowsize pixel the window the user sees */
LINENODEF (Window);		/* WindowLineNo    0 windowsize char  the window the user sees */
LINENODEF (Buffer);		/* BufferLineNo    0 -          char  the lines stored in term.linebuff */
LINENODEF (Terminal);		/* TerminalLineNo  0 windowsize char  the lines of the terminal */
LINENODEF (ServerCursor);	/* ServerLineNo    1 windowsize char  either the lines of the terminal, or the scroll area */

WindowLineNo	   linenoBufferToWindow (BufferLineNo blineno);
BufferLineNo	   linenoWindowToBuffer (WindowLineNo wlineno);
TerminalLineNo	   linenoBufferToTerminal (BufferLineNo blineno);
BufferLineNo	   linenoTerminalToBuffer (TerminalLineNo tlineno);
TerminalLineNo     linenoWindowToTerminal (WindowLineNo wlineno);
WindowLineNo	   linenoTerminalToWindow (TerminalLineNo tlineno);
TerminalLineNo	   linenoServerCursorToTerminal (ServerCursorLineNo slineno);
ServerCursorLineNo linenoTerminalToServerCursor (TerminalLineNo tlineno);

/* 0-based pixel coordinates: */
POINTDEF (Pixel);		/* y is window-based */

/* 0-based character coordinates: */
POINTDEF (Window);		/* y is window-based (cf term.winSize) */
POINTDEF (Buffer);		/* y is linebuff-based (cf term.topVisibleLine) */
POINTDEF (Terminal);		/* y is terminal-based (FIXME) */

/* terminal or scroll-region relative, 1-based character coordinates: */
POINTDEF (ServerCursor);
/* that's what receive in ESC [ row; col H;
   we convert it to TerminalPoint at once
 */

int pointCmp (const POINT *p, const POINT *q);

#define bufferPointCmp(p,q) pointCmp ((const POINT *)(p), (const POINT *)(q))

void pointMin (const POINT *p1, const POINT *p2, POINT *qmin);
void pointMax (const POINT *p1, const POINT *p2, POINT *qmax);

#define bufferPointMin(p1,p2,q) pointMin ((const POINT *)(p1),\
    (const POINT *)(p2), (POINT *)(q))
#define bufferPointMax(p1,p2,q) pointMax ((const POINT *)(p1),\
    (const POINT *)(p2), (POINT *)(q))

/* conversion function
	(values for the examples: charSize: cy=12,cx=9; winSize: cy=25,cx=80):

   pointPixelToWindow:
	returns TWIN_OUT_* (bit-combination)
	examples:
	(y=70,x=80)  -> (y=5,x=8), mod=(y=10,x=8)
	(y=-70,x=80) -> (y=-6,x=8),mod=(y=2,x=8)

   pointClipWindowPoint:
	returns TWIN_OUT_* (bit-combination)
	examples:
	(y<=-1,x<=-1,strict>=1) -> (y=0,x=0)
	(y>=26,x>=81,strict==1) -> (y=25,x=80)
	(y>=25,x>=80,strict==2) -> (y=24,x=79)
 */

void pointPixelToWindow  (const PixelPoint *from, WindowPoint *to, PixelPoint *mod);
void pointWindowToPixel  (const WindowPoint *from, PixelPoint *to);
void pointWindowToBuffer (const WindowPoint *from, BufferPoint *to);
void pointBufferToWindow (const BufferPoint *from, WindowPoint *to);
void pointBufferToTerminal (const BufferPoint *from, TerminalPoint *to);
void pointTerminalToBuffer (const TerminalPoint *from, BufferPoint *to);
void pointServerCursorToTerminal (const ServerCursorPoint *from, TerminalPoint *to);
void pointTerminalToServerCursor (const TerminalPoint *from, ServerCursorPoint *to);
void pointWindowToTerminal (const WindowPoint *from, TerminalPoint *to);
void pointTerminalToWindow (const TerminalPoint *from, WindowPoint *to);

void pointClipWindowPoint   (const WindowPoint *from, WindowPoint *to, int strict);
void pointClipTerminalPoint (const TerminalPoint *pfrom, TerminalPoint *to, int strict);

#define TWIN_OUT_LEFT   1
#define TWIN_OUT_RIGHT  2
#define TWIN_OUT_X      3
#define TWIN_OUT_TOP    4
#define TWIN_OUT_BOTTOM 8
#define TWIN_OUT_Y     12

/* bufferLineNoAdjustAfterLinesRemove:
   when deleting lines from the Buffer, line-numbers have to be changed
   return value is TRUE, if the line in question still exists after deletion
*/
BOOL bufferLineNoAdjustAfterLinesRemove (BufferLineNo *np,
    BufferLineNo first, BufferLineNo ndel);

/* bufferPointAdjustAfterLinesRemove:
   calls the previous one; if the line in question doesn't exist after deletion,
   sets x and y to zero and returns FALSE
*/
BOOL bufferPointAdjustAfterLinesRemove (BufferPoint *bp,
    BufferLineNo first, BufferLineNo ndel);

/* Analoguous pairs for insert:
    bufferLineNoAdjustAfterLinesInsert
    bufferPointAdjustAfterLinesInsert

   example of parameters:
	if a new line has been inserted at the very beginning,
	set 'first' to 0, 'nins' to 1

   return value: TRUE
 */

BOOL bufferLineNoAdjustAfterLinesInsert (BufferLineNo *np,
    BufferLineNo first, BufferLineNo nins);

BOOL bufferPointAdjustAfterLinesInsert (BufferPoint *bp,
    BufferLineNo first, BufferLineNo nins);

typedef struct Interval {
    LineNo top;
    LineNo bottom;
} Interval;

#define INTERVALDEF(typebase)		\
typedef struct typebase##Interval {	\
    typebase##LineNo top;		\
    typebase##LineNo bottom;		\
} typebase##Interval

INTERVALDEF (Terminal);		/* TerminalInterval: eg: scrolling region */
INTERVALDEF (Window);		/* WindowInterval */
INTERVALDEF (Buffer);		/* BufferInterval:   eg: selection */

void intervalTerminalToWindow (const TerminalInterval *from, WindowInterval *to);
void intervalWindowToTerminal (const WindowInterval *from, TerminalInterval *to);
void intervalBufferToTerminal (const BufferInterval *from, TerminalInterval *to);
void intervalBufferToWindow (const BufferInterval *from, WindowInterval *to);

BOOL intervalIsEmpty	       (const Interval *p);
BOOL intervalHasCommonElements (const Interval *p, const Interval *q);
BOOL intervalIsNested          (const Interval *egg, const Interval *nest);

/* Note: RECT is predefined by Windows, 
   fields: left, top, right, bottom
   left and top are included, right and top are not,
   eg: (left=50,top=100,right=51,bottom=101) means 1x1 pixel at y=100,x=50
 */

/* Note: RECT is predefined by Windows,
   but differently in Win16 and Win32 */

#define RECTDEF(typebase)		\
typedef struct typebase##Rect {	\
    Column left;		  	\
    typebase##LineNo top;	  	\
    Column right;		  	\
    typebase##LineNo bottom;  	\
} typebase##Rect

#define EmptyRect {0, 0, 0, 0}

RECTDEF (Pixel);		/* pixel-coordinates (y is window-based) */
RECTDEF (Window);		/* character-coordinates; y is window-based (cf term.winSize) */
RECTDEF (Buffer);		/* character-coordinates; y is linebuff-based (cf term.topVisibleLine) */
RECTDEF (Terminal);		/* character-coordinates; y is terminal-based */

void rectWindowToBuffer (const WindowRect *from, BufferRect *to);
void rectBufferToWindow (const BufferRect *from, WindowRect *to);

#define RANGEDEF(typebase)				\
typedef struct typebase##Range {			\
    typebase##Point from; /* start (included) */	\
    typebase##Point to;	  /* end (not included) */	\
} typebase##Range

typedef struct Range {
    POINT from;		/* start (included) */
    POINT to;		/* end (not included) */
} Range;

#define EmptyRange {{0,0},{0,0}}

RANGEDEF (Window);	/* WindowRange: window-based y-coordinates (cf term.winSize) */
RANGEDEF (Buffer);	/* BufferRange: buffer-based y-coordinates (cf term.topVisibleLine) */
RANGEDEF (Terminal);	/* TerminalRange: terminal-based y-coordinates */

int rangeIsEmpty (const Range *r); /* from>=to */

/* rangeDistinct returns TRUE, if
   the two range doesn't have common elements
   (it's always TRUE, if at least one of them is empty)
 */
BOOL rangeDistinct (const Range *r1, const Range *r2);

/* rangeOverlap returns TRUE, if
   the two range have common elements,
   or if the first is empty, and it is inside the second
   Warnings:
	1. asymmetrical parameter usage
	2. _not_ the inverse of rangeDistinct

   Examples:
	r1=[3,3) r2=[4,5) -> FALSE
	r1=[4,4) r2=[4,5) -> TRUE
	r1=[5,5) r2=[4,5) -> FALSE
	r1=[4,4) r2=[4,4) -> FALSE (r2 is empty)
 */
BOOL rangeOverlap (const Range *r1, const Range *r2);

/* rangeSplit: splits its first 'Range' parameter into three parts,
   depending on the second 'Range' parameter:
	before, under and after it.
   If the two ranges don't have common elements,
   only one value is returned in 'before'

   Return value is a bitmask:
	1/0 means 'before' is non-empty/empty
	2/0 means 'under' is non-empty/empty
	4/0 means 'after' is non-empty/empty

   Examples:
	bread=[5,8), knife=[6,7) -> 7: [5,6),[6,7),[7,8)
	bread=[5,8), knife=[5,7) -> 6: [5,5),[5,7),[7,8) ('before' is empty)
	bread=[5,8), knife=[5,8) -> 2: [5,5),[5,8),[8,8) ('before' and 'after' is empty)
	bread=[5,8), knife=[4,9) -> the same as the previous one
	bread=[5,8), knife=[4,5) -> 1: [5,8),[8,8),[8,8) ('under' and 'after' is empty)
	bread=[5,8), knife=[8,9) -> the same as the previous one 
	bread=[5,8), knife=[6,6) -> the same as the previous ones (knife is empty!)
 */
int rangeSplit (const Range *bread, const Range *knife
	, Range *before, Range *under, Range *after);

BOOL rangeContainsPoint (const Range *i, const POINT *p);
#define bufferRangeContainsPoint(r,p) rangeContainsPoint ((const Range *)(r), (const POINT *)(p))

void rangeUnion (const Range *r1, const Range *r2, Range *into);
#define bufferRangeUnion(r1,r2,into) \
{ \
    const BufferRange *tmp1= (r1); \
    const BufferRange *tmp2= (r2); \
    const BufferRange *tmpo= (into); \
    rangeUnion ((Range *)tmp1, (Range *)tmp2, (Range *)tmpo); \
}

void rangeWindowToBuffer (const WindowRange *from, BufferRange *to);
void rangeBufferToWindow (const BufferRange *from, WindowRange *to);
void rangeTerminalToWindow (const TerminalRange *from, WindowRange *to);
void rangeWindowToTerminal (const WindowRange *from, TerminalRange *to);
void rangeTerminalToBuffer (const TerminalRange *from, BufferRange *to);
void rangeBufferToTerminal (const BufferRange *from, TerminalRange *to);

/* rangeGetSize: the size of a range: ncol*(y2-y1) + (x2-x1)
   example
	ncol=10,r= ((y=4,x=7),(y=4,x=9)) -- two elements
		r= ((y=8,x=9),(y=9,x=2)) -- two elements (ncol matters!)
   note:
       if *.x < 0 or *.x > ncol, the result will be unusable (GIGO principle)
*/
unsigned long rangeGetSize (const Range *r, Column ncol);

/* rangeCompareSize:
    compare the size of the ranges,
    uses rangeGetSize

    example:
	ncol=10,left= ((y=4,x=7),(y=4,x=9)) -- two elements
		right=((y=8,x=9),(y=9,x=2)) -- two elements (ncol matters!)
		return=0 (equal)
*/
int rangeCompareSize (const Range *left, const Range *right, Column ncol);

#define bufferRangeCompareSize(l,r,n)\
    rangeCompareSize ((const Range *)(l),(const Range *)(r),(n))

/* rangeToRect:
	create a rectangle that covers the given Range

   note:
	remember: from.to is not included in the Range;
	 to.right and to.bottom are not included in the RECT

   examples:
	(from=(y=3,x=10),to=(y=3,x=12)) --> (top=3,left=10,bottom=4,right=12)
	(from=(y=3,x=10),to=(y=4,x=3))  --> (top=3,left=10,bottom=5,right=<linelen>)
 */

void rangeToRect (const Range *from, RECT *to);

#define bufferRangeToBufferRect(from,to)\
{ const BufferRange *tmp_from= (from);	\
  BufferRect *tmp_to= (to);		\
  rangeToRect ((const Range *)tmp_from, (RECT *)tmp_to);	\
}
#define windowRangeToWindowRect(from,to)\
{ const WindowRange *tmp_from= (from);	\
  WindowRect *tmp_to= (to);		\
  rangeToRect ((const Range *)tmp_from, (RECT *)tmp_to);	\
}

/* rectUnion:
	combine two rectangles

   example:
	(l=20,t=5,r=35,b=7)+(l=21,t=6,r=30,b=7) -> (l=20,t=5,r=35,b=7)
 */
void rectUnion (const RECT *r1, const RECT *r2, RECT *into);
/* rectSection:
	return TRUE, if the section is not empty
 */
BOOL rectSection (const RECT *p, const RECT *q, RECT *s);

/* Return the line array index of the line at the top of the terminal */
#define winTerminalTopLine() linenoTerminalToBuffer(0)
/* Return a terminal line converted to a window line index */
#define winTerminalToWindow(ypos) linenoTerminalToWindow(ypos)

/* Move the caret to the specifed character position
 -- if visible */
void winCaretPos(const TerminalPoint *tpCaretPos);
/* Set the history scrollbar to indicate the visible part of the
 * line array */
void winSetScrollbar(void);
/* Force windows to update the part of the window we have changed */
void winUpdate(void);
/* The oldest line in the history buffer has been removed */
void winTopLineRemoved(void);

/* Add an Range/Rect (window-coorditates) to the terminal window update rect
 * special case: r==NULL: the whole visible part
 */
void termModifyRange (const TerminalRange *r);
void termModifyRect (const TerminalRect *r);

/* Some characters were inserted into a line */
void winCharsInsert(const TerminalPoint *tp);
/* Some characters were deleted from a line */
void winCharsDelete(const TerminalPoint *tp);

/* termLinesScroll: calls ScrollWindow
   numLines>0: scroll up numLines
   numLines<0: scroll down -numLines
 */
void termLinesScroll (const TerminalInterval *ti, int numLines);
void winLinesScroll  (const WindowInterval *wi, int numLines);

/* termMargin:

   'unused' space around the 'useful' space
   left:   0+  (usually 0, but for some contexts (antialiasing/ClearType/whatnot)
		it should be set to 1 (is that always enough?))
   top:    0   (for now)
   right:  0+  (window width  - termMargin.left)%charWidth
   bottom: 0+  (window height - termMargin.top)%charWidth
 */
extern PixelRect termMargin;

#ifdef _WIN32
    typedef LONG SIZE_CX;
    typedef LONG SIZE_CY;
#else
    typedef int SIZE_CX;
    typedef int SIZE_CY;
#endif

#define SIZEDEF(typebase)	\
typedef struct typebase##Size {	\
    SIZE_CX cx;			\
    SIZE_CY cy;			\
} typebase##Size

SIZEDEF(Pixel);		/* PixelSize */
SIZEDEF(Char);		/* CharSize */

typedef struct ColumnInterval {	/* interval: [left,right) */
    Column left;		/* included */
    Column right;		/* not included */
} ColumnInterval;

/* empty if left>=right */
#define columnIntervalIsEmpty(pic) ((pic)->left >= (pic)->right)

#endif
