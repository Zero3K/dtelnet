/* termwin.c */

#include <string.h>
#include <windows.h>

#include "termwin.h"
#include "term.h"

WindowLineNo linenoBufferToWindow (BufferLineNo blineno)
{
    WindowLineNo wlineno= blineno - term.topVisibleLine;
    return wlineno;
}

BufferLineNo linenoWindowToBuffer (WindowLineNo wlineno)
{
    BufferLineNo blineno= wlineno + term.topVisibleLine;
    return blineno;
}

TerminalLineNo linenoBufferToTerminal (BufferLineNo blineno)
{
    BufferLineNo btermfirst;
    TerminalLineNo tlineno;

    btermfirst= term.linebuff.used - term.winSize.cy;
    if (btermfirst<0) btermfirst= 0;
    tlineno= blineno - btermfirst;
    return tlineno;
}

BufferLineNo linenoTerminalToBuffer (TerminalLineNo tlineno)
{
    BufferLineNo btermfirst;
    BufferLineNo blineno;

    btermfirst= term.linebuff.used - term.winSize.cy;
    if (btermfirst<0) btermfirst= 0;
    blineno= tlineno + btermfirst;
    return blineno;
}

TerminalLineNo linenoWindowToTerminal (WindowLineNo wlineno)
{
    BufferLineNo btermfirst;
    TerminalLineNo tlineno;

    btermfirst= term.linebuff.used - term.winSize.cy;
    if (btermfirst<0) btermfirst= 0;
    tlineno= wlineno + term.topVisibleLine - btermfirst;
    return tlineno;
}

WindowLineNo linenoTerminalToWindow (TerminalLineNo tlineno)
{
    BufferLineNo btermfirst;
    WindowLineNo wlineno;

    btermfirst= term.linebuff.used - term.winSize.cy;
    if (btermfirst<0) btermfirst= 0;
    wlineno= tlineno + btermfirst - term.topVisibleLine;
    return wlineno;
}

TerminalLineNo linenoServerCursorToTerminal (ServerCursorLineNo slineno)
{
    LineNo tmp;
    TerminalLineNo tlineno;

    tmp= slineno;
    if (tmp>0) --tmp;
    if (term.scroll.fRestrict && !term.scroll.fFullScreen) {
	tlineno = tmp + term.scroll.lines.top;
    } else {
	tlineno = tmp;
    }
    return tlineno;
}

ServerCursorLineNo linenoTerminalToServerCursor (TerminalLineNo tlineno)
{
    ServerCursorLineNo slineno;

    if (term.scroll.fRestrict && !term.scroll.fFullScreen) {
	if (tlineno < term.scroll.lines.top) { /* huh?! this shouldn't happen */
	    slineno = 1;
	} else {
	    slineno = tlineno - term.scroll.lines.top + 1;
	}
    } else {
	slineno = tlineno + 1;
    }
    return slineno;
}

/* rangeToRect:
	create a rectangle that covers the given Range

   note:
	remember: from.to is not included in the Range;
	 to.right and to.bottom are not included in the RECT

   examples:
	(from=(y=3,x=10),to=(y=3,x=12)) --> (top=3,left=10,bottom=4,right=12)
	(from=(y=3,x=10),to=(y=4,x=3))  --> (top=3,left=10,bottom=5,right=<linelen>)
 */

void rangeToRect (const Range *from, RECT *to)
{
    if (rangeIsEmpty (from)) {			/* empty 'from' */
	memset (to, 0, sizeof (*to));

    } else if (from->from.y == from->to.y) {	/* single line */
	to->top=    from->from.y;
	to->bottom= from->from.y+1;
	to->left=   from->from.x;
	to->right=  from->to.x;

    } else {					/* multiple lines */
	to->top=    from->from.y;
	to->bottom= from->to.y+1;
	to->left=   0;
	to->right=  term.winSize.cx;
    }
}

/* rectUnion:
	combine two rectangles

   example:
	(l=20,t=5,r=35,b=7)+(l=21,t=6,r=30,b=7) -> (l=20,t=5,r=35,b=7)
 */
void rectUnion (const RECT *r1, const RECT *r2, RECT *into)
{
    RECT tmp= *r1;

    if (r2->left   < tmp.left)   tmp.left=   r2->left;
    if (r2->right  > tmp.right)  tmp.right=  r2->right;
    if (r2->top    < tmp.top)    tmp.top=    r2->top;
    if (r2->bottom > tmp.bottom) tmp.bottom= r2->bottom;

    *into= tmp;
}

BOOL rectSection (const RECT *r1, const RECT *r2, RECT *into)
{
    RECT tmp= *r1;
    BOOL fNonEmpty;

    if (r2->left   > tmp.left)   tmp.left=   r2->left;
    if (r2->right  < tmp.right)  tmp.right=  r2->right;
    if (r2->top    > tmp.top)    tmp.top=    r2->top;
    if (r2->bottom < tmp.bottom) tmp.bottom= r2->bottom;

    if (into) *into= tmp;
    fNonEmpty= (tmp.left < tmp.right) && tmp.top < tmp.bottom;
    return fNonEmpty;
}

void rectWindowToBuffer (const WindowRect *from, BufferRect *to)
{
    to->left=   from->left;
    to->top=    linenoWindowToBuffer (from->top);
    to->right=  from->right;
    to->bottom= linenoWindowToBuffer (from->bottom);
}

void rectBufferToWindow (const BufferRect *from, WindowRect *to)
{
    to->left=   from->left;
    to->top=    linenoBufferToWindow (from->top);
    to->right=  from->right;
    to->bottom= linenoBufferToWindow (from->bottom);
}

int pointCmp (const POINT *p, const POINT *q)
{
    int ncmp= 0;

    if (!ncmp) ncmp= (p->y > q->y) - (p->y < q->y);
    if (!ncmp) ncmp= (p->x > q->x) - (p->x < q->x);

    return ncmp;
}

void pointMin (const POINT *p1, const POINT *p2, POINT *qmin)
{
    POINT tmp;

    if (pointCmp (p1, p2) <= 0) tmp= *p1;
    else			tmp= *p2;

    *qmin= tmp;
}

void pointMax (const POINT *p1, const POINT *p2, POINT *qmax)
{
    POINT tmp;

    if (pointCmp (p1, p2) >= 0) tmp= *p1;
    else			tmp= *p2;

    *qmax= tmp;
}

int rangeIsEmpty (const Range *r) /* from>=to */
{
    int cmp;
    cmp= pointCmp (&r->from, &r->to);

    return cmp>=0;
}

void rangeUnion (const Range *r1, const Range *r2, Range *into)
{
    Range tmp;

    if (r1->from.y < r2->from.y ||
	(r1->from.y == r2->from.y && r1->from.x <= r2->from.x)) {
	tmp.from= r1->from;
    } else {
	tmp.from= r2->from;
    }

    if (r1->to.y  >  r2->to.y ||
	(r1->to.y == r2->to.y && r1->to.x >= r2->to.x)) {
	tmp.to= r1->to;
    } else {
	tmp.to= r2->to;
    }

    *into= tmp;
}

/* rangeDistinct returns TRUE, if
   the two range doesn't have common elements
   (it's always TRUE, if at least one of them is empty)
 */
BOOL rangeDistinct (const Range *r1, const Range *r2)
{
    return 
	pointCmp (&r1->to, &r2->from)<=0 || 
	pointCmp (&r2->to, &r1->from)<=0 ||
	rangeIsEmpty (r1) ||
	rangeIsEmpty (r2);
}

/* rangeOverlap returns TRUE, if
   the two range have common elements,
   or if the first is empty, and it is inside the second
   Warnings: 
	1. asymmetrical parameter usage
	2. _not_ the inverse of rangeDistinct
 */
BOOL rangeOverlap (const Range *r1, const Range *r2)
{
    return
	pointCmp (&r1->from, &r2->to)<0 &&
	pointCmp (&r1->to,   &r2->from)>=0 &&
	! rangeIsEmpty (r2);
}

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
int rangeSplit (const Range *pbread, const Range *pknife
	, Range *before, Range *under, Range *after)
{
/* arguments might overlap, or outputs can be NULL -- be careful */
    const Range mybread= *pbread, *bread= &mybread;
    const Range myknife= *pknife, *knife= &myknife;
    Range myb, myu, mya;
    int retval;

    if (!before) before= &myb;
    if (!under)  under= &myu;
    if (!after)  after= &mya;

    if (rangeDistinct (bread, knife)) {
	*before= *bread;
	under->from= bread->to;
	under->to=   bread->to;
	*after= *under;
	retval= 1;
	goto RETURN;
    }

    before->from= bread->from;
    after->to= bread->to;
    retval= 2;

    if (pointCmp (&knife->from, &bread->from)>0) {
	before->to= under->from= knife->from;
	retval |= 1;
    } else {
	before->to= under->from= bread->from;
	/* 'before' is empty */
    }

    if (pointCmp (&knife->to, &bread->to)<0) {
	after->from= under->to= knife->to;
	retval |= 4;
    } else {
	after->from= under->to= bread->to;
	/* 'after' is empty */
    }

RETURN:
    return retval;
}

BOOL rangeContainsPoint (const Range *r, const POINT *p)
{
    int cmp1, cmp2;
    BOOL retval;

    cmp1= pointCmp (p, &r->from);
    cmp2= pointCmp (p, &r->to);
    retval= cmp1>=0 && cmp2<0;
    return retval;
}

void pointPixelToWindow (const PixelPoint *pfrom, WindowPoint *pto, PixelPoint *mod)
{
    PixelPoint from= *pfrom;
    WindowPoint to;

    to.x= (from.x - termMargin.left) / term.charSize.cx;
    to.y= (from.y - termMargin.top)  / term.charSize.cy;

    if (pto) *pto= to;

    if (mod) {
	mod->x= (from.x - termMargin.left) - to.x * term.charSize.cx;
	mod->y= (from.y - termMargin.top)  - to.y * term.charSize.cy;
    }
}

void pointWindowToPixel (const WindowPoint *from, PixelPoint *to)
{
    to->x= termMargin.left + from->x * term.charSize.cx;
    to->y= termMargin.top  + from->y * term.charSize.cy;
}

void pointWindowToTerminal (const WindowPoint *from, TerminalPoint *to)
{
    BufferPoint b;

    pointWindowToBuffer (from, &b);
    pointBufferToTerminal (&b, to);
}

void pointTerminalToWindow (const TerminalPoint *from, WindowPoint *to)
{
    BufferPoint b;

    pointTerminalToBuffer (from, &b);
    pointBufferToWindow (&b, to);
}

void pointServerCursorToTerminal (const ServerCursorPoint *from, TerminalPoint *to)
{
    LineNo tmp;

    to->y = linenoServerCursorToTerminal (from->y);

    tmp= from->x;
    if (tmp>0) --tmp;
    to->x = tmp;
}

void pointTerminalToServerCursor (const TerminalPoint *from, ServerCursorPoint *to)
{
    to->y = linenoTerminalToServerCursor (from->y);
    to->x = 1 + from->x;
}

void pointClipWindowPoint (const WindowPoint *pfrom, WindowPoint *to, int strict)
{
    WindowPoint from= *pfrom;

    *to= from;

    if (strict>0) {
	if (from.x<0) to->x= 0;
	if (from.y<0) to->y= 0;
	if (strict==1) {
	    if (from.x>term.winSize.cx) to->x= term.winSize.cx;
	    if (from.y>term.winSize.cy) to->y= term.winSize.cy;
	} else {
	    if (from.x>=term.winSize.cx) to->x= term.winSize.cx-1;
	    if (from.y>=term.winSize.cy) to->y= term.winSize.cy-1;
	}
    }
}

/* "Terminal" has the same size as "Window". For now. */
void pointClipTerminalPoint (const TerminalPoint *pfrom, TerminalPoint *to, int strict)
{
    TerminalPoint from= *pfrom;

    *to= from;

    if (strict>0) {
	if (from.x<0) to->x= 0;
	if (from.y<0) to->y= 0;
	if (strict==1) {
	    if (from.x>term.winSize.cx) to->x= term.winSize.cx;
	    if (from.y>term.winSize.cy) to->y= term.winSize.cy;
	} else {
	    if (from.x>=term.winSize.cx) to->x= term.winSize.cx-1;
	    if (from.y>=term.winSize.cy) to->y= term.winSize.cy-1;
	}
    }
}

void pointWindowToBuffer (const WindowPoint *from, BufferPoint *to)
{
    to->y = term.topVisibleLine + from->y;
    to->x = from->x;
}

void pointBufferToWindow (const BufferPoint *from, WindowPoint *to)
{
    to->y = from->y - term.topVisibleLine;
    to->x = from->x;
}

void pointBufferToTerminal (const BufferPoint *from, TerminalPoint *to)
{
    to->y = linenoBufferToTerminal (from->y);
    to->x = from->x;
}

void pointTerminalToBuffer (const TerminalPoint *from, BufferPoint *to)
{
    to->y = linenoTerminalToBuffer (from->y);
    to->x = from->x;
}

void rangeWindowToBuffer (const WindowRange *from, BufferRange *to)
{
    pointWindowToBuffer (&from->from, &to->from);
    pointWindowToBuffer (&from->to,   &to->to);
}

void rangeBufferToWindow (const BufferRange *from, WindowRange *to)
{
    pointBufferToWindow (&from->from, &to->from);
    pointBufferToWindow (&from->to,   &to->to);
}

void rangeTerminalToWindow (const TerminalRange *from, WindowRange *to)
{
    pointTerminalToWindow (&from->from, &to->from);
    pointTerminalToWindow (&from->to,   &to->to);
}

void rangeWindowToTerminal (const WindowRange *from, TerminalRange *to)
{
    pointWindowToTerminal (&from->from, &to->from);
    pointWindowToTerminal (&from->to,   &to->to);
}

void rangeTerminalToBuffer (const TerminalRange *from, BufferRange *to)
{
    pointTerminalToBuffer (&from->from, &to->from);
    pointTerminalToBuffer (&from->to,   &to->to);
}

void rangeBufferToTerminal (const BufferRange *from, TerminalRange *to)
{
    pointBufferToTerminal (&from->from, &to->from);
    pointBufferToTerminal (&from->to,   &to->to);
}

void intervalTerminalToWindow (const TerminalInterval *from, WindowInterval *to)
{
    to->top=    linenoTerminalToWindow (from->top);
    to->bottom= linenoTerminalToWindow (from->bottom);
}

void intervalWindowToTerminal (const WindowInterval *from, TerminalInterval *to)
{
    to->top=    linenoWindowToTerminal (from->top);
    to->bottom= linenoWindowToTerminal (from->bottom);
}

void intervalBufferToTerminal (const BufferInterval *from, TerminalInterval *to)
{
    to->top=    linenoBufferToTerminal (from->top);
    to->bottom= linenoBufferToTerminal (from->bottom);
}

void intervalBufferToWindow (const BufferInterval *from, WindowInterval *to)
{
    to->top=    linenoBufferToWindow (from->top);
    to->bottom= linenoBufferToWindow (from->bottom);
}

BOOL intervalHasCommonElements (const Interval *p, const Interval *q)
{
    BOOL fCommon;

    fCommon= (p->top < p->bottom) &&
	     (q->top < q->bottom) &&
	    ((q->top <= p->top && p->top < q->bottom) ||
	     (p->top <= q->top && q->top < p->bottom));
    return fCommon;
}

BOOL intervalIsNested (const Interval *egg, const Interval *nest)
{
    BOOL fNested;

    fNested= nest->top <= egg->top && egg->bottom <= nest->bottom;
    return fNested;
}

BOOL intervalIsEmpty (const Interval *p)
{
    return p->top >= p->bottom;
}

BOOL bufferLineNoAdjustAfterLinesRemove (BufferLineNo *np,
    BufferLineNo first, BufferLineNo ndel)
{
    BOOL retval;

    if (ndel<=0) {
	retval= TRUE;

    } else if (*np<first) {
	retval= TRUE;

    } else if (*np>=first+ndel) {
	retval= TRUE;
	*np -= ndel;

    } else {
	retval= FALSE;
	*np = 0;
    }

    return retval;
}

/* bufferPointAdjustAfterLinesRemove:
   calls the previous one; if the line in question doesn't exist after deletion,
   sets x and y to zero and returns FALSE
*/
BOOL bufferPointAdjustAfterLinesRemove (BufferPoint *bp,
    BufferLineNo first, BufferLineNo ndel)
{
    BOOL retval;

    retval= bufferLineNoAdjustAfterLinesRemove (&bp->y, first, ndel);
    if (!retval) bp->x= 0;
    return retval;
}

/* Analoguous pairs for insert: */

BOOL bufferLineNoAdjustAfterLinesInsert (BufferLineNo *np,
    BufferLineNo first, BufferLineNo nins)
{
    if (nins<=0) {
	/* no change */

    } else if (*np<first) {
	/* no change */

    } else {
	*np += nins;
    }

    return TRUE;
}

BOOL bufferPointAdjustAfterLinesInsert (BufferPoint *bp,
    BufferLineNo first, BufferLineNo nins)
{
    BOOL retval;

    retval= bufferLineNoAdjustAfterLinesInsert (&bp->y, first, nins);
/*  if (!retval) bp->x= 0;	 always TRUE! */

    return retval;
}

unsigned long rangeGetSize (const Range *r, Column ncol)
{
    long sum;

    if (rangeIsEmpty (r)) {
	sum= 0;
	goto RETURN;
    }
    sum  = (long)(r->to.y - r->from.y) * ncol;
    sum += r->to.x - r->from.x;

RETURN:
    if (sum<0) sum= 0;
    return (unsigned long)sum;
}

int rangeCompareSize (const Range *left, const Range *right, Column ncol)
{
    unsigned long sl, sr;
    int retval;

    sl= rangeGetSize (left,  ncol);
    sr= rangeGetSize (right, ncol);

    if      (sl>sr) retval=  1;
    else if (sl<sr) retval= -1;
    else	    retval=  0;

    return retval;
}
