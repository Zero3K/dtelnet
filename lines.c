/* lines.c
 * Copyright (c) 1997 David Cole
 *
 * Manage line array for terminal with history.  At the moment, only a
 * fixed size history is supported.
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "term.h"
#include "termwin.h"
#include "lines.h"
#include "emul.h"
#include "log.h"
#include "graphmap.h"
#include "uconv.h"

#if defined(_WIN32)
    #define DTC_CTYPE uint32_t
    #define DTC_VMASK (0x00fffffful)
    #define DTC_TMASK (0xff000000ul)
    #define DTC_SHIFT 24
#else
    #define DTC_CTYPE uint16_t
    #define DTC_VMASK (0x00fful)
    #define DTC_TMASK (0xff00ul)
    #define DTC_SHIFT 8
#endif

struct DtChar {
    DTC_CTYPE u;
};

#define DTC_FromByte(cs,charcode) \
    ((DTC_CTYPE)\
    ((((cs)&0xffu)<<DTC_SHIFT) |\
      ((unsigned char)(charcode))))
#define DTC_FromUnic(cs,charcode) \
    ((DTC_CTYPE)\
      ((((cs)&0xffu)<<DTC_SHIFT) |\
      ((charcode)&DTC_VMASK)))

#define DTC_GetCset(dtc) \
    (((DTC_CTYPE)(dtc))>>DTC_SHIFT)
#define DTC_GetCharCode(dtc) \
    (((DTC_CTYPE)(dtc))&DTC_VMASK)

/* mind you, BIG_ENDIAN is unlikely on Wintel platform */
#if defined (BIG_ENDIAN)
#define IntPtrAsCharPtr(ip) ((char *)(ip) + sizeof *(ip) - 1)
#else
#define IntPtrAsCharPtr(ip) (char *)ip
#endif

/* alloc 'list', fill with 'NULL' */
void linesAllocList (LineBuffer *l, int newalloc)
{
    Line **lp;

    if (newalloc <= l->allocated) return;
    lp = (Line **)xrealloc (l->list, sizeof (Line *)*newalloc);
    l->list = lp;
    memset (l->list + l->allocated, 0, sizeof (Line *)*(newalloc - l->allocated));
    l->allocated= newalloc;
}

void linesFreeList (LineBuffer *l)
{
    int i;

    for (i=0; i<l->allocated; ++i) {
	if (l->list[i]) {
	    free (l->list[i]);
	    l->list[i]= NULL;
	}
    }
    if (l->allocated) free (l->list);
    l->allocated= 0;
    l->used= 0;
    l->list= NULL;
}

/* Return the specified line
 */
Line* linesGetLine(BufferLineNo num)
{
    if (num < 0 || num >= term.linebuff.used)
	return NULL;
    ASSERT(term.linebuff.list != NULL);
    ASSERT(term.linebuff.list[num] != NULL);
    return term.linebuff.list[num];
}

/* initialize a single line */
void linesInit (Line *line)
{
    /* Initialise the line with zero length, no wrap, and current
     * color attributes.
     */
    ASSERT(line != NULL);
    line->len = 0;
    line->wrapped = 0;
    line->end_attr = linesEndAttr (getAttrColors ());
}

/* allocate and initialize a single line */
Line* linesAllocInit (Line **into)
{
    Line *line;

    line= (Line *)xmalloc (sizeof *line);
    line->text = (DtChar *)xmalloc (MaxLineLen * sizeof (DtChar));
    linesInit (line);

    if (into) *into= line;
    return line;
}

static Line *linesCopyOne (Line **pto, const Line *from)
{
    Line *to;

    if (pto && *pto) to= *pto;
    else	     to= linesAllocInit (pto);

    *to= *from;
    memcpy (to->text, from->text, MaxLineLen * sizeof (DtChar));

    return to;
}

LineNo linesAvailable (void)
{
    ASSERT (term.linebuff.used <= term.linebuff.allocated);
    ASSERT (term.linebuff.used <= term.maxLines);
    return term.maxLines - term.linebuff.used;
}

/* Allocate a Line.
 *
 * Return a pointer to the new line, initialised to 0 length.
 */
static Line* linesAlloc(void)
{
    Line* line;

    if (term.linebuff.used == term.maxLines) {
	/* We have filled our history.  We have to remove the oldest
	 * line to make room for the new line.
	 */
	ASSERT(term.linebuff.list != NULL);
	line = term.linebuff.list[0];
	term.linebuff.list[0]= NULL;
	ASSERT(line != NULL);
	linesInit (line);

	ASSERT(term.maxLines > 1);
	memmove(&term.linebuff.list[0], &term.linebuff.list[1],
		(term.maxLines - 1) * sizeof(*term.linebuff.list));
	term.linebuff.used--;
	/* Tell the terminal window that the top history line was
	 * removed.  This might cause a scroll.
	 */
	winTopLineRemoved();
    } else {
	/* Allocate a new line
	 */
	line = linesAllocInit (NULL);
    }

    return line;
}

static void linesDeleteOne (Line **from)
{
    if (from && *from) {
	Line *tmp;

	tmp= *from;
	*from= NULL;

	if (tmp->text) {
	    xfree (tmp->text);
	    tmp->text= NULL;
	}
	xfree (tmp);
    }
}

void linesDeleteOldestLines (LineNo nDel)
{
    LineNo i;

    ASSERT (term.linebuff.used <= term.linebuff.allocated);
    ASSERT (nDel > 0);
    ASSERT (nDel <= term.linebuff.used - term.winSize.cy);

/* free the first 'nDel' lines */
    for (i= 0; i<nDel; ++i) {
	linesDeleteOne (&term.linebuff.list[i]);
    }

/* move the remaining lines backwards */
    if (term.linebuff.used > nDel) {
	LineNo nMove;

	nMove= term.linebuff.used - nDel;
	memmove(&term.linebuff.list[0],
		&term.linebuff.list[nDel],
		nMove * sizeof(*term.linebuff.list));
    }

    term.linebuff.used -= nDel;

/* mark the last 'nDel' entries as unused */
    for (i= 0; i<nDel; ++i) {
	term.linebuff.list[term.linebuff.used + i] = NULL;
    }
}

void linesDeleteBottomLines (LineNo nDel)
{
    BufferLineNo i, lnMin, lnLim;

    ASSERT (term.linebuff.used <= term.linebuff.allocated);
    ASSERT (nDel>=0 && nDel<=term.linebuff.used);

    if (nDel==0) return;
    if (nDel>term.linebuff.used) nDel= term.linebuff.used;
    lnMin= term.linebuff.used - nDel;
    lnLim= term.linebuff.used;

    for (i= lnLim; --i >= lnMin; ) {
	linesDeleteOne (&term.linebuff.list[i]);
    }
    term.linebuff.used -= nDel;
}

/* linesNewLines:
 * Add zero or more new _initialized_ line(s) to the bottom of the line array.
 * It returns (if nDeletedLines!=NULL) the number of deleted lines,
 * so the caller be able to decrement 'term.topVisibleLine' and 'term.select.range'
 */
void linesNewLines (BufferLineNo nNewLines, BufferLineNo *pnDeletedLines)
{
    BufferLineNo nAvailable, i;
    BufferLineNo nDelFromHistory= 0;
    
    ASSERT (nNewLines>=0 && nNewLines<=term.maxLines);

    if (nNewLines<=0) goto RETURN;

    nAvailable= linesAvailable ();

    if (nNewLines > nAvailable) {
	nDelFromHistory= nNewLines - nAvailable;
	linesDeleteOldestLines (nDelFromHistory);
    }
    for (i=0; i<nNewLines; ++i) {
	linesAllocInit (&term.linebuff.list[term.linebuff.used++]);
    }
RETURN:
    if (pnDeletedLines) *pnDeletedLines= nDelFromHistory;
}

/* linesInsertArea
   Make space for some new pointers in the 'list' array
   example: linesInsertArea (npos=3, ncnt=2)
   before: used=6, allocated>=8(!),  list: index: 0 1 2 3 4 5
					   value: A B C D E F
   after:  used=8, allocated=(same), list: index: 0 1 2 3 4 5 6 7
					   value: A B C - - D E F
   here '-' represents NULL pointers

   Notes:
   1. ncnt==0 does nothing
   2. Make sure that before calling allocated >= used+ncnt, 0<=npos<=used
   3. Immediately after this function, you have to deal with the NULL values
      in 'list'
 */
void linesInsertArea (LineBuffer *buff, BufferLineNo npos, BufferLineNo ncnt)
{
    BufferLineNo nOldUsed, nmove;

    ASSERT (buff->used + ncnt <= buff->allocated &&
	    0<=npos && npos<=buff->used);
    if (!ncnt) goto RETURN;

    nOldUsed= buff->used;
    buff->used += ncnt;

    nmove= nOldUsed-npos;
    if (nmove) {
	memmove (&buff->list[npos+ncnt], &buff->list[npos], nmove*sizeof (buff->list[0]));
    }

    memset (&buff->list[npos], 0, ncnt*sizeof (buff->list[0]));

RETURN:;
}

/* linesDupArea

   Duplicates lines in the 'list' array

   example: linesMoveArea (nto=2, nfrom=5, ncnt=2)
   before:  used=8, allocated=8, list: index: 0 1 2 3 4 5 6 7
				       value: A B - - C D E F
   here '-' represents NULL pointers

   after:   used=8, allocated=8, list: index: 0 1 2 3 4 5 6 7
				       value: A B D E C D E F

   Notes:
   1. ncnt==0 does nothing, nor does nto==nfrom
   2. Make sure that before calling, allocated >= used &&
      0<=nfrom && nfrom+ncnt<=used &&
      0<=nto   && nto+ncnt<=used
   3. the output area may contain NULL-values (e.g. coming from linesInsertArea)
      they will be allocated.
 */
void linesDupArea (LineBuffer *buff,
	BufferLineNo nto, BufferLineNo nfrom, BufferLineNo ncnt)
{
    BufferLineNo ln, lnbeg, lnlim, lnstep;

    ASSERT (buff->used <= buff->allocated &&
	    nfrom+ncnt <= buff->used &&
	    nto  +ncnt <= buff->used);
    if (ncnt==0 || nto==nfrom) goto RETURN;

/* direction depends on the relation between 'nfom' and 'nto' */
    if (nto<nfrom) {
	lnbeg= ncnt-1;
	lnlim=  -1;
	lnstep= -1;
    } else {
	lnbeg= 0;
	lnlim= ncnt;
	lnstep= 1;
    }

    for (ln= lnbeg; ln != lnlim; ln += lnstep) {
	linesCopyOne (&buff->list[nto+ln], buff->list[nfrom+ln]);
    }

RETURN:;
}

/* Make sure that lines exist up to the specified line array index
 */
void linesCreateTo(BufferLineNo idx)
{
    if (idx >= term.maxLines)
	idx = term.maxLines - 1;

    while (term.linebuff.list[idx] == NULL)
	term.linebuff.list[term.linebuff.used++] = linesAlloc();
}

/* Fill a part of a line with an ASCII character + attribute. */
static void linesFill (Line *line, int x, int n, unsigned char c, Attrib attr)
{
    int i;

    if (n<=0) return;
    line->text[x].u = DTC_FromByte (DTCHAR_ASCII, c);
    for (i=0; i<n; ++i) {
	line->text[x+i] = line->text[x];
	line->attr[x+i] = attr;
    }
}

/* Insert a number of lines at the specified terminal line.
 *
 * Args:
 * posy -     the terminal line to insert lines before
 * bottom -   the bottom terminal line (handles scroll regions)
 * numLines - the number of lines to insert
 */
void linesInsert(int posy, int bottom, int numLines)
{
    /* Convert terminal lines to line array index
     */
    int posyIdx = linesTerminalToLine(posy);
    int bottomIdx = linesTerminalToLine(bottom);

    ASSERT(posy >= 0);
    ASSERT(bottom >= 1);
    ASSERT(posy < bottom);
    ASSERT(bottom <= term.winSize.cy);
    ASSERT(numLines > 0);
    ASSERT(posyIdx <= term.maxLines);
    ASSERT(bottomIdx <= term.maxLines);

    /* Make sure that we do not try to insert more lines than will fit
     * in the region.
     */
    if (posy + numLines > bottom)
	numLines = bottom - posy;

    /* Make that lines exist to the specified y position
     */
    linesCreateTo(posyIdx);

    /* Insert the blank lines
     */
    while (numLines-- > 0) {
	Line* line;

	if (term.linebuff.used >= bottomIdx) {
	    /* Take the line off the bottom and insert it
	     * at the current position.
	     */
	    line = term.linebuff.list[bottomIdx - 1];
	    ASSERT(line != NULL);
	    line->len = 0;
	    line->wrapped = 0;
	    line->end_attr = linesEndAttr (getAttrToStore ());

	} else
	    /* We need to create a new line to insert it
	     */
	    line = linesAlloc();

	/* If there are any lines at or after the insertion position,
	 * we need to move them down.
	 */
	if (posyIdx < bottomIdx - 1)
	    memmove(&term.linebuff.list[posyIdx + 1], &term.linebuff.list[posyIdx],
		    (bottomIdx - posyIdx - 1) * sizeof(*term.linebuff.list));
	term.linebuff.list[posyIdx] = line;
    }
}

/* Insert a number of blank characters in the specified terminal line.
 *
 * Args:
 * posy -     the terminal line to insert characters into
 * posx -     the column at which characters will be inserted
 * numChars - the number of characters to be inserted
 */
void linesCharsInsert (TerminalLineNo posy, Column posx, NChars numChars)
{
    /* Convert the terminal line to line array index
     */
    BufferLineNo posyIdx = linenoTerminalToBuffer (posy);
    Line* line;			/* line being modified */

    ASSERT(posy >= 0);
    ASSERT(posy < term.winSize.cy);
    ASSERT(posx >= 0);
    ASSERT(numChars > 0);
    ASSERT(posyIdx < term.maxLines);

    linesCreateTo(posyIdx);
    line = term.linebuff.list[posyIdx];
    ASSERT(line != NULL);

    /* Do not insert any characters if at end of line
     */
    if (line == NULL || posx >= line->len)
	return;

    /* Make sure that we do not insert past the right edge of the
     * window
     */
    if (numChars + posx > term.winSize.cx)
	numChars = term.winSize.cx - posx;
    if (numChars + line->len > term.winSize.cx)
	line->len = term.winSize.cx - numChars;

    /* Now insert the blank characters
     */
    memmove(line->text + posx + numChars,
	    line->text + posx,
	    sizeof(line->text[0])*(line->len - posx));
    memmove(line->attr + posx + numChars,
	    line->attr + posx,
	    sizeof(line->attr[0])*(line->len - posx));
    linesFill (line, posx, numChars, ' ', getAttrToStore());

    line->len += numChars;
}

/* Delete a number of lines at the specified terminal line.
 *
 * Args:
 * posy -     the terminal line to delete lines at
 * bottom -   the bottom terminal line (handle scroll regions)
 * numLines - the number of lines to delete
 */
void linesDelete(int posy, int bottom, int numLines)
{
    /* Convert terminal lines to line array index
     */
    int posyIdx = linesTerminalToLine(posy);
    int bottomIdx = linesTerminalToLine(bottom);

    ASSERT(posy >= 0);
    ASSERT(bottom >= 1);
    ASSERT(posy < bottom);
    ASSERT(bottom <= term.winSize.cy);
    ASSERT(numLines > 0);
    ASSERT(posyIdx <= term.maxLines);
    ASSERT(bottomIdx <= term.maxLines);

    /* Limit bottom line to the number of lines that have actually
     * been used.
     */
    if (bottomIdx > term.linebuff.used)
	bottomIdx = term.linebuff.used;

    /* Make sure that we do not try to delete more lines than are in
     * the region.
     */
    if (posy + numLines > bottom)
	numLines = bottom - posy;

    /* Make that lines exist to the specified y position
     */
    linesCreateTo(posyIdx);

    while (numLines--) {
	/* Take the top line, clear it, and rotate it to the bottom of
	 * the delete region.
	 */
	Line* line = term.linebuff.list[posyIdx];
	ASSERT(line != NULL);
	line->len = 0;
	line->wrapped = 0;
	line->end_attr = linesEndAttr (getAttrToStore ());

	/* If there are any lines used after the current point -
	 * shuffle them up.
	 */
	if (posyIdx < bottomIdx - 1)
	    memmove(&term.linebuff.list[posyIdx], &term.linebuff.list[posyIdx + 1],
		    (bottomIdx - 1 - posyIdx) * sizeof(*term.linebuff.list));
	term.linebuff.list[bottomIdx - 1] = line;
    }
}

/* Delete a number of characters in the specified terminal line.
 *
 * Args:
 * posy -     the terminal line to delete characters from
 * posx -     the column at which characters will be deleted
 * numChars - the number of characters to be deleted
 */
void linesCharsDelete (TerminalLineNo posy, Column posx, NChars numChars)
{
    /* Convert terminal line to line array index
     */
    BufferLineNo posyIdx = linenoTerminalToBuffer (posy);
    Line* line;			/* the line being modified */

    ASSERT(posy >= 0);
    ASSERT(posy < term.winSize.cy);
    ASSERT(posx >= 0);
    ASSERT(numChars > 0);
    ASSERT(posyIdx < term.maxLines);

    /* Make sure lines exist up to the one being modified
     */
    linesCreateTo(posyIdx);
    line = term.linebuff.list[posyIdx];

    /* Make sure that the line length does not extend past the right
     * edge of the window
     */
    if (line->len > term.winSize.cx)
	line->len = term.winSize.cx;

    /* Do not delete any characters if past end of line
     */
    if (posx >= line->len)
	return;
    /* Only delete characters up to the end of the line
     */
    if (numChars > line->len - posx)
	numChars = line->len - posx;

    memmove(line->text + posx,
	    line->text + posx + numChars,
	    sizeof (line->text[0])*(line->len - posx - numChars));
    memmove(line->attr + posx,
	    line->attr + posx + numChars,
	    sizeof (line->attr[0])*(line->len - posx - numChars));

    line->len -= numChars;
}

/* Clear all of the text in the terminal between two locations.
 *
 * Args:
 * y1/x1 - terminal line/column to clear from
 * y2/x2 - terminal line/column to clear to
 *
 * The from location is assumed to precede the to location.
 */
void linesClearRange (TerminalLineNo y1, Column x1, TerminalLineNo y2, Column x2)
{
    int lineIdx;		/* iterate over lines to be cleared */
    Line* line;			/* line currently being cleared */
    Attrib setattr;

    setattr= linesEndAttr (getAttrToStore ());

    /* The blank characters inherit only the colors, not the attributes (underline, bold, blink, inverse) */
    setattr &= (ATTR_FLAG_CLR | ATTR_FG_MASK | ATTR_BG_MASK);

    /* If start of clear operation is off bottom of terminal, we have
     * no work to do.
     */
    if (y1 >= term.winSize.cy)
	return;

    /* If end of clear operation is off bottom of terminal, set it to
     * the end of the last line on the terminal.
     */
    if (y2 >= term.winSize.cy) {
	y2 = term.winSize.cy - 1;
	x2 = term.winSize.cx;
    }

    ASSERT(x1 >= 0);
    ASSERT(x2 >= 0);
    ASSERT(y2 >= y1);

    /* Convert the starting line to line array index and make sure
     * lines exist to that point.
     */
    lineIdx = linesTerminalToLine(y1);
    linesCreateTo(linesTerminalToLine(y2));
    line = term.linebuff.list[lineIdx];
    ASSERT(line != NULL);

    if (y1 == y2) {
	/* Clearing text on single line
	 */
	ASSERT(x1 <= x2);
	if (x1 <= line->len) {
	    if (x2 >= line->len) {
		/* Cleared to end of line - redefine line length
		 */
		line->len = x1;
		line->wrapped = 0;
		line->end_attr = setattr;
	    } else {
		/* Cleared part of line
		 */
		linesFill (line, x1, x2-x1, ' ', setattr);
	    }
	}
	return;
    }

    /* Clearing more than one line - first line is from x1 to end of line
     * Note: Ideally, if x1>line->len, we could add spaces to the line 
      (with the previous end_attr, increasing the line->len to x1)
     */
    if (x1 > line->len) {
	linesFill (line, line->len, x1-line->len, ' ', line->end_attr);
    }
    line->len = x1;
    line->wrapped = 0;
    line->end_attr = setattr;

    ++lineIdx;
    ++y1;
    /* Totally clear all lines until y2
     */
    while (y1 < y2) {
	ASSERT(term.linebuff.list[lineIdx] != NULL);
	line= term.linebuff.list[lineIdx];
	line->len = 0;
	line->wrapped = 0;
	line->end_attr = setattr;
	++lineIdx;
	++y1;
    }
    /* Clearing last line - clear from start of line to x2
     */
    if (y1 == term.winSize.cy)
	return;
    line = term.linebuff.list[lineIdx];
    ASSERT(line != NULL);
    if (x2 >= line->len) {
	line->len = 0;
	line->wrapped = 0;
	line->end_attr = setattr;
    } else {
	ASSERT(x2 > 0);
	linesFill (line, 0, x2, ' ', setattr);
    }
}

/* Fill the screen with 'E' for alignment test
 */
void linesAlignTest (void)
{
    TerminalLineNo y;			/* iterate over lines to be cleared */
    Attrib attr= ATTR_FLAG_DEF_FG | ATTR_FLAG_DEF_BG;

    for (y = 0; y < term.winSize.cy; y++) {
	BufferLineNo lineIdx = linenoTerminalToBuffer (y);
	Line* line;

	linesCreateTo(lineIdx);
	line = term.linebuff.list[lineIdx];
	ASSERT(line != NULL);
	line->len = term.winSize.cx;
	line->wrapped = 0;
	linesFill (line, 0, term.winSize.cx, 'E', attr);
	line->end_attr = linesEndAttr (getAttrToStore());
    }
}

/* Set the character at the current cursor position.
 *
 * Args:
 * c - the character to set at the cursor position.
 */
/* For now: single-byte only */
void linesSetChar(int dtCharType, uint32_t c)
{
    int lineIdx;		/* cursor terminal line as line array index */
    Line* line;			/* line being modified */
    int posy = term.cursor.pos.y;
    int posx = term.cursor.pos.x;
    int i;

    /* If cursor off bottom of terminal, we have nothing to do
     */
    if (posy >= term.winSize.cy)
	return;

    /* Make sure lines up to the cursor location exist
     */
    lineIdx = linesTerminalToLine(posy);
    ASSERT(lineIdx < term.maxLines);
    linesCreateTo(lineIdx);
    line = term.linebuff.list[lineIdx];
    ASSERT(line != NULL);

    ASSERT(posx < MaxLineLen);
    if (posx >= line->len) {
	/* Add (posx - line->len) spaces with the attribute 'end_attr'
	 */
	for (i=line->len; i<posx; ++i) {
	    line->text[i].u = DTC_FromByte (DTCHAR_ASCII, ' ');
	    line->attr[i] = line->end_attr;
	}
	line->len = posx + 1;
    }

    /* Set the character
     */
    line->text[posx].u = DTC_FromUnic (dtCharType, c);
    line->attr[posx]   = getAttrToStore();

    /* Make sure that the line length does not extend past the right
     * edge of the window
     */
    if (line->len > term.winSize.cx)
	line->len = term.winSize.cx;
}

Attrib linesEndAttr (Attrib from)
{
    Attrib setattr;

    setattr= from & (Attrib)~ATTR_FLAG_UNDERLINE;
    return setattr;
}

DtChar *linesCharPos (const Line *line, Column col)
{
    ASSERT (line!=NULL && col>=0);

/* we allow pointer right after the last character (limit),
   but not above  */
    if (col > line->len) col= line->len;

    return &line->text[col];
}

/* special function for 'select.c' */
BOOL linesIsSpaceAtIndex (const Line *line, Column col)
{
    BOOL retval;

    ASSERT (line!=NULL && col>=0 && col<line->len);

    retval= DTC_GetCharCode (line->text[col].u) == ' ';
    return retval;
}

/* secure bytes at the end of line
   return pointer to the line end */
DtChar *linesSecureBytes (Line *line, int n)
{
    ASSERT (line!=NULL && n>=0 && n<=MaxLineLen-line->len);
    return &line->text[line->len];
}

/* linesGetChar: get some characters either single-bytes or utf16le (wchar_t)
 *
 * one or more calls of this function might form a 'session';
 *
 * before the very first call set into->len to 0
 *
 * after the very last call
 * *nChar=number of elements (char or wchar_t), *cf=DTCHAR_xxx
 */

void linesGetChars (int mode, const Line *ln, Column frompos, NColumns fromlen,
		    Buffer *into, int *cs)
{
    ColumnInterval ciFrom;

    ciFrom.left=  frompos;
    ciFrom.right= frompos + fromlen;
    linesGetCharsExt (mode, ln, &ciFrom, NULL, NULL, into, cs);
}

void linesGetCharsExt (int pmode, const Line *ln,
	const ColumnInterval *ciFrom, ColumnInterval *ciDone, ColumnInterval *ciRest,
	struct Buffer *into, int *cs)
{
    int leave;
    const DtChar *dtFrom, *dtLim, *dtPtr;
    int mycs;
    char *toptr, *tolim;	/* used as wchar_t in mode==UTF16 */
    int amode=  pmode&LGC_ALLOC;
    int csmode= pmode&LGC_MODE_CSMASK;

    if (!cs) {
	cs= &mycs;
	*cs= DTCHAR_ASCII;
    }

    dtFrom= linesCharPos (ln, ciFrom->left);
    dtLim=  linesCharPos (ln, ciFrom->right);

/* <FIXME> we could be more conservative */
    if (amode==LGC_ALLOC &&
	dtFrom != dtLim &&
        into->maxlen <= into->len + 4096) {
	BuffSecure (into, 8192);
    }

    toptr= into->ptr + into->len;
    tolim= into->ptr + into->maxlen;

    for (dtPtr= dtFrom-1, leave=0; !leave && ++dtPtr<dtLim && toptr<tolim; ) {
	unsigned currCharSet =  DTC_GetCset (dtPtr->u);	/* DTCHAR_xxx */
	unsigned currCharCode = DTC_GetCharCode (dtPtr->u);

	if (csmode==LGC_MODE_UTF16) {
	    int cpin;	/* CP_OEMCP or CP_ACP depending on cType */
	    int tomaxlen;
	    int retlen;

	    if (currCharSet==DTCHAR_XTERM_GRAPH) {
		*(wchar_t *)toptr = GraphMapWide (currCharCode);
		toptr += sizeof (wchar_t);
		continue;

	    } else if (currCharSet==DTCHAR_UNICODE) {
		unsigned wlen;

		wlen = uconvUnicodeToUtf16 ((uint16_t *)toptr, currCharCode);
		toptr += 2*wlen;
		continue;
	    }

	    cpin= currCharSet==DTCHAR_OEM? CP_OEMCP: CP_ACP;
	    tomaxlen= (tolim - toptr) / sizeof (wchar_t);

	    retlen= MultiByteToWideChar (cpin, 0
		, IntPtrAsCharPtr (&currCharCode), 1	/* from: ptr, len    */
		, (wchar_t *)toptr,  tomaxlen);		/* to:   ptr, maxlen */

	    if (retlen <= 0 || retlen >= tomaxlen) {
		leave= -1;
		continue;
	    }
	    toptr += retlen * sizeof (wchar_t);

	} else {				/* single-byte output */
	    if (currCharSet==DTCHAR_XTERM_GRAPH) {
		if (*cs==DTCHAR_ASCII) {
		    *cs= DTCHAR_OEM;	/* 'OEM' might be better for pseudo-graph */
		} else if (*cs!=DTCHAR_OEM && csmode==LGC_MODE_NARROW_NOCONV) {
		    leave= 1;
		    continue;
		}

		if (*cs==DTCHAR_OEM) *toptr++ = GraphMapOem (currCharCode);
		else		     *toptr++ = GraphMapAnsi (currCharCode);

	    } else if (currCharSet==DTCHAR_ASCII) {	/* ASCII 32-127: compatible with both ANSI and OEM  */
		*toptr++ = (char)currCharCode;
		
	    } else if (*cs==DTCHAR_ASCII) {	 /* *cs isn't defined yet, define it to the current */
		*cs = currCharSet;
		*toptr++ = (char)currCharCode;

	    } else if (currCharSet==(unsigned)*cs) {	 /* *cs is defined, the current matches */
		*toptr++ = (char)currCharCode;

	    } else { 			 	 /* defined, doesn't match: stop or convert (depending on 'mode') */
		if (csmode==LGC_MODE_NARROW_NOCONV) {
		    leave= 1;
		    continue;

		} else {
		    *toptr = currCharCode;
		    if (currCharSet==DTCHAR_OEM) OemToAnsiBuff (toptr, toptr, 1);
		    else			 AnsiToOemBuff (toptr, toptr, 1);
		    ++toptr;
		}
	    }
	}
    }
    into->len = toptr - into->ptr;

    if (ciDone) {
	ciDone->left=  dtFrom - (const DtChar *)ln->text;	/* automagically divided by sizeof (DtChar) !!!! */
	ciDone->right= dtPtr  - (const DtChar *)ln->text;	/* automagically divided by sizeof (DtChar) !!!! */
    }
    if (ciRest) {
	ciRest->left=  dtPtr  - (const DtChar *)ln->text;	/* automagically divided by sizeof (DtChar) !!!! */
	ciRest->right= dtLim  - (const DtChar *)ln->text;	/* automagically divided by sizeof (DtChar) !!!! */
    }
}

/* check if the current line is the continuation of the previous one
   (if there is a previous line), 
   if so return the previous line, otherwise NULL
 */
Line *linesGetPossiblePrevLine (BufferLineNo blnAct)
{
    const Line *lnCurr, *lnPrev, *lnRet= NULL;

    if (blnAct < 1 || blnAct >= term.linebuff.used)
	goto RETURN;

    ASSERT(term.linebuff.list != NULL);

    lnPrev= term.linebuff.list[blnAct-1];
    lnCurr= term.linebuff.list[blnAct];
    ASSERT(lnCurr!=NULL && lnPrev!=NULL);

    if (lnPrev->wrapped&LINE_WRAP_NEXTLINE) lnRet= lnPrev;

RETURN:
    return (Line *)lnRet;
}

/* check if the next line is the continuation of the current one
   (if there is a next line), 
   if so return the previous line, otherwise NULL
 */

Line *linesGetPossibleNextLine (BufferLineNo blnAct)
{
    const Line *lnCurr, *lnNext, *lnRet= NULL;

    if (blnAct < 0 || blnAct+1 >= term.linebuff.used)
	goto RETURN;

    ASSERT(term.linebuff.list != NULL);

    lnCurr= term.linebuff.list[blnAct];
    lnNext= term.linebuff.list[blnAct+1];
    ASSERT(lnCurr!=NULL && lnNext!=NULL);

    if (lnCurr->wrapped&LINE_WRAP_NEXTLINE) lnRet= lnNext;

RETURN:
    return (Line *)lnRet;
}
