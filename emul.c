/* emul.c
 * Copyright (c) 1997 David Cole
 *
 * Terminal emulation
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <shellapi.h>

#include "utils.h"
#include "term.h"
#include "termwin.h"
#include "lines.h"
#include "emul.h"
#include "connect.h"
#include "dtelnet.h"
#include "raw.h"
#include "socket.h"
#include "log.h"
#include "status.h"
#include "printing.h"
#include "editor.h"
#include "dtchars.h"
#include "scroll.h"
#include "uconv.h"

#define DEBUG(str)

static unsigned char staticUsCharSet [] = {
      0,   1,   2,   3,   4,   5,   6,   7,
      8,   9,  10,  11,  12,  13,  14,  15,
     16,  17,  18,  19,  20,  21,  22,  23,
     24,  15,  26,  27,  28,  29,  30,  31,
    ' ', '!', '"', '#', '$', '%', '&', '\'',
    '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', ':', ';', '<', '=', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
    '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
    'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    'x', 'y', 'z', '{', '|', '}', '~', 127,
    128, 129, 130, 131, 132, 133, 134, 135,
    136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151,
    152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167,
    168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183,
    184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199,
    200, 201, 202, 203, 204, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215,
    216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231,
    232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247,
    248, 249, 250, 251, 252, 253, 254, 255
};

static unsigned char staticGraphicsCharSet [] = {
      0,   1,   2,   3,   4,   5,   6,   7,
      8,   9,  10,  11,  12,  13,  14,  15,
     16,  17,  18,  19,  20,  21,  22,  23,
     24,  15,  26,  27,  28,  29,  30,  31,
    ' ', '!', '"', '#', '$', '%', '&', '\'',
    '(', ')', '*', '+', ',', '-', '.', '/',
    219, '1', '2', '3', '4', '5', '6', '7',
    '8', '9', ':', ';', '<', '=', '>', '?',
    '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', '[', '\\', ']', '^', ' ',
    '`', 177, 254, 254, 254, 254, 248, 241,
    176, 254, 217, 191, 218, 192, 197, 254,
    254, 196, 254, '_', 195, 180, 193, 194,
    179, 243, 242, 227, 254, 156, 250, 127,
    128, 129, 130, 131, 132, 133, 134, 135,
    136, 137, 138, 139, 140, 141, 142, 143,
    144, 145, 146, 147, 148, 149, 150, 151,
    152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167,
    168, 169, 170, 171, 172, 173, 174, 175,
    176, 177, 178, 179, 180, 181, 182, 183,
    184, 185, 186, 187, 188, 189, 190, 191,
    192, 193, 194, 195, 196, 197, 198, 199,
    200, 201, 202, 203, 204, 205, 206, 207,
    208, 209, 210, 211, 212, 213, 214, 215,
    216, 217, 218, 219, 220, 221, 222, 223,
    224, 225, 226, 227, 228, 229, 230, 231,
    232, 233, 234, 235, 236, 237, 238, 239,
    240, 241, 242, 243, 244, 245, 246, 247,
    248, 249, 250, 251, 252, 253, 254, 255
};

unsigned char *usCharSet = staticUsCharSet;
unsigned char *ukCharSet = staticUsCharSet;
unsigned char *altCharSet = staticUsCharSet;
unsigned char *graphicsCharSet = staticGraphicsCharSet;
unsigned char *altGraphicsCharSet = staticGraphicsCharSet;

/* map graphic characters from vt100 code to OEM charset
 * warning: some codes are below 0x20 (eg 0x11 as LARROW)
 */
static unsigned char graphToOEM [] = {
'l', 0xda, 'm', 0xc0, 'k', 0xbf, 'j', 0xd9, 't', 0xc3, 'u', 0xb4,
'v', 0xc1, 'w', 0xc2, 'q', 0xc4, 'x', 0xb3, 'n', 0xc5, 'o', 0x7e,
's', 0x5f, '`', 0x04, 'a', 0xb1, 'f', 0xf8, 'g', 0xf1, '~', 0xfe,
',', 0x11, '+', 0x10, '.', 0x19, '-', 0x18, 'h', 0xb0, 'i', 0xce,
'0', 0xdb, 'p', 0xc4, 'r', 0xc4, 'y', 0xf3, 'z', 0xf2, '{', 0xe3,
'|', 0xd8, '}', 0x7d, 0
};
/*
 * terminating zero does not count into length
*/
#define NgraphToOEM ((sizeof (graphToOEM) -1)/2)

static Emul currEmul; /* initialised with zeroes */;

static void emulCharSet(int charSet);
static void emulCharDeleteCore (int numChars, BOOL bKeyboard);

static void emulClearScreenCore (int param);

/* Return pointer to the description of the terminal currently being
 * emulated.
 */
Emul* emulGetTerm()
{
    return &currEmul;
}

/* Set the terminal type to emulate by name
 *
 * Args:
 * name - the name of the terminal type to emulate
 */
void emulSetTerm(const char* name)
{
    telnetGetTermProfile(&currEmul, name);
}

/* Set the from/to server charsets specified by name */
/* cf dtchars.h: 
   name       index fAnsi
   UTF-8/UTF8 1     2
   ANSI       2     1
   OEM        3     0
 */
int emulSetCharset (Emul *emul, const char *name)
{
    int rc;
    int cindex;

    if (!emul) emul= &currEmul;

    rc= dtcEC_Load (name,
		emul->ctServerToOem,  emul->ctOemToServer,
		emul->ctAnsiToServer, emul->ctServerToAnsi);

    cindex= dtcEC_GetIndex (name);
    if      (cindex==DTC_UTF8) emul->fAnsi= 2;
    else if (cindex==DTC_OEM)  emul->fAnsi= 0;
    else		       emul->fAnsi= 1;

    return rc;
}

#define emulGetServerDtcc(emulptr) \
    ((emulptr)->fAnsi==0? DTCHAR_OEM:\
     (emulptr)->fAnsi==1? DTCHAR_ANSI:\
			  DTCHAR_UTF8)

/**************************
 * ATTRIBUTE MANIPULATION *
 **************************/

/* Set the foreground attribute for new characters
 *
 * Args:
 * color - ANSI color number 0..7 or 0..255
 */
static void setForeground (const DtColor color)
{
    SET_FG (term.attr.current, color);
    term.attr.current &= ~ ATTR_FLAG_DEF_FG;
}

static void setDefaultForeground (void)
{
    SET_FG (term.attr.current, 0);  		/* has no importance */
    term.attr.current |= ATTR_FLAG_DEF_FG;	/* this is the important part */
}

/* Set the background attribute for new characters
 *
 * Args:
 * color - ANSI color number 0..7 or 0..255
 */
static void setBackground (DtColor color)
{
    SET_BG (term.attr.current, color);
    term.attr.current &= ~ ATTR_FLAG_DEF_BG;
}

static void setDefaultBackground (void)
{
    SET_BG (term.attr.current, 0);  		/* has no importance */
    term.attr.current |= ATTR_FLAG_DEF_BG;	/* this is the important part */
}

/* Bold: it might make the current foreground color intensive (cf ATTR_FGCOLOR_SET8)
 */
static void setBold(int fset)
{
    if (fset) term.attr.current |=  ATTR_FLAG_BOLD;
    else      term.attr.current &= ~ATTR_FLAG_BOLD;
}

/* Blink: it might make the current background color intensive (cf ATTR_BGCOLOR_SET8)
 */
static void setBlink(BOOL fset)
{
    if (fset) term.attr.current |=  ATTR_FLAG_BLINK;
    else      term.attr.current &= ~ATTR_FLAG_BLINK;
}

/* Set or clear the inverse attribute (DECSCNM -- ESC[?5h vs ESC[?5l)
 *
 * Args:
 * inverse - when TRUE, foreground and background are transposed.
 */
static void setInverse(BOOL inverse)
{
    term.attr.inverseVideo = inverse;
}

/* Set or clear the negative attribute (ESC[7m vs ESC[27m)
 *
 * Args:
 * inverse - when TRUE, foreground and background are transposed.
 * (unlike SetInverse, this affects the future, not the past!)
 */
static void setNegative(BOOL negative)
{
    if (negative) term.attr.current |=  ATTR_FLAG_NEGATIVE;
    else	  term.attr.current &= ~ATTR_FLAG_NEGATIVE;
}

/* Restore the default color attributes ESC[0m
 */
static void clearAttribs(void)
{
    term.attr.current= ATTR_FLAG_DEF_FG | ATTR_FLAG_DEF_BG;
}

/* only the colors, not the underline,bold,blink,inverse attributes */
Attrib getAttrColors(void)
{
    Attrib retval= term.attr.current & (FG_MASK|BG_MASK|ATTR_FLAG_DEF_FG|ATTR_FLAG_DEF_BG);
    return retval;
}

/* every attribute */
Attrib getAttrToStore (void)
{
    Attrib retval= term.attr.current;
    return retval;
}

/**********************
 * TERMINAL EMULATION *
 **********************/

/* Clear parameters used for various terminal commands
 */
static void emulParamInit (InputParsingState *pst, int newstate, int chSeq)
{
    memset (pst, 0, sizeof *pst);
    pst->state = newstate;
    pst->chSeq = (unsigned char)chSeq;
}

/* Start building the next command parameter
 * called at ';' (eg: ESC[;H ESC[1;H ESC[;1H ESC[2;4H)
 */
static void emulParamNext (InputParsingState *pst)
{
    if (pst->numParams==0) { /* special case! */
	pst->numParams= 2;

    } else if (pst->numParams < (int)(sizeof pst->params / sizeof pst->params[0])) {
	pst->numParams++;
    }
}

/* Accumulate a digit in the current command parameter
 *
 * Args:
 * num - a decimal digit 0 .. 9
 */
static void emulParamDigit (InputParsingState *pst, int digit)
{
    if (pst->numParams == 0)
	pst->numParams++;

    pst->params[pst->numParams - 1]
	= pst->params[pst->numParams - 1] * 10 + digit;
}

static void emulParamStringChar (InputParsingState *pst, int charcode)
{
    int val;

    if (pst->numSParam >= MAXSPARAM) {
	return; /* ignore */

/* normal character */
    } else if (pst->stateSParam == 0) {
	if (emulGetServerDtcc (&currEmul) == DTCHAR_UTF8) { /* now we convert it back to UTF8 */
	    pst->numSParam += uconvUnicodeToUtf8 ((unsigned char *)pst->sparam + pst->numSParam, charcode);
	} else {
	    pst->sparam [pst->numSParam++] = (char)charcode;
	}

/* hex digit: first/second half of a character */
    } else {
	val = fromhex ((char)charcode);
	if (pst->stateSParam == 1) {
	    pst->sparam [pst->numSParam] = (char)(val<<4);
	    pst->stateSParam = 2;
	} else {
	    pst->sparam [pst->numSParam] += (char)val;
	    ++pst->numSParam;
	    pst->stateSParam = 1;
	}
    }
}

/* Set the window title from the captured text
 */
static void emulSetTitle (InputParsingState *pst, int unused)
{
    if (pst->numParams == 0)
	pst->params[pst->numParams++] = 0;
    switch (pst->params[0]) {
    case 0:
    case 2:
	pst->sparam [pst->numSParam] = '\0';	/* terminate string param */
	termSetTitle (pst->sparam, emulGetServerDtcc (&currEmul));
	break;
    }
}

/* termLoc:
   the relation between y and the emulated terminal:
	<0: y is above the first line of the terminal
	=0: y is in the terminal
	>0: y is below the last line of the terminal
 */

static TerminalLineNo termLoc (TerminalLineNo y)
{
    TerminalLineNo retval;

    if (y<0) retval= y;
    else if (y<term.winSize.cy) retval= 0;
    else retval= y-(term.winSize.cy-1);
    return retval;
}

/* scrollLoc:
   the relation between y and the scrolling region:
	<0: y is above the first line of the SR
	=0: y is in the SR
	>0: y is below the last line of the SR
 */

static TerminalLineNo scrollLoc (TerminalLineNo y)
{
    TerminalLineNo retval;

    if (term.scroll.fFullScreen) retval= termLoc (y);
    else if (y<term.scroll.lines.top) retval= y-term.scroll.lines.top;
    else if (y<term.scroll.lines.bottom) retval= 0;
    else retval= y-(term.scroll.lines.bottom-1);
    return retval;
}

static void termGetScrollLines (TerminalInterval *ti)
{
    if (term.scroll.fFullScreen) {
	ti->top= 0;
	ti->bottom= term.winSize.cy;
    } else {
	*ti = term.scroll.lines;
    }
}

static void emulKeyPadMode (InputParsingState *pst,int fAppl)
{
    int fStatusChange;

    termSetKeyPadMode (NULL, fAppl, &fStatusChange);
    if (fStatusChange) statusInvalidate ();
}

/* dir: 0/1 = DECBI/DECFI
 * 	2/3 = SL/SR
 */
static void emulDecBiFi (InputParsingState *pst, int dir)
{
    char scroll= ' '; /* I/D: insert empty line at column #0 / delete column #0 */
    char nscroll= 1;

    switch (dir) {
    case 0:
	if (term.cursor.pos.x > 0) --term.cursor.pos.x;
	else			   scroll= 'I';
	break;

    case 1:
	if (term.cursor.pos.x < term.winSize.cx - 1) ++term.cursor.pos.x;
	else			   		     scroll= 'D';
	break;

    case 2:
	scroll= 'D';
	nscroll= pst->numParams>0 && pst->params[0]>0 ? pst->params[0] : 1;
	break;

    case 3:
	scroll= 'I';
	nscroll= pst->numParams>0 && pst->params[0]>0 ? pst->params[0] : 1;
	break;
    }

    if (scroll=='I') {
	TerminalInterval tlScroll;
	TerminalLineNo tl;

	term.cursor.pos.x = 0;
/* scroll right, if the cursor is in the scrolling area */
	if (scrollLoc (term.cursor.pos.y)!=0) goto RETURN;

	termGetScrollLines (&tlScroll);
	for (tl= tlScroll.top; tl < tlScroll.bottom; ++tl) {
	    linesCharsInsert (tl, 0, nscroll);	/* insert space(s) */
	}

    } else if (scroll=='D') {
	TerminalInterval tlScroll;
	TerminalLineNo tl;

	term.cursor.pos.x = term.winSize.cx - 1;
/* scroll left, if the cursor is in the scrolling area */
	if (scrollLoc (term.cursor.pos.y)!=0) goto RETURN;

	termGetScrollLines (&tlScroll);
	for (tl= tlScroll.top; tl < tlScroll.bottom; ++tl) {
	    linesCharsDelete (tl, 0, nscroll);	/* delete the first colum(s) */
	}
    }

RETURN:
    term.cursor.fLineFull = FALSE;
}

static void emulResetTerm1 (InputParsingState *pst, int fHard)
{
    emulResetTerminal (fHard);
}

static BOOL emulTableSeq (InputParsingState *pst);

/* nLines<0: go -nLines up,
 * nLines>0: go nLines down
 * fScroll: 
 * if the cursor enters the scrolling region, it won't leave it;
 * scrolling will occure depending on 'fScroll'
 * eg: ESC[A, ESC[B, ESC[E - no scroll;
 *     ESC D, ESC M - scroll
 */
static void cursorUpDown (int nLines, BOOL fScroll)
{
    int sloc1;
    int sloc2;
    int tloc2;
    TerminalLineNo newy;
    int nScroll;

    newy= term.cursor.pos.y + nLines;
    sloc1= scrollLoc (term.cursor.pos.y);
    sloc2= scrollLoc (newy);
    tloc2= termLoc (newy);

    if (sloc2==0) { /* new position is in the scrolling region: okay */
        newy= term.cursor.pos.y + nLines;

    } else if ((sloc1<0 && sloc2<0) ||
	       (sloc1>0 && sloc2>0)) { /* scroll region is avoided */
	/* don't leave the screen */
	newy -= tloc2;

    } else {
	nScroll= sloc2; /* positive: scroll the scrolling region up nSrcoll lines */
			/* negative: scroll the scrolling region down -nScroll lines */
	/* don't leave the scrolling region */
        newy -= sloc2;

	if (fScroll) Scroll (&term.scroll.lines, nScroll);
    }
    term.cursor.pos.y= newy;
    term.cursor.fLineFull= FALSE;
}

static void cursorUpDownScroll (int nLines)
{
    cursorUpDown (nLines, TRUE);
}

/* If a character added to the end of the current line will cause a
 * line wrap, perform necessary actions.
 */
static void emulCheckWrap(void)
{
    if (term.cursor.pos.x==term.winSize.cx-1 &&
	term.cursor.fLineFull) {
	if (term.lineWrap) {
	    /* Wrap to start of next line
	     */
	    int lineIdx = linesTerminalToLine(term.cursor.pos.y);
	    Line* line;
	    linesCreateTo(lineIdx);
	    line = linesGetLine(lineIdx);
	    /* Remember that we wrapped this line.  This allows us to
	     * copy/paste lines that wrap without getting spurious end
	     * of line sequences.
	     */
	    if (line != NULL)
		term.linebuff.list[lineIdx]->wrapped = 1;

	    term.cursor.pos.x = 0;
	    if (termIsScrollBottom (term.cursor.pos.y)) {
		Scroll (&term.scroll.lines, 1);

	    } else if (! termIsScreenBottom (term.cursor.pos.y)) {
	        term.cursor.pos.y++;
	    }
	}
    }
}

/* Carriage Return moves cursor to the beginning of the current line
 */
static void emulCarriageReturn(void)
{
    term.cursor.pos.x = 0;
    term.cursor.fLineFull = FALSE;
}

/* Line Feed moves cursor down one line.
 * called by:
 *	'IND' = 'ESC D' (fcr=0)
 *	'NEL' = 'ESC E' (fcr=1)
 */
static void emulLineFeed (InputParsingState *pst, int fcr)
{
    if (fcr || rawInBinaryMode())
	emulCarriageReturn();
    cursorUpDownScroll (1);
}

static void emulControlBits (InputParsingState *pst, int nbits)
{
    if (nbits==8) term.miscFlags |=  TERMFLAG_8BIT;
    else	  term.miscFlags &= ~TERMFLAG_8BIT;
}

/* generate either ESC;<c7> or 0x80+(c7-0x40) */
/* 0x40 <= c7 <= 0x5f */
static size_t emulC1 (char *to, unsigned c7)
{
    char *p= to;

    if (term.miscFlags & TERMFLAG_8BIT) {
	*p++ = (char)(0x80 + (c7-0x40));

    } else {
	*p++ = 0x1b;
	*p++ = (char)c7;
    }

    return (size_t)(p-to);
}

/* Move cursor to the next tab stop.  The cursor always stays on the
 * current line.
 */
static void emulTab(void)
{
    if (printingIsActive()){
        printingAddChar('\t');
        return;
    }

    /* Move the cursor */
    do
	term.cursor.pos.x++;
        while (term.cursor.pos.x < MaxLineLen
	   && !term.tabStops[term.cursor.pos.x]);

    /* Make sure that we do not move the cursor off the end of the
     * visible line.
     */
    if (term.cursor.pos.x >= term.winSize.cx)
	term.cursor.pos.x =  term.winSize.cx-1;
}

/* Move cursor to the previous tab stop.
 */
static void emulBacktab(void)
{
    do
	term.cursor.pos.x--;
    while (term.cursor.pos.x > 0 && !term.tabStops[term.cursor.pos.x]);

    if (term.cursor.pos.x < 0)
	term.cursor.pos.x = 0;
}

/* Tab forward/backward 'n' tab stops (default=1)
 * fBack: FALSE: Forward (CHT)
 *        TRUE:  Backward (CBT)
 */
static void emulTabForward (InputParsingState *pst, int fBack)
{
    int nTabs;

    if (pst->numParams>1)
	return;
    else if (pst->numParams==0 || pst->params[0]==0)
	nTabs= 1;
    else
	nTabs= pst->params[0];

    if (fBack) {
	while (--nTabs >= 0) emulBacktab();
    } else {
	while (--nTabs >= 0) emulTab ();
    }
}

/* Set a tab stop at the current cursor column.
 */
static void emulTabSet(void)
{
    if (term.cursor.pos.x < MaxLineLen)
	term.tabStops[term.cursor.pos.x] = 1;
}

/* Clear tab stops depending on <param-0>;
 *   if 0, clear tab stop at current cursor column
 *   if 3, clear all tab stops
 */
static void emulTabClear(void)
{
    if (term.state.numParams != 1)
	return;
    switch (term.state.params[0]) {
    case 0:
	/* Clear tab stop at current cursor column
	 */
	if (term.cursor.pos.x >= 0 && term.cursor.pos.x < MaxLineLen)
	    term.tabStops[term.cursor.pos.x] = 0;
	break;
    case 3:
	/* Clear all tab stops
	 */
	memset(term.tabStops, 0, sizeof(term.tabStops));
	break;
    }
}

/* Reset all tab stops to 8 character interval
 */
static void emulTabReset(void)
{
    int x;

    for (x = 0; x < (int)sizeof(term.tabStops); x++)
	term.tabStops[x] = (char)(x > 0 && (x % 8) == 0);
}

/* Backspace the cursor
 */
static void emulBackspace(BOOL bKeyboard)
{
    int lineIdx;
    Line *line;

    if (term.cursor.pos.x > 0) {
	term.cursor.pos.x--;
        if (bKeyboard)
	    emulCharDeleteCore (1, bKeyboard);
    } else {
	lineIdx = linesTerminalToLine(term.cursor.pos.y);
	if (lineIdx==0) goto RETURN;
	--lineIdx;

	linesCreateTo(lineIdx);
	line = linesGetLine(lineIdx);
	if (! line->wrapped) goto RETURN;

/*	term.cursor.pos.x = term.winSize.cx - 1; */
	ASSERT (line->len > 0);
	term.cursor.pos.x = line->len - 1;

	--term.cursor.pos.y;
        if (bKeyboard)
	    emulCharDeleteCore (1, bKeyboard);
    }
RETURN:
    term.cursor.fLineFull= FALSE;	/* we do this in every case! */
}

/* Send the current logical line to the remote end */
static void emulSendCurrentLine (TerminalLineNo *plastline)
{
    TerminalLineNo termline;
    BufferLineNo lineIdx;
    Line *line, *pline;
    int loop;
    char LineBuff [sizeof (line->text)];
    Buffer bLineBuff;
static const char sCRLF [2] = "\r\n";
    int cs;
    const unsigned char *ctmap;

    bLineBuff.ptr= LineBuff;
    bLineBuff.len= 0;
    bLineBuff.maxlen= sizeof LineBuff;

    termline= term.cursor.pos.y;
    lineIdx = linenoTerminalToBuffer (termline);
    linesCreateTo(lineIdx);
    line = linesGetLine(lineIdx);

    if (currEmul.fAnsi) {
	cs = DTCHAR_ANSI;
	ctmap = currEmul.ctAnsiToServer;
    } else {
	cs = DTCHAR_OEM;
	ctmap = currEmul.ctOemToServer;
    }

    while (lineIdx > 0 &&
	   (pline = linesGetLine(lineIdx-1))->wrapped) {
	--lineIdx;
	--termline;
	line = pline;
    }
    for (loop = 1; loop;) {
	if (line->len) {/* do not send empty line */
	    bLineBuff.len= 0;
	    linesGetChars (LGC_MODE_NARROW,
		line, 0, line->len,
		&bLineBuff, &cs);
	    ctabTranslay (ctmap, bLineBuff.ptr, bLineBuff.len); /* <FIXME> is this necessary? */
	    socketWrite (bLineBuff.ptr, bLineBuff.len);
	}
	if (! line->wrapped) {
	    loop = 0;
	    continue;
	}

	++lineIdx;
	line = linesGetLine(lineIdx);
	++termline;
    }

    socketWrite (sCRLF, sizeof (sCRLF));
    logSession (sCRLF, sizeof (sCRLF));

    if (plastline) *plastline= termline;
    term.cursor.pos.x = 0;
    cursorUpDownScroll (termline + 1 - term.cursor.pos.y);
}

/* Save the current cursor position and character attributes
 */
static void emulCursorSave(SavedCursorData *into)
{
    into->pos = term.cursor.pos;
    into->fLineFull = term.cursor.fLineFull;
    into->attr= term.attr;
    into->charSet = term.charSet;
    into->G0 = term.G0;
    into->G1 = term.G1;
}

/* Restore the previously save cursor position and character attributes
 */
static void emulCursorRestore(const SavedCursorData *from)
{
    BOOL fChanged= FALSE;

    term.cursor.pos = from->pos;
    term.cursor.fLineFull = from->fLineFull;

    if (term.cursor.pos.y >= term.winSize.cy) {
	term.cursor.pos.y= term.winSize.cy-1;
	fChanged= TRUE;
    }
    if (term.cursor.pos.x >= term.winSize.cx) {
	term.cursor.pos.x= term.winSize.cx-1;
	fChanged= TRUE;
    }
    if (fChanged) term.cursor.fLineFull = FALSE;

    term.attr= from->attr;
    term.charSet = from->charSet;
    term.G0 = from->G0;
    term.G1 = from->G1;
}

#define CRSR_SAVE     0
#define CRSR_RESTORE  1
#define CRSR_SAVE_TO_BOTH 2 /* save to both saved[0] and saved[1] -- used by 'reset' */

static void emulCursorSaveRestore (int action)
{
    SavedCursorData *pscd;

    if (action==CRSR_SAVE_TO_BOTH) {
	emulCursorSave (&term.saved[0]);
	emulCursorSave (&term.saved[1]);
	goto RETURN;
    }
    
    if (term.altbuff==0) pscd= &term.saved[0];
    else		 pscd= &term.saved[1];

    if (action==CRSR_SAVE) emulCursorSave (pscd);
    else		   emulCursorRestore (pscd);

RETURN:
   return;
}

/* Perform a reverse line feed.  Scroll the terminal down if necessary.
 */
static void emulReverseLineFeed(void)
{
    cursorUpDownScroll (-1);
}

/* Make sure that the cursor location is within the specified bounds
 * fInScroll: don't leave the scrolling region,
 * even in fRestrict is not set
 */
static void checkCursorLimits(BOOL fInScroll)
{
    if ((term.scroll.fRestrict || fInScroll) && 
	!term.scroll.fFullScreen) {
	if (term.cursor.pos.y < term.scroll.lines.top)
	    term.cursor.pos.y = term.scroll.lines.top;
	if (term.cursor.pos.y >= term.scroll.lines.bottom)
	    term.cursor.pos.y = term.scroll.lines.bottom - 1;
    } else {
	if (term.cursor.pos.y < 0)
	    term.cursor.pos.y = 0;
	if (term.cursor.pos.y >= term.winSize.cy)
	    term.cursor.pos.y = term.winSize.cy - 1;
    }
    if (term.cursor.pos.x < 0)
	term.cursor.pos.x = 0;
    if (term.cursor.pos.x >= term.winSize.cx)
	term.cursor.pos.x = term.winSize.cx - 1;
}

static void emulCursorAddressCore (ServerCursorLineNo y, Column x)
{
    ServerCursorPoint curpos;

    curpos.y= y;
    curpos.x= x;

    pointServerCursorToTerminal (&curpos, &term.cursor.pos);
    term.cursor.fLineFull= FALSE;

    checkCursorLimits (term.scroll.fRestrict);
}

/* Move the cursor to the position y = <param-0>, x = <param-1>.  The
 * top-left position is 1,1 on the terminal, but 0,0 in all variables.
 * If a parameter was missing, a value of 0 is assumed.
 */
static void emulCursorAddress(void)
{
/* terminal or scroll-region relative, 1-based character coordinates: */

    if (term.state.numParams<2) term.state.params[1] = 0;
    if (term.state.numParams<1) term.state.params[0] = 0;

    emulCursorAddressCore (term.state.params[0], term.state.params[1]);
}

/* 20140212.LZS: Almost emulResetScrollRegion,
   except for fRestrict: we leave that unchanged
 */
static void emulFullScreenScrollRegion (void)
{
    term.scroll.lines.top = 0;
    term.scroll.lines.bottom = term.winSize.cy;
    term.scroll.fFullScreen= TRUE;
}

static void emulResetScrollRegion (void)
{
    emulFullScreenScrollRegion ();
    term.scroll.fRestrict= FALSE;
}

/* Define a scroll region; top = <param-0> to bottom = <param-1>.  If
 * top and bottom are 0, cancel any previously defined scroll
 * region.
 */
static void emulScrollRegion(void)
{
    if (term.state.numParams != 0 && term.state.numParams != 2)
	return;
    if (term.state.numParams == 0)
	term.state.params[0] = term.state.params[1] = 0;

    if ((term.state.params[0] == 0 && term.state.params[1] == 0) ||
	(term.state.params[0] == 1 && term.state.params[1] == term.winSize.cy)) {
	/* Cancel scroll region
	 */
	emulFullScreenScrollRegion ();	/* 20140212.LZS: don't reset DECOM! */
	emulCursorAddressCore (1, 1);	/* see the comment below */
	return;
    }

    /* Make sure top line is at least 1, and bottom is greater than
     * top and on the screen.
     */
    if (term.state.params[0] == 0)
	term.state.params[0] = 1;
    if (term.state.params[0] >= term.state.params[1] || term.state.params[1] > term.winSize.cy)
	return;

    /* Define the new scroll region
     */
    term.scroll.lines.top = term.state.params[0] - 1;
    term.scroll.lines.bottom = term.state.params[1];
    term.scroll.fFullScreen= FALSE;

/*  checkCursorLimits (FALSE); 20140212.LZS: I've just found out that after setting to scrolling region (DECSTBM),
    cursor should go to position 1,1 (depending on DECOM) */
    emulCursorAddressCore (1, 1);
}

/* Move the cursor to the column defined in <param-0>
 */
static void emulColumnAddress(void)
{
    if (term.state.numParams == 1)
	term.cursor.pos.x = term.state.params[0] == 0 ? 0 : term.state.params[0] - 1;
    checkCursorLimits (FALSE);
}

/* Move the cursor to the row defined in <param-0>
 */
static void emulRowAddress(void)
{
    if (term.state.numParams == 0)
	term.state.params[0] = 0;
    else if (term.state.numParams != 1)
	return;
    if (term.state.params[0] > 0)
	term.state.params[0]--;

    if (term.scroll.fRestrict && !term.scroll.fFullScreen)
	term.state.params[0] += term.scroll.lines.top;
    term.cursor.pos.y = term.state.params[0];

    checkCursorLimits (FALSE);
}

/* emulPanUpDown:
 *	sel=0: Up   (SU) ESC[<num>S
 *	sel=1: Down (SD) ESC[<num>T
 * supplied, insert one line.
 */
static void emulPanUpDown(int sel)
{
    LineNo nlines;

    if (term.state.numParams==0) term.state.params[0]= 1;
    else if (term.state.params[0]==0) return;
    if (sel) nlines= -term.state.params[0];
    else     nlines=  term.state.params[0];
    Scroll (&term.scroll.lines, nlines);
}

/* Insert/delete 'n' lines at the cursor location. (default=1Ö
 * supplied, insert one line.
 * fDel: FALSE: Insert Lines (EL)
 *       TRUE: Insert Lines (EL)
 */
static void emulLineInsert(InputParsingState *pst, int fDel)
{
    int numLines;		/* number of lines to insert/delete */
    int nScrollLines;		/* either numLines or -numLines */
    TerminalInterval ti;

    if (pst->numParams>1)
	return;
    else if (pst->numParams==0 || pst->params[0]==0)
	numLines= 1;
    else
	numLines= pst->params[0];

    if (! termIsInScrollRegion (term.cursor.pos.y)) return;

    /* There is no point inserting/deleting more lines than are on the terminal
     */
    if (term.cursor.pos.y + numLines >= term.scroll.lines.bottom)
	numLines = term.scroll.lines.bottom - term.cursor.pos.y;

    /* Insert blank lines into line array and terminal window
     */
    ti.top= term.cursor.pos.y;
    ti.bottom= term.scroll.lines.bottom;

    nScrollLines= fDel? numLines: -numLines;
    Scroll (&ti, nScrollLines);

    term.cursor.pos.x= 0; /* 20160426.LZS: cursor to the left margin */
    term.cursor.fLineFull= FALSE;
}

/* Insert <param-0> blank characters at the cursor position.  If
 * param-0 was not supplied, insert one character.
 */
static void emulCharInsert (InputParsingState *pst, int unused)
{
    int numChars;		/* number of characters to insert */

    if (term.state.numParams == 0)
	numChars = 1;
    else if (term.state.numParams == 1)
	numChars = term.state.params[0];
    else
	return;

    /* Insert characters into line array and terminal window
     */
    linesCharsInsert(term.cursor.pos.y, term.cursor.pos.x, numChars);
    winCharsInsert(&term.cursor.pos);
}

/* Delete n characters at the cursor position.
 * If 'bKeyboard' then handle wrapped lines
 */
static void emulCharDeleteCore (int numChars, BOOL bKeyboard)
{
    int lineIdx, nlIdx;
    Line *line, *nl;
    int y, x;
    int nDelChar, nLineRest, nCpyChar;
    TerminalRange r;
    TerminalPoint orig;

    y = term.cursor.pos.y;
    x = term.cursor.pos.x;
    orig= term.cursor.pos;

    lineIdx = linesTerminalToLine(term.cursor.pos.y);

    linesCreateTo(lineIdx);
    line = linesGetLine(lineIdx);

    if (line->len <= x) return;
    nDelChar = numChars;

	nLineRest = line->len - x;
	nDelChar = (nDelChar<=nLineRest) ? numChars : nLineRest;
	nCpyChar = nLineRest - nDelChar;

	if (nCpyChar > 0) {
	    char *pto=   (char *)linesCharPos (line, x);
	    char *pfrom= (char *)linesCharPos (line, x+numChars);
	    char *plim=  (char *)linesCharPos (line, line->len);

	    memmove (pto, pfrom, plim-pfrom);
/**	    memmove(line->text + x, line->text + x + numChars, nCpyChar * sizeof (line->text[0])); **/
	    memmove(line->attr + x, line->attr + x + numChars, nCpyChar);
	}
	line->len -= nDelChar;

    if (bKeyboard) { /* 20060426: only if local */
	while (line->wrapped) {
	    nlIdx = lineIdx + 1;
	    linesCreateTo (nlIdx);
	    nl = linesGetLine (nlIdx);

	    nDelChar = (nDelChar <= nl->len) ? nDelChar : nl->len;
	    nCpyChar = nl->len - nDelChar;

	    if (nDelChar>0) {
		char *pto;
		const char *pfrom, *plim;

		pfrom= (char *)linesCharPos (nl,   0);
		plim=  (char *)linesCharPos (nl,   nDelChar);
		pto=   (char *)linesSecureBytes (line, plim-pfrom);

		memmove (pto, pfrom, plim-pfrom);
/**		memmove (line->text + line->len, nl->text, nDelChar * sizeof (line->text[0])); **/
		memmove (line->attr + line->len, nl->attr, nDelChar);
		line->len += nDelChar;
		if (nl->len==0 && !nl->wrapped) {
		    line->wrapped = 0;
		}
		pfrom= (char *)linesCharPos (nl, nDelChar);
		plim=  (char *)linesCharPos (nl, nl->len);
		pto=   (char *)linesCharPos (nl, 0);
		memmove (pto, pfrom, plim-pfrom);
/**		memmove (pto, pfrom, nl->text + nDelChar, nCpyChar * sizeof (line->text[0])); **/
		memmove (nl->attr, nl->attr + nDelChar, nCpyChar);

		nl->len -= nDelChar;
	    }

	    y = y+1;
	    lineIdx = nlIdx;
	    line = nl;
	}
    }

    r.from= orig;
    r.to.y= y;
    r.to.x= term.winSize.cx;
    termModifyRange (&r);
}

/* Delete <param-0> characters at the cursor position.  If param-0 was
 * not supplied, delete one character.
 */
static void emulCharDelete(BOOL bKeyboard)
{
    int numChars;		/* number of characters to delete */

    if (term.state.numParams == 0)
	numChars = 1;
    else if (term.state.numParams != 1)
	return;
    else
	numChars = term.state.params[0];

    emulCharDeleteCore (numChars, bKeyboard);
}

/* Blank <param-0> characters from the current cursor position.  If
 * param-0 was not supplied, blank one character.
 */
static void emulCharBlank(void)
{
    int numChars;		/* number of characters to blank */
    TerminalRange tr;

    if (term.state.numParams == 0)
	numChars = 1;
    else if (term.state.numParams == 1)
	numChars = term.state.params[0];
    else
	return;

    /* Blank characters from the line array and terminal window
     */
    linesClearRange(term.cursor.pos.y, term.cursor.pos.x,
		    term.cursor.pos.y, term.cursor.pos.x + numChars);
    tr.from= term.cursor.pos;
    tr.to.y= term.cursor.pos.y;
    tr.to.x= term.cursor.pos.x+numChars;
    termModifyRange (&tr);
}

/* Set various terminal attributes
 */
static void emulParamInterpret(void)
{
    int idx;

    if (term.state.numParams == 0) term.state.params[term.state.numParams++] = 0;

    for (idx = 0; idx < term.state.numParams; ++idx) {
	switch (term.state.params[idx]) {
	    case 0: clearAttribs(); break;
	    case 1: setBold(TRUE); break;
	    case 4:
		term.attr.current |= ATTR_FLAG_UNDERLINE;
		break;
	    case 5: setBlink(TRUE); break;
	    case 7: setNegative(TRUE); break;
	    case 8: DEBUG("concealed\n"); break;
	    case 10: emulCharSet(0); break;
	    case 11: emulCharSet(1); break;
	    case 21: setBold(FALSE); break;
	    case 22: DEBUG("alt intensity OFF\n"); break;
	    case 24:
		term.attr.current &= ~ATTR_FLAG_UNDERLINE;
		break;
	    case 25: setBlink(FALSE); break;
	    case 27: setNegative(FALSE); break;
	    case 30:
	    case 31:
	    case 32:
	    case 33:
	    case 34:
	    case 35:
	    case 36:
	    case 37:
		setForeground ((DtColor)(term.state.params[idx]-30)); /* colors 0..7 */
		break;

	    case 38:
		if (term.state.numParams - idx < 2) { /* too short */
		    idx= term.state.numParams-1; /* leave the cycle */
		    break;
		}
		++idx;
		if (term.state.params[idx]==5) { /* 38;5;color -- set foreground color 0..255 */
		    if (term.state.numParams - idx < 2) { /* too short */
			idx= term.state.numParams-1; /* leave the cycle */
			break;
		    }
		    ++idx;
		    setForeground ((DtColor)term.state.params[idx]); /* colors 0..255 */

		} else {		   /* 38;* -- unknown/unsupported */
		    idx= term.state.numParams-1; /* leave the cycle */
		    break;
		}
		break;

	    case 39:
		setDefaultForeground ();
		break;
	    case 40:
	    case 41:
	    case 42:
	    case 43:
	    case 44:
	    case 45:
	    case 46:
	    case 47:
		setBackground ((DtColor)(term.state.params[idx]-40));
		break;

	    case 48:
		if (term.state.numParams - idx < 2) { /* too short */
		    idx= term.state.numParams-1; /* leave the cycle */
		    break;
		}
		++idx;
		if (term.state.params[idx]==5) { /* 48;5;color -- set background color 0..255 */
		    if (term.state.numParams - idx < 2) { /* too short */
			idx= term.state.numParams-1; /* leave the cycle */
			break;
		    }
		    ++idx;
		    setBackground ((DtColor)term.state.params[idx]); /* colors 0..255 */

		} else {		   /* 48;* -- unknown/unsupported */
		    idx= term.state.numParams-1; /* leave the cycle */
		    break;
		}
		break;

	    case 49:
		setDefaultBackground ();
		break;

	    case 90:
	    case 91:
	    case 92:
	    case 93:
	    case 94:
	    case 95:
	    case 96:
	    case 97:
		setForeground ((DtColor)(8+term.state.params[idx]-90)); /* colors 8..15 */
		break;

	    case 100:
	    case 101:
	    case 102:
	    case 103:
	    case 104:
	    case 105:
	    case 106:
	    case 107:
		setBackground ((DtColor)(8+term.state.params[idx]-100)); /* colors 8..15 */
		break;
	}
    }
}

static void emuli_FuncKeyPress (int localaction, const ConstBuffData *b, int dtcc, BOOL fDoPrintFrame);
static void emuli_AddKeybClipText (const ConstBuffData *b, int dtcc,
    int sourceCode, BOOL fDoPrintFrame);

/* Device Status report
 * ANSI: CSI <param> n
 * DEC:  CSI ? <param> n
 */
static void emulDSR (InputParsingState *pst, int fdec)
{
    char buff[32], *p= buff;
    ServerCursorPoint curpos;
    ConstBuffData b;

    if (term.state.numParams == 0)
	term.state.params[term.state.numParams++] = 0;

    switch (term.state.params[0]) {
    case 5:	/* DSR-OS (Operating Status) */
	if (!fdec) {
	    p += emulC1 (p, '[');	/* CSI */
	    p += sprintf (p, "0n");	/* good */
	}
	break;

    case 6:		/* fdec=0: DSR-CPR  (Cursor Position Report) */
			/* fdec=1: DSR-XCPR (Extended CPR)	     */
	pointTerminalToServerCursor (&term.cursor.pos, &curpos);
	p += emulC1 (p, '[');			/* CSI */
	if (fdec) *p++ = '?';			/* DEC-mode: insert '?' to satisfy 'vttest' */
						/* note: documentations don't really agree on this */
						/* cf http://vt100.net/ */
	p += sprintf (p, "%d;%d",
	    (int)curpos.y, (int)curpos.x);
	if (fdec) p += sprintf (p, ";1");	/* DEC-mode: page=1 */
	p += sprintf (p, "R");
	break;

    case 26:	/* DSR-KBD (Keyboard) */
	if (fdec) {
	    p += emulC1 (p, '[');		/* CSI */
	    p += sprintf (p, "?27;1;0;0n");	/* North America <FIXME> */
	}
	break;

    case 75:	/* DSR-DIR (Data Integrity Report */
	if (fdec) {
	    p += emulC1 (p, '[');		/* CSI */
	    p += sprintf (p, "?70n");		/* ready, no prob. */
	}
	break;
    }

    b.ptr= buff;
    b.len= p-buff;

    if (b.len) {
	emuli_FuncKeyPress (EFK_LOCAL_SKIP, &b, DTCHAR_ISO88591, FALSE);
    }
}

/* Report terminal status
 */
static void emulStatusReport (InputParsingState *pst,
	int sel /* 0/1/2 = Primary/Secondary/Tertiary */
		/* 10 = Terminal Parameters */)
{
    char buff[64], *p= buff;

    if (sel==0) {			/* primary */
	p += emulC1 (p, '[');		/* CSI */
	p += sprintf (p, "?62;2c");	/* 62=vt220; 2=with printer */

    } else if (sel==1) {		/* secondary */
	p += emulC1 (p, '[');		/* CSI */
	p += sprintf (p, ">1;95;0c");
	    /*    1=vt220
		 95=version of xterm(!) -- for 'vim' 95 means ESC[?1006 works
		  0=always zero
	     */

    } else if (sel==2) {		/* tertiary */
	char tmpip[64];

	socketGetAddrStr (tmpip, SOCGA_LOCAL|SOCGA_NO_PORT, 0);

	p += emulC1 (p, 'P');		/* DCS */
	p += sprintf (p, "!|%.48s",	/* ! | <string> */
	    tmpip);
	p += emulC1 (p, '\\');		/* ST */

    } else if (sel==10) {		/* Terminal Parameters */
	if (pst->numParams==0) {
	    pst->params[0]= 0;
	    pst->numParams= 1;

	} else if (pst->numParams!=1 || pst->params[0]>1) {
	    return;
	}

	p += emulC1 (p, '[');		/* CSI */
	p += sprintf (p, "%d;1;1;128;128;1;0x",
	    pst->params[0] + 2);	/* Ask DEC about this: 0->2, 1->3 */
    }

    if (p != buff) {
	ConstBuffData b;

	b.ptr= buff;
	b.len= p-buff;
	emuli_FuncKeyPress (EFK_LOCAL_SKIP, &b, DTCHAR_ISO88591, FALSE);
    }
}

#define ESAB_CLEAR_ALT_AT_LEAVING    1047	/* set: nothing special; reset: if on the alternate screen, clear it */
#define ESAB_CRSR_SAV_REST_CLEAR_ALT 1049	/* set: save cursor, switch to alternate, clear; reset: switch to normal, cursor restore */

static void emulSwitchAltBuff (BOOL set, int opt)
{
    BOOL fClearBefore= FALSE;
    BOOL fClearAfter=  FALSE;
    BOOL fSwitch;
    BOOL fSaveCrsr= FALSE;
    BOOL fRestoreCrsr= FALSE;

    fSwitch= termGetAltBuff () != (int)set;
    if (opt==0) { /* nothing special */

    } else if (opt==ESAB_CLEAR_ALT_AT_LEAVING) {
	if (!set && fSwitch) fClearBefore= TRUE;

    } else if (opt==ESAB_CRSR_SAV_REST_CLEAR_ALT) {
	if (set) {
	    fSaveCrsr= TRUE;
	    fClearAfter= TRUE;
	} else {
	    fRestoreCrsr= TRUE;
	}
    } else { /* huh? */
    }

    if (fSaveCrsr)    emulCursorSaveRestore (CRSR_SAVE);
    if (fClearBefore) emulClearScreenCore (2);	/* clear whole screen */
    if (fSwitch) {
	TerminalRange tr;
    
	termCancelSelect (1); /* cancel selection if it is on 	 */
			      /* (or overlaps with) the visible area */
	termSetAltBuff ((int)set);
	tr.from.y= 0;
	tr.from.x= 0;
	tr.to.y= term.winSize.cy-1;
	tr.to.x= term.winSize.cx;
	termModifyRange (&tr);
    }
    if (fClearAfter)  emulClearScreenCore (2);	/* clear whole screen */
    if (fRestoreCrsr) emulCursorSaveRestore (CRSR_RESTORE);
}

/* Set various terminal modes
    fDec: FALSE: ESC[	ANSI
	  TRUE:  ESC[?	DEC-private
    num:  identifies the action to perform
    set:  FALSE: 'l'	clear
	  TRUE:  'h'	set
 */
static void emulSetMode1 (BOOL fDec, int num, BOOL set)
{
    if (fDec) {		/* DEC-private */
	switch (num) {
	case 1:					/* DECCKM: set(h):   UP=>ESC O A ("application" mode)*/
	    term.ansiCursorKeys = set;		/*	   reset(l): UP=>ESC [ A ("ANSI" mode) */
	    break;
	case 2:					/* DECANM: I decided against implementing is, sorry */
	    break;

	case 3: {/* 20130625.LZS */
	    CharSize charSize;
	    PixelSize pixelSize;

	    charSize.cx= set? 132: 80;
	    charSize.cy= 24;
	    termCalculatePixelSize (&charSize, &pixelSize, TRUE);
	    telnetTerminalSize (pixelSize.cx, pixelSize.cy);

	/* clear screen, cursor home, reset scrolling area */
	    emulResetScrollRegion ();

	    if (term.miscFlags & TERMFLAG_NCSM) {
		 /* don't clear the screen */
	    } else {
		emulClearScreenCore (2);	  /* clear whole screen */
	    }

	    emulCursorAddressCore (1, 1); /* cursor home */
	    break; }

	case 5:					/* DECSCNM */
	    /* Switch to/from inverse video
	     */
	    if (term.attr.inverseVideo == set) break;
	    setInverse(set);
	    termModifyRange (NULL);		/* Whole 'window' = (whole visible area) */
	    break;
	case 6:					/* DECOM */
	    term.scroll.fRestrict = set;
	    break;
	case 7:					/* DECAWM */
	    term.lineWrap = set;
	    break;
	case 8:					/* DECARM */
	    term.autoRepeat = set;
	    break;
	case 25:				/* DECTCEM */
	    termSetCursorMode(set);
	    break;

	case 66:        			/* DECNKM */
	    emulKeyPadMode(NULL, set);		/* set=Application,clear=Numeric */
	    break;

	case 67:				/* DECBKM -- we don't do this */
	    break;

	case 95:
	    if (set) term.miscFlags |=  TERMFLAG_NCSM;		/* No Clear on Column Change (DECNCSM) -- CSI ? 95 h */
	    else     term.miscFlags &= ~TERMFLAG_NCSM;		/*    Clear on Column Change (DECNCSM) -- CSI ? 95 l */
	    break;

	case 47: /* rxvt */
	    emulSwitchAltBuff (set, 0);		/* just switch, nothing else */
	    break;

	case 1047: /* xterm */
	    emulSwitchAltBuff (set, ESAB_CLEAR_ALT_AT_LEAVING);
	    break;

	case 1048: /* xterm */				/* save/restore cursor */
	    if (set) emulCursorSaveRestore (CRSR_SAVE);
	    else     emulCursorSaveRestore (CRSR_RESTORE);
	    break;

       	case 1049: /* xterm */
	    emulSwitchAltBuff (set, ESAB_CRSR_SAV_REST_CLEAR_ALT);
	    break;

	case 9:
	    /* X10 compatible mouse reporting */
	    if (emulGetTerm()->flags & ETF_MOUSE_EVENT) {
		termSetMouseRep (set? TERM_MOUSE_REP_X10: 0);
	    }
	    break;

	case 1000:
	    /* X11 compatible mouse reporting */
	    if (emulGetTerm()->flags & ETF_MOUSE_EVENT) {
		termSetMouseRep (set? TERM_MOUSE_REP_X11: 0);
	    }
	    break;

	case 1001:
	    /* X11 compatible mouse tracking, we don't actually do it */
	    if (emulGetTerm()->flags & ETF_MOUSE_EVENT) {
		termSetMouseRep (set? TERM_MOUSE_REP_X11_HIGHLIGHT: 0);
	    }
	    break;

	case 1002:
	    /* X11 compatible mouse tracking */
	    if (emulGetTerm()->flags & ETF_MOUSE_EVENT) {
		termSetMouseRep (set? TERM_MOUSE_REP_X11_BTN_EVENT: 0);
	    }
	    break;

	case 1003:
	    /* X11 compatible mouse tracking, we don't actually do it */
	    if (emulGetTerm()->flags & ETF_MOUSE_EVENT) {
		termSetMouseRep (set? TERM_MOUSE_REP_X11_ANY_EVENT: 0);
	    }
	    break;

	case 1005:	/* UTF8 */
	case 1006:	/* SGR */
	case 1015:	/* URXVT */
	    if (set) {
		int cmode;

		switch (num) {
		case 1005: cmode= TERM_MOUSE_COORD_UTF8;  break;
		case 1006: cmode= TERM_MOUSE_COORD_SGR;   break;
		case 1015: cmode= TERM_MOUSE_COORD_URXVT; break;
		}
		termSetMouseCoord (cmode);
	    } else {
		termSetMouseCoord (TERM_MOUSE_COORD_STD);
	    }
	    break;
#if 0
	case 2:
	    debug("mode:%s\n", set ? "ANSI" : "VT52");
	    break;
	case 3:
	    debug("mode:column %d\n", set ? 132 : 80);
	    break;
	case 4:
	    debug("mode:scrolling %s\n", set ? "smooth" : "jump");
	    break;
	case 18:
	    debug("mode:print-ff %s\n", set ? "yes" : "no");
	    break;
	case 19:
	    debug("mode:print-extent %s\n", set ? "fullscreen" : "region");
	    break;
	default:
	    debug("mode:unknown? %d\n", num);
	    break;
#endif
	}
    } else {		/* ANSI (non-DEC-private) */
	switch (num) {
	case 4:
	    term.insertMode = set;
	    break;
	case 20:
	    term.newLineMode = set;
	    break;
#if 0
	case 2:
	    debug("mode:keyboard %s\n", set ? "locked" : "unlocked");
	    break;
	case 12:		/* set='h'=don't echo; unset='l'=echo */
/* this is not this easy,
   because we use term.echoKeystrokes as if it were 'lineMode'
   which it isn't
 */
	    emulSetEcho (!set);
	    break;
	case 12:
	    debug("mode:send-receive %s\n", set ? "full" : "echo");
	    break;
	default:
	    debug("mode:unknown %d\n", num);
	    break;
#endif
	}
    }
}

static void emulSetMode(BOOL set)
{
    int i;

    for (i=0; i<term.state.numParams; ++i) {
	emulSetMode1 (term.state.seenQuestion, term.state.params[i], set);
    }
}

/* Move the cursor up <param-0> lines.  If param-0 was not supplied,
 * move cursor up one line.
 */
static void emulCursorUp(void)
{
    int numLines = 1;		/* number of lines to move the cursor up */

    if (term.state.numParams == 1)
	numLines = term.state.params[0];
    else if (term.state.numParams != 0)
	return;
    if (numLines == 0)
	numLines = 1;

    cursorUpDown (-numLines, FALSE);
}

/* Move the cursor down <param-0> lines.  If param-0 was not supplied,
 * move the cursor down one line.
 */
static void emulCursorDown (InputParsingState *pst, int fcr)
{
    int numLines = 1;		/* number of line to move the cursor down */

    if (pst->numParams == 0) {
	numLines = 1;

    } else if (pst->numParams == 1) {
	numLines = pst->params[0];

    } else {
	return;
    }

    if (numLines == 0)
	numLines = 1;

    cursorUpDown (numLines, FALSE);

    if (fcr) {
	emulCarriageReturn ();
    }
}

/* Move the cursor right <param-0> columns.  If param-0 was not
 * supplied, move the cursor right one column.
 */
static void emulCursorRight(void)
{
    int numColumns = 1;		/* number of columns to move right */

    if (term.state.numParams == 1)
	numColumns = term.state.params[0];
    else if (term.state.numParams != 0)
	return;
    if (numColumns == 0)
	numColumns = 1;

    term.cursor.pos.x += numColumns;
    term.cursor.fLineFull = FALSE;
    checkCursorLimits (FALSE);
}

/* Move the cursor left <param-0> columns.  If param-0 was not
 * supplied, move the cursor left one column.
 */
static void emulCursorLeft(void)
{
    int numColumns = 1;		/* number of columns to move left */

    if (term.state.numParams == 1)
	numColumns = term.state.params[0];
    else if (term.state.numParams != 0)
	return;
    if (numColumns == 0)
	numColumns = 1;

    term.cursor.pos.x -= numColumns;
    term.cursor.fLineFull = FALSE;
    checkCursorLimits (FALSE);
}

/* Clear part or all of current line depending on param-0;
 *   0 - erase from cursor to end of line
 *   1 - erase from start of line up to and including cursor
 *   2 - erase entire line
 * If param-0 is not supplied, assume value of 0
 */
static void emulClearInLine(void)
{
    TerminalRange tr;

    if (term.state.numParams == 0)
	term.state.params[term.state.numParams++] = 0;
    if (term.state.numParams != 1)
	return;

    switch (term.state.params[0]) {
    case 0:
	/* Erase cursor to end of line
	 */
	linesClearRange(term.cursor.pos.y, term.cursor.pos.x,
			term.cursor.pos.y, term.winSize.cx);
	tr.from= term.cursor.pos;
	tr.to.y= term.cursor.pos.y;
	tr.to.x= term.winSize.cx;
	termModifyRange (&tr);
	break;

    case 1:
	/* Erase beginning of line through cursor
	 */
	linesClearRange(term.cursor.pos.y, 0, term.cursor.pos.y, term.cursor.pos.x + 1);
	tr.from.y= term.cursor.pos.y;
	tr.from.x= 0;
	tr.to.y= term.cursor.pos.y;
	tr.to.x= term.cursor.pos.x+1;
	termModifyRange (&tr);
	break;

    case 2:
	/* Erase line
	 */
	linesClearRange(term.cursor.pos.y, 0, term.cursor.pos.y, term.winSize.cx);
	tr.from.y= term.cursor.pos.y;
	tr.from.x= 0;
	tr.to.y= term.cursor.pos.y;
	tr.to.x= term.winSize.cx;
	termModifyRange (&tr);
    }
}

/* Erase part or all of screen depending on param;
 *   0 - erase from cursor to end of screen
 *   1 - erase from start of screen up to and including cursor
 *   2 - erase entire screen
 *   3 - erase history (xterm addition)
 */
static void emulClearScreenCore (int param)
{
    TerminalRange tr;

    switch (param) {
    case 0:
	/* Erase cursor to end of screen
	 */
	linesClearRange(term.cursor.pos.y, term.cursor.pos.x,
			term.winSize.cy, term.winSize.cx);
	tr.from= term.cursor.pos;
	tr.to.y= term.winSize.cy-1;
	tr.to.x= term.winSize.cx;
	termModifyRange (&tr);
	break;

    case 1:
	/* Beginning of screen through cursor
	 */
	linesClearRange(0, 0, term.cursor.pos.y, term.cursor.pos.x+1);
	tr.from.y= 0;
	tr.from.x= 0;
	tr.to.y= term.cursor.pos.y;
	tr.to.x= term.cursor.pos.x+1;
	termModifyRange (&tr);
	break;

    case 2:
	/* Erase entire screen
	 */
	linesClearRange(0, 0, term.winSize.cy, term.winSize.cx);
	tr.from.y= 0;
	tr.from.x= 0;
	tr.to.y= term.winSize.cy-1;
	tr.to.x= term.winSize.cx;
	termModifyRange (&tr);
	break;

    case 3: {
	/* Erase history -- xterm addition
	 */
	BufferLineNo nHistory= linenoTerminalToBuffer (0);
	if (nHistory<=0) break;

	scrollDeleteFromHistory (nHistory);
	break; }
    }
}

/* Erase part or all of screen depending on param-0;
 *   0 - erase from cursor to end of screen
 *   1 - erase from start of screen up to and including cursor
 *   2 - erase entire screen
 * If param-0 not supplied, assume value of 0
 */
static void emulClearEndOfScreen(void)
{
    if (term.state.numParams == 0)
	term.state.params[term.state.numParams++] = 0;
    if (term.state.numParams != 1)
	return;
    emulClearScreenCore (term.state.params[0]);
}

void emulCursorReset (struct CursorData *cp)
{
    cp->pos.x= cp->pos.y= 0;
    cp->fLineFull= FALSE;
}

static Utf8StreamContext FromServerUtf8StreamContext= Empty_Utf8StreamContext;

/* Reset terminal to initial state
 * note: RIS=hard reset; DECSTR=soft reset
 * in the future, they might mean different things
 */
void emulResetTerminal (BOOL fHard)
{
    int fStatusChange;

    clearAttribs();
    emulParamInit (&term.state, consNormal, 0);
    emulClearScreenCore (2);
    emulTabReset();
    termSetCursorMode(1);

    term.cursorVisible = TRUE;
    term.attr.inverseVideo = FALSE;
    term.lineWrap = TRUE;	/* should it be configurable? */
    term.insertMode = FALSE;
    term.ansiCursorKeys = FALSE;
    term.autoRepeat = TRUE;
    emulCursorReset (&term.cursor);

    term.charSet = 0;
    setTermCharSet (&term.G0, TCSN_US);
    setTermCharSet (&term.G1, TCSN_GRAPHICS);

    emulResetScrollRegion ();

    termClearAltBuff ();
    emulCursorSaveRestore (CRSR_SAVE_TO_BOTH);

    ctabIdent (graphicsCharSet);
    ctabModif (graphicsCharSet, graphToOEM, NgraphToOEM, 0);

    termSetKeyPadMode (NULL, FALSE, &fStatusChange);	/* numeric mode */

    emulControlBits (NULL, 7);	/* we send 7-bit sequences */

    termSetMouseRep (TERM_MOUSE_REP_OFF);
    termSetMouseCoord (TERM_MOUSE_COORD_STD);

    term.miscFlags= 0;

    if (fStatusChange) statusInvalidate ();

    Reset_Utf8StreamContext (&FromServerUtf8StreamContext);
}

/* Control double width / double height
 *
 * Args:
 * mode - desired double height/width mode
 */
static void emulSetDouble(int modecode)
{
    TerminalRange tr;

    switch (modecode) {
    case '3': /* double-height top half */
    case '4': /* double-height bottom half */
    case '5': /* single-width single-height */
    case '6': /* double-width single-height */
	break;

    case '8': /* DECALN fill screen with E */
	linesAlignTest();
	emulCursorAddressCore (1, 1);

	tr.from.y= 0;
	tr.from.x= 0;
	tr.to.y= term.winSize.cy-1;
	tr.to.x= term.winSize.cx;
	termModifyRange (&tr);
	break;
    }
}

static struct {
    char msg [128];
    int  len;
} AnswerBack = {
    "dtelnet from Dave Cole",
    22
};

static void emulSetAnswerBk (InputParsingState *pst, int unused)
{
    emulSetAnswerBack (pst->numSParam, pst->sparam);
}

void emulSetAnswerBack (int len, const char *from)
{
    if (len > (int)sizeof (AnswerBack.msg))
	len = sizeof (AnswerBack.msg);
    memcpy (AnswerBack.msg, from, len);
    AnswerBack.len = len;
}

static void emulTransmitAnswerback (void)
{
    if (AnswerBack.len) {
	socketWrite (AnswerBack.msg, AnswerBack.len);	/* <FIXME> unicode */
    }
}

static BOOL emulExecuteFlag= FALSE;

BOOL emulGetExecuteFlag (void)
{
    return emulExecuteFlag;
}

BOOL emulSetExecuteFlag (BOOL newval)
{
    BOOL oldval= emulExecuteFlag;
    emulExecuteFlag = newval;

    return oldval;
}

static void emulExecute (InputParsingState *pst, int unused)
{
    if (! emulExecuteFlag) return;

    if (pst->numSParam>=2 && pst->sparam[0]=='1') { /* '1' = ShellExecute */
	pst->sparam [pst->numSParam]= '\0';

	ShellExecute (NULL, "open", pst->sparam + 1, NULL, NULL, SW_SHOW);
    }
    return;
}

/* Switch to character set
 *
 * Args:
 * charSet - the desired character set
 */
static void emulCharSet(int charSet)
{
    term.charSet = charSet;
}

/* Not implemented
 */
static void emulHaltTransmit(void)
{
}

/* Not implemented
 */
static void emulResumeTransmit(void)
{
}

/* Not implemented
 */
static void emulAddCheckerboard(void)
{
}

void emulSetKeypadMode (const unsigned char newval[2])
{
    int fChange= 0;
    int i;

    for (i=0; i<2; ++i) {
	if (term.keypadMode[i] != newval[i]) {
	    fChange= 1;
	    term.keypadMode[i]= newval[i];
	}
    }
    if (fChange) statusInvalidate ();
}

/* call this _after_ CheckSizeLimits */
static int emulCheckedResizePixels (const PixelSize *pixelSize)
{
    telnetTerminalSize (pixelSize->cx, pixelSize->cy);

    /* don't clear the screen */

    term.cursor.fLineFull = FALSE;
    if (term.cursor.pos.x >= term.winSize.cx) term.cursor.pos.x = term.winSize.cx - 1;
    if (term.cursor.pos.y >= term.winSize.cy) term.cursor.pos.y = term.winSize.cy - 1;

    return 0;
}

static int emulResizeChars (const CharSize *newSize)
{
    CharSize  charSize= *newSize;
    PixelSize pixelSize;

    termCheckSizeLimits (&charSize);
    termCalculatePixelSize (&charSize, &pixelSize, TRUE);
    emulCheckedResizePixels (&pixelSize);

    return 0;
}

static void emulSetColumns (InputParsingState *pst, int unused)
{
    int n;
    CharSize charSize;

    if (pst->numParams==0) n= 80;
    else 		   n= pst->params[0];

    if (n!=80 && n!=132) return;

    charSize.cy= term.winSize.cy;
    charSize.cx= n;
    emulResizeChars (&charSize);
}

/* Window manipulation functions
 */
static void emulWinManip (void)
{
    char buff [64];
    CharSize charSize;
    PixelSize pixelSize;
    BOOL fChange;
    BuffData b;

    if (term.state.numParams < 1) return;
    switch (term.state.params[0]) {
    case 4:
	if (term.state.numParams != 3) goto RETURN;
	pixelSize.cy= term.state.params[1];
	pixelSize.cx= term.state.params[2];
	termCalculateCharSize (&pixelSize, &charSize, TRUE);
	fChange= termCheckSizeLimits (&charSize);	/* this might change 'charSize', if it is too big/small */
	if (fChange) {					/* then we have to re-calculate pixelSize from the changed values */
	    termCalculatePixelSize (&charSize, &pixelSize, TRUE);
	}
	emulCheckedResizePixels (&pixelSize);
        break;

    case 8:
	if (term.state.numParams != 3) goto RETURN;
	charSize.cy= term.state.params[1];
	charSize.cx= term.state.params[2];
	emulResizeChars (&charSize);
        break;

    case 14: /* at this point we don't report scrollbar and termMargin... should we? */
	b.ptr= buff;
	b.len =  emulC1 (b.ptr, '[');	/* ESC [ or CSI */
	b.len += sprintf (b.ptr+b.len, "4;%ld;%ldt",
                 (long)(term.winSize.cy*term.charSize.cy),
                 (long)(term.winSize.cx*term.charSize.cx));
	emuli_FuncKeyPress (EFK_LOCAL_SKIP, (ConstBuffData *)&b, DTCHAR_ISO88591, FALSE);
	break;

    case 18:
	b.ptr= buff;
	b.len =  emulC1 (b.ptr, '[');	/* ESC [ or CSI */
	b.len += sprintf (b.ptr+b.len, "8;%ld;%ldt", (long)term.winSize.cy, (long)term.winSize.cx);
	emuli_FuncKeyPress (EFK_LOCAL_SKIP, (ConstBuffData *)&b, DTCHAR_ISO88591, FALSE);
	break;

/* DECSLPP: fixed values for lines/pages (LZS: I don't really know what 'pages' are) */
    case 24: case 25: case  36:
    case 48: case 72: case 144:
	charSize.cy= term.state.params[0];
	charSize.cx= term.winSize.cx;
	emulResizeChars (&charSize);
        break;

    default:
	break;
    }
RETURN:
    return;
}

/* Starting or stopping print mode
 */
static void emulPrintControl(void)
{
    if (term.state.numParams < 1) return;

    if (!termAttachedPrinter()) return; /* only print when allowed */

    switch (term.state.params[0]) {
        case 4: /* printer off */
            printingStop();
            break;
        case 5: /* printer on */
            printingStart();
            break;
    }
}

static void emuli_BeforeAddText (void);
static void emuli_AfterAddText  (void);

/* Add character at current cursor position.
 *
 * Args:
 * c - the character to add
 */
static void emuli_AddChar (uint32_t c, int dtcc, BOOL bKeyboard, BOOL fDoPrintFrame)
{
    unsigned char cType;         /* DTCHAR_xxx */
    uint32_t cCode= c;
    TerminalRange tr;

    if (printingIsActive()) {
	printingAddChar(c);	/* <fixme> unicode */
	return;
    }

    if (fDoPrintFrame) emuli_BeforeAddText ();

    /* Check for line wrap before adding character.
     */
    emulCheckWrap();
    if (term.insertMode) {
    /* When in insert mode, move rest of line right one column
     */
	term.state.numParams = 0;
        emulCharInsert (&term.state, 0);
    }

    /* Set character at current cursor position, update the terminal
     * window, and then advance the cursor one column to the right.
     */

    if (bKeyboard) { /* keyboard: don't need to worry about graphic mode */
	cType = dtcc;
	cCode = c;
    } else {
	const TermCharSet *ts;
	if (term.charSet == 0) ts= &term.G0;
	else		       ts= &term.G1;

	if ((ts->flags&TCSF_GRAPH_MASK)==TCSF_GRAPH_YES &&
	    c<=0xff && currEmul.ctGraphConv[c] != 0) {
	    /* graphics mode AND graphical character */
		cType = DTCHAR_XTERM_GRAPH;
		cCode = currEmul.ctGraphConv[c];	/* act.term -> xterm */

	} else { /* non-graph character <fixme> unicode */
	    if (dtcc==DTCHAR_UNICODE) {
		cType = DTCHAR_UNICODE;
		cCode = c;
	    } else {
		if (currEmul.fAnsi) {
		    cType = DTCHAR_ANSI;
		    cCode = currEmul.ctServerToAnsi [c];
		} else {
		    cType = DTCHAR_OEM;
		    cCode = currEmul.ctServerToOem [c];
		}
	    }
	}
    }

    linesSetChar (cType, cCode);
    tr.from= term.cursor.pos;
    tr.to.y= term.cursor.pos.y;
    tr.to.x= term.cursor.pos.x+1;
    termModifyRange (&tr);

/* 20130522.LZS: Cursor won't move out of the screen */
    term.cursor.fLineFull= term.cursor.pos.x == term.winSize.cx-1;
    if (!term.cursor.fLineFull) term.cursor.pos.x++;

    if (fDoPrintFrame) emuli_AfterAddText ();
}

/* process a character of an ESC-sequence */
static void emulEscapeChr (InputParsingState *pst, unsigned c);

/* process a complete CSI-sequence, both normal and DEC-private */
static void emulCSI (InputParsingState *pst);
static void emuli_AddServerChar (uint32_t c, int dtcc, InputParsingState *pst);
static void emuli_FuncKeyPress (int localaction, const ConstBuffData *b, int dtcc, BOOL fDoPrintFrame);
static void emuli_KeyPress (uint32_t ch, int dtcc, BOOL fDoPrintFrame);

typedef struct IterStruct {
    int dtcc_in;			/* UTF16 -> 	UTF8 ->		other -> */
    int dtcc_out;			/* -> UTF32	-> UTF32	-> same	 */
    ConstBuffData rest;
    Utf8StreamContext *putf8ctx;
    Utf8StreamContext utf8ctx;		/* we use this if the caller doesn't give one */
    Utf16StreamContext *putf16ctx;
    Utf16StreamContext utf16ctx;	/* we use this if the caller doesn't give one */
} IterStruct;

static int emuli_IterStart (IterStruct *ctx, const ConstBuffData *bd, int dtcc,
    Utf8StreamContext *putf8ctx, Utf16StreamContext *putf16ctx)
{
    memset (ctx, 0, sizeof *ctx);
    ctx->dtcc_in= dtcc;
    ctx->rest= *bd;

    if (dtcc==DTCHAR_UTF8)  {
	ctx->putf8ctx= putf8ctx?  putf8ctx:  &ctx->utf8ctx;
	ctx->dtcc_out= DTCHAR_UTF32;

    } else if (dtcc==DTCHAR_UTF16) {
	ctx->putf16ctx= putf16ctx? putf16ctx: &ctx->utf16ctx;
	ctx->dtcc_out=  DTCHAR_UTF32;

    } else {
	ctx->dtcc_out= dtcc;

    }

    return ctx->dtcc_out;
}

BOOL emuli_IterNextChar (IterStruct *ctx, uint32_t *pc)
{
    BOOL retval;

    if (ctx->rest.len == 0) return FALSE;

    switch (ctx->dtcc_in) {
    case DTCHAR_UTF8:
	retval= Utf8StreamContextAdd (ctx->putf8ctx, &ctx->rest, pc);
	break;

    case DTCHAR_UTF16:
	retval= Utf16StreamContextAdd (ctx->putf16ctx, &ctx->rest, pc);
	break;

    case DTCHAR_UTF32:
	retval= TRUE;
	*pc= *(uint32_t *)ctx->rest.ptr;
	ctx->rest.ptr += sizeof (uint32_t);
	ctx->rest.len -= sizeof (uint32_t);
	break;

    default:
	retval= TRUE;
	*pc= *(unsigned char *)ctx->rest.ptr;
	++ctx->rest.ptr;
	--ctx->rest.len;
	break;
    }
    return retval;
}

/* Process a stream of characters through the terminal emulator.  This
 * is the main terminal emulation entry point.
 *
 * Args:
 * text - array of characters to process
 * len  - number of characters in the text array
 *
 * The emulator is implemented as a state machine which can handle
 * breaks in character sequences at any point.
 */
/* emulAddTextServer: calls logSession, emuli_BeforeAddText, emuli_AfterAddText */
void emulAddTextServer (const char* text, int len)
{
    int dtcc;
    ConstBuffData bd;
    uint32_t c;
    IterStruct itctx;
    int dtcc2;	/* if dtcc==UTFx then dtcc2==UTF32, otherwize dtcc2==dtcc */

    bd.ptr= text;
    bd.len= len;

    dtcc= emulGetServerDtcc (&currEmul);

    logSession ((char *)bd.ptr, bd.len);			/* <FIXME> unicode */
    emuli_BeforeAddText ();

    dtcc2= emuli_IterStart (&itctx, &bd, dtcc, &FromServerUtf8StreamContext, NULL);
    while (emuli_IterNextChar (&itctx, &c)) {
	emuli_AddServerChar (c, dtcc2, &term.state);
    }

    emuli_AfterAddText ();
}

static BOOL cursorHidden;

static void emuli_BeforeAddText (void)
{
    cursorHidden = term.cursorVisible;

    if (termBottomOnOutput())
	termScrollToBottom();

    /* Hide the caret while performing updates to avoid windows
     * leaving ghost carets all over the place (good one
     * Microsoft).
     */
    if (cursorHidden)
	termHideCaret();
}

static void emuli_AfterAddText (void)
{
    /* Force Windows to update the terminal window
     */
    winUpdate();
    /* Move the caret to the new location and redisplay it.
     */
    winCaretPos(&term.cursor.pos);
    if (cursorHidden)
	termShowCaret();

    /* Update the history scrollbar
     */
    winSetScrollbar();
}

void emulAddText (const void *text, int len, int dtcc, int sourceCode)
{
    ASSERT(text != NULL && len >= 0);

    /* Log the text if necessary
     */
    logSession((char *)text, len);			/* <FIXME> unicode */

    emuli_BeforeAddText ();

    if (sourceCode != 0) {				/* <FIXME> keyboard vs clipboard */
	ConstBuffData b;

	b.ptr= text;
	b.len= len;
	emuli_AddKeybClipText (&b, dtcc, sourceCode, FALSE);

    } else {
	/* data from server is not implemented yet in this function */
	exit (72);
    }

    emuli_AfterAddText ();
}

static void emuli_AddKeybClipChar (uint32_t c, int dtcc, int sourceCode, BOOL fDoPrintFrame)
{

    if ((sourceCode==SOURCE_KEYBOARD  && c==0x0d) ||
	(sourceCode==SOURCE_CLIPBOARD && c==0x0a)) {
	BOOL fLocal= term.echoKeystrokes || !socketConnected();

	if (fLocal) emuli_FuncKeyPress (EFK_LOCAL_SEND, NULL, 0, fDoPrintFrame);
	else        emuli_KeyPress     ('\r', DTCHAR_ASCII, fDoPrintFrame);

    } else if (c<=0x1f || c==0x7f ||
	       (dtcc==DTCHAR_UNICODE && (c>=0x80 && c<=0x9f))) {
	/* nope */

    } else { /* Let's assume it is a printable character */
	emuli_AddChar (c, dtcc, TRUE, fDoPrintFrame);
    }
}

static void emuli_AddKeybClipText (const ConstBuffData *b, int dtcc,
    int sourceCode, BOOL fDoPrintFrame)
{
    uint32_t c;
    IterStruct itctx;
    int dtcc2;	/* if dtcc==UTFx then dtcc2==UTF32, otherwize dtcc2==dtcc */
    Utf16StreamContext FromClipboardUtf16StreamContext= Empty_Utf16StreamContext;

    if (!b || !b->len) return;
    if (fDoPrintFrame) emuli_BeforeAddText ();

    if (sourceCode==SOURCE_KEYBOARD) {
	dtcc2= emuli_IterStart (&itctx, b, dtcc,
	    (Utf8StreamContext *)NULL, (Utf16StreamContext *)NULL);
    } else {
	dtcc2= emuli_IterStart (&itctx, b, dtcc,
	    (Utf8StreamContext *)NULL, &FromClipboardUtf16StreamContext);
    }
    while (emuli_IterNextChar (&itctx, &c)) {
	emuli_AddKeybClipChar (c, dtcc2, sourceCode, FALSE);
    }

    if (fDoPrintFrame) emuli_AfterAddText ();
}

static int isControl (uint32_t c)
{
    return c<=0x1f || c==0x7f ||
	  (c>=0x80 && c<=0x9f);
}

static void emuli_AddServerChar (uint32_t c, int dtcc, InputParsingState *pst)
{
	if (pst->state == consInStringParam) {
	    if (isControl (c)) {
		if (c==0x9C ||	/* ST  (string terminator) */
		    c==0x07) {	/* BEL (also string terminator) */
		    emulTableSeq (pst);
		    emulParamInit (pst, consNormal, 0);

		} else if (c==0x1B) { /* ESC \ is same as ST (we are expecting '\' now) */
		    pst->state = consInStringEnd;

		} else {
		    /* emulTableSeq (pst);incomplete/bogus sequence */
		    emulParamInit (pst, consNormal, 0);
		}

	    } else {
		if (pst->stateSParam) { /* expecting hex digit */
		    if (('0'<=c && c<='9') ||
			('a'<=c && c<='f') ||
			('A'<=c && c<='F')) {
			emulParamStringChar (pst, c);
		    } else {
			emulParamInit (pst, consNormal, 0);
		    }
		} else {
		    emulParamStringChar (pst, c);
		}
	    }
	    return;

	} else if (pst->state == consInStringEnd) {
	    if (c=='\\') { /* second part of sequence ESC\ */
		emulTableSeq (pst);

	    } else {
		/* emulTableSeq (pst); incomplete/bogus sequence */
	    }
	    emulParamInit (pst, consNormal, 0);
	    return;
	}

	/* Some characters are state independant - handle those first
	 */
	switch (c) {
	case 0:    /* NUL -- CTRL+@ */
	    return;
	case 5:    /* ENQ -- CTRL+E */
	    emulTransmitAnswerback();
	    return;
	case 7:    /* BEL -- CTRL+G */
	    MessageBeep(MB_ICONASTERISK);
	    return;
	case '\b': /* BS  -- CTRL+H */
	    emulBackspace(FALSE);
	    return;
	case '\t': /* HT  -- CTRL+I */
	    emulTab();
	    return;
	case '\n': /* LF  -- CTRL+J */
	case 11:   /* VT  -- CTRL+K */
            if (printingIsActive())
                printingAddChar('\n');
            else
	        emulLineFeed(pst, 0);
	    return;
	case 12:   /* FF  -- CTRL+L */
	    if (printingIsActive())
	        printingAddChar(12);
	    else
	        emulLineFeed(pst, 0);
	    return;
	case '\r': /* CR  -- CTRL+M */
	    emulCarriageReturn();
	    return;
	case 14:   /* SO  -- CTRL+N */
	    emulCharSet(1);
	    return;
	case 15:   /* SI  -- CTRL+O */
	    emulCharSet(0);
	    return;
	case 17:   /* DC1 -- CTRL+Q (aka XON) */
	    emulResumeTransmit();
	    return;
	case 19:   /* DC3 -- CTRL+S (aka XOFF) */
	    emulHaltTransmit();
	    return;
	case 24:   /* CAN -- CTRL+X */
	case 26:   /* SUB -- CTRL+Z */
	    pst->state = consNormal;
	    emulAddCheckerboard();
	    return;
	case 27:   /* ESC -- CTRL+[ */
	    emulParamInit (pst, consEscape, c);
	    return;
 	case 127:  /* DEL -- CTRL+? */
	    return;
	}

	switch (pst->state) {
	case consNormal:
	    if (c == 0x90) { 	/* DCS -- same as ESC P	*/
		emulParamInit (pst, consDCS, c);
		pst->stateSParam= 0;	/* expecting 'normal string' */
		return;

	    } else if (c == 0x9B) { /* CSI -- same as ESC [ */
		emulParamInit (pst, consOpenSquare, c);
		return;

	    } else if (c == 0x9D) { /* OSC -- same as ESC ] */
		emulParamInit (pst, consCloseSquare, c);
		return;

	    } else if (c >= 0x80 && c <= 0x9f) { /* single-character control */
		emulParamInit (pst, consNormal, c);
		emulTableSeq (pst);

	    } else {
		emuli_AddChar (c, dtcc, FALSE, FALSE);
	    }
	    return;

	case consDCS:				/* ESC P 1 v/x hexstring ST */
						/* ESC P   v/x characters ST */
	    if (c == '1') {
		pst->stateSParam = 1;		/* hexadecimal value */
		pst->state = consDCS1;
		return;
	    }
	    goto case_consDCS1;

	case consDCS1:
	case_consDCS1:
	    pst->numSParam= 0;			/* no data yet */
	    pst->chFinal= (char)c;		/* for now: v/x */
	    pst->state = consInStringParam;
	    return;
	case consEscape:		/* ESC  */
	    emulEscapeChr (&term.state, c);
	    return;

	case consCloseSquare:		/* ESC ] param; string BEL */
	    if (c >= '0' && c <= '9') {
		emulParamDigit (pst, c-'0');
	    } else if (c==';') {
		emulParamInit (pst, consInStringParam, 0x9D);	/* OSC */
		pst->stateSParam= 0;	/* expecting 'normal string' (not hexcodes) */
	    } else {			/* invalid -- ignore */
		emulParamInit (pst, consNormal, 0);
	    }
	    return;

	case consOpenSquare:	/* ESC [ or CSI */
	    if (c == ';') {
		emulParamNext(pst);
		return;

	    } else if (c >= '0' && c <= '9') {
		emulParamDigit (pst, c - '0');
		return;

	    } else if (c>=0x20 && c<=0x3f) {
		if (pst->nImd==0) {
		    if (c == '?') {
			pst->seenQuestion = TRUE;
		    }
		    pst->chFirstImd= (char)c;

		} else if (pst->nImd>=1) {
		    pst->chLastImd= (char)c;
		}
		++pst->nImd;
		return;

	    } else {
		BOOL solved;
		pst->chFinal = (char)c;
		
		solved= emulTableSeq (&term.state);
		if (! solved) {
		    emulCSI (&term.state);
		}
		pst->state = consNormal;
		return;
	    }

	case consHash:
	    /* Double height/width commands
	     */
	    pst->state = consNormal;
	    emulSetDouble(c);
	    return;

	case consOpenParen:
	    /* Change G0 character set
	     */
	    pst->state = consNormal;
	    switch (c) {
	    case 'A': setTermCharSet (&term.G0, TCSN_UK); break;
	    case 'B': setTermCharSet (&term.G0, TCSN_US); break;
	    case '0': setTermCharSet (&term.G0, TCSN_GRAPHICS); break;
	    case '1': setTermCharSet (&term.G0, TCSN_ALT); break;
	    case '2': setTermCharSet (&term.G0, TCSN_ALTGRAPH); break;
	    }
	    return;

	case consCloseParen:
	    /* Change G1 character set
	     */
	    pst->state = consNormal;
	    switch (c) {
	    case 'A': setTermCharSet (&term.G1, TCSN_UK); break;
	    case 'B': setTermCharSet (&term.G1, TCSN_US); break;
	    case '0': setTermCharSet (&term.G1, TCSN_GRAPHICS); break;
	    case '1': setTermCharSet (&term.G1, TCSN_ALT); break;
	    case '2': setTermCharSet (&term.G1, TCSN_ALTGRAPH); break;
	    }
	    return;

	case consInStringParam:	/* handled earlier! */
	case consInStringEnd:
	    return;
	}
}

static void emulCSI (InputParsingState *pst)
{
    int c= pst->chFinal;

/* supported DEC sequences: */
    if (c=='h') {
        emulSetMode(TRUE);
        return;

    } else if (c=='l') {
	emulSetMode(FALSE);
	return;
    }

    if (pst->seenQuestion) {
	return;
    }

	    /* None of these can have '?'
	     */
	    switch (c) {
	    case 'A':
		emulCursorUp();
		break;
	    case 'C':	/* CUF = Cursor Forward */
	    case 'a':	/* HPR = Horizontal Position Relative */
		emulCursorRight();
		break;
	    case 'D':
		emulCursorLeft();
		break;
	    case 'F':	/* CPL Cursor Previous Line */
		emulCursorUp();
		term.cursor.pos.x = 0;
		break;
	    case 'H':	/* CUP Cursor Position */
	    case 'f':	/* HVP Horizontal & Vertical Position */
		emulCursorAddress();
		break;
	    case 'G':	/* CHA = Cursor Horizontal Absolute */
	    case '`':	/* HPA = Horizontal Position Absolute */
		emulColumnAddress();
		break;
	    case 'd':	/* VPA = Vertical Position Absolute */
		emulRowAddress();
		break;

	    case 'K':
		emulClearInLine();
		break;
	    case 'J':
		emulClearEndOfScreen();
		break;
	    case 'X':
		emulCharBlank();
		break;
	    case 'P':
		emulCharDelete(FALSE);
		break;
	    case 'S':			/* SU */
		emulPanUpDown(0);
		break;
	    case 'T':			/* SD */
		emulPanUpDown(1);
		break;
	    case 'r':			/* DECSTBM */
		emulScrollRegion();
		break;
	    case 'm':
		emulParamInterpret();
		break;
	    case 'g':
		emulTabClear();
		break;
	    case 's':
		emulCursorSaveRestore (CRSR_SAVE);
		break;
	    case 'u':
		emulCursorSaveRestore (CRSR_RESTORE);
		break;
            case 'i':
                emulPrintControl();
                break;
	    case 't':
		emulWinManip ();
	    }
}

static void emulReqMode (InputParsingState *pst, BOOL fDec)
{
    int parnum;
    int sendval= -1;
    char buff [32], *p= buff;
/*
0 	Mode not recognized
1 	Set
2 	Reset
3 	Permanently set
4 	Permanently reset
*/
#define REPLYCODE(n) ((n)!=0?1: 2)

    if (pst->numParams != 1) return;
    parnum= pst->params[0];
    if (fDec) {
	switch (parnum) {
	case 1:
	    sendval= REPLYCODE (term.ansiCursorKeys);	/* DECCKM */
	    break;
	case 2:
	    sendval= 4;	/* DECANM: VT52 mode -- not supported */
	    break;
	case 3:
	    if	    (term.winSize.cx==132 && term.winSize.cy==24) sendval= 1;
	    else if (term.winSize.cx== 80 && term.winSize.cy==24) sendval= 2;
	    else						  sendval= 0;
	    break;
	case 4:		/* DECSCLM: scroll=jump */
	    sendval= 4;
	    break;
	case 5:		/* DECSCNM */
	    sendval= REPLYCODE (term.attr.inverseVideo);
	    break;
	case 6:		/* DECOM */
	    sendval= REPLYCODE (term.scroll.fRestrict);
	    break;
	case 7:					/* DECAWM */
	    sendval= REPLYCODE (term.lineWrap);
	    break;
	case 8:					/* DECARM */
	    sendval= REPLYCODE (term.autoRepeat);
	    break;
	case 9:					/* XT_MSE_X10 */
	    sendval= REPLYCODE (term.mouseRep.mouseReporting==TERM_MOUSE_REP_X10);
	    break;
	case 1000:				/* XT_MSE_X11 */
	    sendval= REPLYCODE (term.mouseRep.mouseReporting==TERM_MOUSE_REP_X11);
	    break;
	case 1001:				/* XT_MSE_HL */
	    sendval= REPLYCODE (term.mouseRep.mouseReporting==TERM_MOUSE_REP_X11_HIGHLIGHT);
	    break;
	case 1002:				/* XT_MSE_BTN */
	    sendval= REPLYCODE (term.mouseRep.mouseReporting==TERM_MOUSE_REP_X11_BTN_EVENT);
	    break;
	case 1003:				/* XT_MSE_ANY */
	    sendval= REPLYCODE (term.mouseRep.mouseReporting==TERM_MOUSE_REP_X11_ANY_EVENT);
	    break;
	case 47:	/* XT_ALTSCRN */
	case 1047:	/* XT_ALTS_47 */
	case 1049:	/* XT_EXTSCRN */
	    sendval= REPLYCODE (term.altbuff);
	    break;
	default:
	    sendval= 0;
	}
    } else {
	switch (parnum) {
	case 20: sendval= term.newLineMode? 1: 2;    break;	/* LNM */
	default: sendval= 0;
	}
    }
    if (sendval>=0) {
	ConstBuffData b;

	p += emulC1 (p, '[');	/* ESC [ or CSI */
	if (fDec) *p++ = '?';
	p += sprintf (p, "%d;%d$y", parnum, sendval);
	b.ptr= buff;
	b.len= p - buff;
	emuli_FuncKeyPress (EFK_LOCAL_SKIP, &b, DTCHAR_ISO88591, FALSE);
    }
#undef REPLYCODE
}

static void emulEscapeChr (InputParsingState *pst, unsigned c)
{
    /* <FIXME> This should be properly refactored... */
    pst->state = consNormal;

    switch (c) {
    case 'N':
	emulCharSet(1);
	break;

    case 'O':
	emulCharSet(0);
	break;

    case 'M':
	emulReverseLineFeed();
	break;

    case '7':
	emulCursorSaveRestore (CRSR_SAVE);
	break;

    case '8':
	emulCursorSaveRestore (CRSR_RESTORE);
	break;

    case 'H':
	emulTabSet();
	break;

    case '[':			/* ESC [ -- same as CSI */
	emulParamInit (pst, consOpenSquare, (unsigned char)0x9B);
	break;

    case ']':			/* ESC ] -- same as OSC */
	emulParamInit (pst, consCloseSquare, (unsigned char)0x9D);
	break;

    case '#':
	emulParamInit (pst, consHash, '\x1b');
	pst->chFirstImd= (char)c;
	++pst->nImd;
	break;

    case '(':
	emulParamInit (pst, consOpenParen, '\x1b');
	pst->chFirstImd= (char)c;
	++pst->nImd;
	break;

    case ')':
	emulParamInit (pst, consCloseParen, '\x1b');
	pst->chFirstImd= (char)c;
	++pst->nImd;
	break;

    case 'P':			/* ESC P -- same as DCS */
	emulParamInit (pst, consDCS, (unsigned char)0x90);
	pst->stateSParam= 0;	/* expecting 'normal string' */
	break;

    default:
	if (c >= 0x20 && c <= 0x2F) { /* intermediate */
	    if (pst->nImd==0) {
		pst->chFirstImd= (char)c;
	    }
	    ++pst->nImd;
	    pst->state = consEscape;
	    break;

	} else if (pst->nImd==0 &&
		   c >= 0x40 && c <= 0x5F) { /* 7 bit -> 8 bit,
					       eg: ESC H (1B 48) -> HTS (88)
						   ESC _ (1B 5F) -> APC (9F) */
	    int c8bit;

	    c8bit= 0x80 + (c-0x40);
		    /* <FIXME> DCS, CSI, OSC should be handled specially: */
		    /* they don't end the sequence */
		    /* (they have been handled individually earlier) */

	    pst->chSeq= (char)c8bit;

	} else {
	    pst->chFinal= (char)c;
	}

	emulTableSeq (pst);
	emulParamInit (pst, consNormal, 0);
    }
}

/* Handle a normal key press
 */
static void emuli_KeyPress (uint32_t ch, int dtcc, BOOL fDoPrintFrame)
{
    if (!socketConnected() || term.echoKeystrokes) {
	/* When in local echo mode, or not connected, echo the character
	 */
	emuli_AddKeybClipChar (ch, dtcc, SOURCE_KEYBOARD, fDoPrintFrame);

	if (!socketConnected()) return;
    }

/*  if (!socketConnected())
	return; */

    /* Do local flow control first - this needs special handling
     */
    if (ch == CTRL('S') ||
	ch == CTRL('Q')) {
	/* Stop/Resume character flow.
	 */
	if (rawGetProtocol() != protoNone && socketLocalFlowControl()) {
	    /* We are doing either telnet or login protocol, and we
	     * have negotiated local flow control.  Handle the flow
	     * control by not reading any more data from the
	     * socket.
	     */
	    socketSetFlowStop (ch == CTRL('S')); /* S/Q => TRUE/FALSE => Stop/Resume */

	} else {
	    /* Flow control is handled by the remote end.  Send the ^S/^Q.
	     */
	    socketWriteByte (ch);
	}
	return;
    }

    /* Now handle normal characters.
     */
    if (socketFlowStopped()
	&& rawGetProtocol() != protoNone
	&& socketLocalFlowControl()
	&& term.flowStartAny) {
	/* Flow is stopped and we have negotiated flow restart on any
	 * keystroke.  Use the keystroke to restart flow.
	 */
	socketSetFlowStop(FALSE);

    } else {
	/* Send the character to the remote end.
	 */
	if (ch == '\r') {
	    /* Carriage return gets special treatment.
	     */
	    switch (rawGetProtocol()) {
	    case protoTelnet: {
		/* it is a bit difficult:
		   when term.newLineMode==FALSE (default setting; LNMIRM=reset ESC[20l),
			we might send \r\0 or \r\n; both result ^M on the server
		   when term.newLineMode==TRUE (LNMIRM=set ESC[20h),
			we send '\r\0\n' which results ^M^J on the server
		 */
		static const char CrNullLf [3]= {'\r', '\0', '\n'};
		static const char CrNull   [2]= {'\r', '\0'};

		if (term.newLineMode) {
		    socketWrite(CrNullLf, 3);
		} else {
		    socketWrite(CrNull, 2);
		}
		break; }

	    case protoRlogin:
		/* For login protocol, send only the Carriage Return
		 */
		if (term.newLineMode)
		    socketWrite("\r\n", 2);
		else
		    socketWrite("\r", 1);
		break;

	    case protoNone:
		/* For all other protocols, send Carriage Return
		 * followed by Line Feed.  This satisfies things like
		 * SMTP, NNTP, HTTP, FTP, ...
		 */
		socketWrite("\r\n", 2);
		break;
	    }

	} else {
	    /* Normal character - just send it
	     */
	    char sendbuff [16];
	    int sendlen;

	    sendlen= uconvCharToServer (ch, dtcc, sendbuff, &currEmul);
	    if (sendlen) {
		socketWrite (sendbuff, sendlen);
	    }
	}
    }
}

void emulKeyPress (uint32_t ch, int dtcc)
{
    emuli_KeyPress (ch, dtcc, TRUE);
}

/* Handle a function key press local/remote
*/
void emulFuncKeyPress2 (int localaction, const char *str, int dtcc)
{
    ConstBuffData b= EmptyBuffData;

    if (str) {
	b.ptr= str;
	b.len= strlen (b.ptr);
    }
    emuli_FuncKeyPress (localaction, &b, dtcc, TRUE);
}

static void emuli_KeyboardLocalAction (int localaction, const ConstBuffData *b, int dtcc, BOOL fDoPrintFrame)
{
	if (localaction==EFK_LOCAL_SKIP) return;

	if (fDoPrintFrame) emuli_BeforeAddText ();

	switch (localaction) {
	case EFK_LOCAL_SKIP:
	    break;

	case EFK_LOCAL_SAME:
	    if (b && b->len) {
		emuli_AddKeybClipText (b, dtcc, SOURCE_KEYBOARD, FALSE);
	    }
	    break;

	case EFK_LOCAL_BACKSPACE:
	    emulBackspace(TRUE);
	    break;

	case EFK_LOCAL_DELETE:
	    emulCharDeleteCore(1, TRUE);
	    break;

	case EFK_LOCAL_INSERT:
	    term.insertMode = ! term.insertMode;
	    break;

	case EFK_LOCAL_LEFT:
	case EFK_LOCAL_RIGHT:
	case EFK_LOCAL_UP:
	case EFK_LOCAL_DOWN:
	case EFK_LOCAL_HOME:
	case EFK_LOCAL_END:
	    editMoveCursor(localaction);
	    break;

	case EFK_LOCAL_SEND:
	    if (socketConnected()) {
		TerminalLineNo lastline;

		emulSendCurrentLine (&lastline);
		term.cursor.pos.x = 0;
		cursorUpDownScroll (lastline + 1 - term.cursor.pos.y);
	    } else {
		emulLineFeed (&term.state, 1);
	    }
	    break;

	default:
	    break;
	}

	if (fDoPrintFrame) emuli_AfterAddText ();
}

static void emuli_FuncKeyPress (int localaction, const ConstBuffData *b, int dtcc, BOOL fDoPrintFrame)
{
    if (localaction != EFK_LOCAL_SKIP &&
	(term.echoKeystrokes || !socketConnected())) {
	/* When in local echo mode, or not connected, echo the key sequence
	 */
	emuli_KeyboardLocalAction (localaction, b, dtcc, fDoPrintFrame);
    }

    if (socketConnected() && !term.echoKeystrokes) {
	/* When connected and not local echo mode send to remote end */
	if (socketFlowStopped()
	    && rawGetProtocol() != protoNone
	    && socketLocalFlowControl()
	    && term.flowStartAny)
	    /* Flow is stopped and we have negotiated flow restart on any
	    * keystroke.  Use the keystroke to restart flow.
	    */
	    socketSetFlowStop(FALSE);
	else {
	    if (b && b->len) {
	    /* Send the function key sequence to the remote end
	    */
		DynBuffer btmp= EmptyBuffer;

		uconvBytesToServer (b, dtcc, &btmp, &currEmul, NULL);	/* FIXME: too many malloc's */
	        if (btmp.len) socketWrite (btmp.ptr, btmp.len);
		BuffCutback (&btmp, 0);
	    }
	}
    }
}

/* Handle a function key press local/remote
*/
void emulFuncKeyPressB (int localaction, const ConstBuffData *b, int dtcc)
{
    emuli_FuncKeyPress (localaction, b, dtcc, TRUE);
}

/* Set terminal local echo mode on or off
 */
void emulSetEcho(BOOL echo)
{
    term.echoKeystrokes = echo;
}

/* Set terminal local flow control "resume with any character" on or off
 */
void emulLocalFlowAny(BOOL any)
{
    term.flowStartAny = any;
}

char *emulShCtrlMagic (const TermKey *tk, const char *from, char *to, int emulflags)
{
    int shctrlmagic;
    int froml;
    int modcode, modchrctl;
    int tmodifiers= tk->modifiers;
    unsigned char actions= 0;
#define ACT_PRE_ESC   0x01	/* ESC [ A    ->  ESC ESC [ A  (rxvt Alt+Up) */
#define ACT_CSI_2_SS3 0x02	/* ESC [ A    ->  ESC O A      (not a real sequence, only an example) */
#define ACT_INS_1     0x04	/* ESC [ H    ->  ESC [ 1 H    (not a real sequence, only an example) */
#define ACT_INS_SC    0x08	/* ESC [ 6 ~  ->  ESC [ 6 ; ~  (not a real sequence, only an example) */
#define ACT_INS_VAR   0x10	/* ESC O P    ->  ESC [ O 2 P  (konsole Shift+F1) */
#define ACT_LTR_TLOW  0x20	/* ESC [ A    ->  ESC [ a      (rxvt Shift+Up) */
#define ACT_TRM_VAR   0x40	/* ESC [ 2 ~  ->  ESC [ 2 ^    (rxvt Ctrl+End) */
    const char *p;
    char *q;

    if ((tmodifiers&(TMOD_SHIFT|TMOD_CONTROL|TMOD_ALT))==0) return (char *)from;

    modcode= 1;
    modchrctl= 0;				/* for rxvt terminator character: ~ $ ^ @ = base shift ctrl shift+ctrl */
    if (tmodifiers&TMOD_SHIFT)   modcode += 1, modchrctl += 1;
    if (tmodifiers&TMOD_ALT)     modcode += 2;
    if (tmodifiers&TMOD_CONTROL) modcode += 4, modchrctl += 2;

    shctrlmagic= emulflags&ETF_SHCTRL_MASK;
    if (!shctrlmagic) return (char *)from;

    froml= strlen (from);
    if (from[0]!='\x1b' ||
	(froml<3 || froml>8) ||
	(froml==3 && !isupper((unsigned char)from[froml-1])) ||
	(froml>3 &&
	    (!isdigit((unsigned char)from[froml-2]) ||
	     (from[froml-1]!='~' && from[froml-1]!='$')))) return (char *)from;


    if (shctrlmagic==ETF_SHCTRL_XTERM_NEW ||
	shctrlmagic==ETF_SHCTRL_KONSOLE) {
	if (shctrlmagic==ETF_SHCTRL_KONSOLE && tk->classe==TKC_F_14) {
	    actions= ACT_INS_VAR; /* special case */
	} else {
	    actions= ACT_INS_1 | ACT_INS_SC | ACT_INS_VAR; /* default case */
	}

    } else if (shctrlmagic==ETF_SHCTRL_RXVT) {
	if (tmodifiers&TMOD_ALT) actions |= ACT_PRE_ESC;

	if (froml==3 && isupper((unsigned char)from[froml-1])) {
	    if (tmodifiers&TMOD_SHIFT)         actions |= ACT_LTR_TLOW;
	    else if (tmodifiers&TMOD_CONTROL)  actions |= ACT_CSI_2_SS3 | ACT_LTR_TLOW;

	} else if (from[froml-1]=='~') {
	    actions |= ACT_TRM_VAR;

	} else if (from[froml-1]=='$' && (tmodifiers&TMOD_CONTROL)) { /* special case: ESC[num$ + Shift --> ESC[num@ (eg: Shift+Ctrl+F11) */
	    actions |= ACT_TRM_VAR;
	    modchrctl= 3;	/* results '@' at the end of sequence */
	}

    } else { /* others: the only thing we do is adding ALT... and even that is arguable */
	actions |= ACT_PRE_ESC;
    }

    p= from;
    q= to;

    if ((actions&ACT_PRE_ESC) & (tmodifiers&TMOD_ALT)) {
	*q++= '\x1b';	/* extra ESC */
    }

    if (*p=='\x1b') {
	*q++= *p++;
	if (actions&ACT_CSI_2_SS3) *q++= 'O';
	else			   *q++= *p;
	++p;

    } else if (*p=='\x8f' || *p=='\x9b') {
	if (actions&ACT_CSI_2_SS3) *q++= '\x8f';	/* SS3 */
	else			   *q++= *p;		/* CSI */
	++p;

    } else return (char *)from;

    if ((actions&ACT_INS_1) && !isdigit ((unsigned char)*p)) *q++ = '1';
    while (isdigit ((unsigned char)*p)) {
	*q++= *p++;
    }

    if (actions&ACT_INS_SC)  *q++= ';';
    if (actions&ACT_INS_VAR) *q++= (char)('0' + modcode);

    if ((actions&ACT_LTR_TLOW) && isupper ((unsigned char)*p)) {
	*q++ = (char) tolower ((unsigned char)*p);

    } else if ((actions&ACT_TRM_VAR) && 
	       ((*p=='~') || (*p=='$' && modchrctl==3))) {
	*q++= "~$^@"[modchrctl];

    } else {
	*q++= *p;
    }
    *q= '\0';

    return to;
}

/* ansi cursor mode: ESC [ A -> ESC O A
   It affects Up/Down/Left/Right,
   and for xterm Home/End/Num5
   I honestly don't know what it is good for
 */
static char *emulAnsiCursorMode (const char *from, char *to)
{
    int froml;

    froml= strlen (from);
    if (froml!=3 ||
	from[0]!='\x1b' ||
	from[1]!='[' ||
	strchr ("ABCDEFH", from[2])==NULL) { /* Yuck, harcoded list -- it is ugly, I know */
	return (char *)from;
    }
    memcpy (to, from, froml+1);
    to[1]= 'O';
    return to;
}

const char *emulGetKeySeq (const TermKey *tk, BOOL fAnsiCrsrMode)
{
    const char *retp;
    int shctrlmagic= 0;
static char tmp [32];
    int tmodifiers= tk->modifiers;

    switch (tk->keycode) {
    case VK_PRIOR:
	retp= currEmul.keyPageUp;
	shctrlmagic= 2;
	break;

    case VK_NEXT:
	retp= currEmul.keyPageDown;
	shctrlmagic= 2;
	break;

    case VK_HOME:
	retp= currEmul.keyHome;
	shctrlmagic= 2;
	break;

    case VK_END:
	retp= currEmul.keyEnd;
	shctrlmagic= 2;
	break;

    case VK_INSERT:
	retp= currEmul.keyInsert;
	shctrlmagic= 2;
	break;

    case VK_DELETE:
	retp= currEmul.keyDelete;
	shctrlmagic= 2;
	break;

    case VK_BACK:
	retp= currEmul.keyBackSpace;
	break;

    case VK_UP:
	retp= "\x1b[A";
	shctrlmagic= 1;
	break;

    case VK_DOWN:
	retp= "\x1b[B";
	shctrlmagic= 1;
	break;

    case VK_RIGHT:
	retp= "\x1b[C";
	shctrlmagic= 1;
	break;

    case VK_LEFT:
	retp= "\x1b[D";
	shctrlmagic= 1;
	break;

    case VK_CLEAR: /* Numpad5 */
	retp= currEmul.keyNum5;
	shctrlmagic= 2;
	break;

    default:
	retp= "";
    }

    if ((currEmul.flags & ETF_SHCTRL_MASK) && shctrlmagic &&
	(tmodifiers & (TMOD_CONTROL | TMOD_SHIFT | TMOD_ALT))) {
	retp= emulShCtrlMagic (tk, retp, tmp, currEmul.flags);

    } else if (fAnsiCrsrMode) {
	retp= emulAnsiCursorMode (retp, tmp);
    }

    return retp;
}

void emulRelBuff (Emul *ep)
{
    int i;

    for (i=0; i<MAX_F_KEY; ++i) {
	if (ep->keyFn[i].ptr) free (ep->keyFn[i].ptr);
    }
    memset (ep, 0, sizeof *ep);
}

typedef struct SeqTableKey {
    unsigned char chSeq;	/* ESC (0x1b) or CSI (0x9b) or ...
				   Note: ESC P is stored as CSI */
    unsigned char chFirstImd;	/* first 'intermediate' character after ESC[	   */
    unsigned char chLastImd;
    unsigned char chFinal;	/* the terminating character;			   */
} SeqTableKey;

typedef struct SeqTable {
    SeqTableKey key;
    void (*funptr)(InputParsingState *pst, int opt1);
    int opt1;
} SeqTable;

/* keep sorted by chSeq, chFirstImd, chlastImd, chFinal */
static const SeqTable sqtab [] = {
    {{0x1b,  0,   0,  '6'}, emulDecBiFi,     0},	/*	      ESC 6	-- DECBI Backward Index */
    {{0x1b,  0,   0,  '9'}, emulDecBiFi,     1},	/*	      ESC 9	-- DECFI Forward Index */
    {{0x1b,  0,   0,  '='}, emulKeyPadMode,  1},	/*            ESC =     -- DECKPAM */
    {{0x1b,  0,   0,  '>'}, emulKeyPadMode,  0},	/*            ESC >     -- DECKPNM */
    {{0x1b,  0,   0,  'c'}, emulResetTerm1,  1},	/*	      ESC c	-- Hard reset (RIS) */
    {{0x1b, ' ',  0,  'F'}, emulControlBits, 7},	/*            ESC sp F  -- S7C1T */
    {{0x1b, ' ',  0,  'G'}, emulControlBits, 8},	/*            ESC sp G  -- S8C1T */
    {{0x84,  0,   0,   0 }, emulLineFeed, 0},		/* IND     -- ESC D     -- Index */
    {{0x85,  0,   0,   0 }, emulLineFeed, 1},		/* NEL     -- ESC E     -- Next Line */
    {{0x90,  0,   0,  'v'}, emulSetAnswerBk, 0},	/* DCS [1] v chars ST   -- Set AnswerBack */
    {{0x90,  0,   0,  'x'}, emulExecute, 0},		/* DCS [1] x chars ST   -- LocalExecute */
    {{0x9a,  0,   0,   0 }, emulStatusReport, 0},	/* SCI     -- ESC Z     -- Send Primary Device Attributes */
    {{0x9b,  0,   0,  '@'}, emulCharInsert, 0},		/* CSI @   -- ESC [ @	-- Insert character */
    {{0x9b,  0,   0,  'B'}, emulCursorDown, 0},		/* CSI B   -- ESC [ B	-- Cursor Down */
    {{0x9b,  0,   0,  'E'}, emulCursorDown, 1},		/* CSI E   -- ESC [ E	-- Cursor Next Line */
    {{0x9b,  0,   0,  'I'}, emulTabForward, 0},		/* CSI I   -- ESC [ I	-- CHT Tab Forward */
    {{0x9b,  0,   0,  'L'}, emulLineInsert, 0},		/* CSI L   -- ESC [ L	-- Insert Line (IL) */
    {{0x9b,  0,   0,  'M'}, emulLineInsert, 1},		/* CSI M   -- ESC [ M	-- Insert Line (DL) */
    {{0x9b,  0,   0,  'Z'}, emulTabForward, 1},		/* CSI Z   -- ESC [ Z	-- CBT Tab Backward */
    {{0x9b,  0,   0,  'c'}, emulStatusReport, 0},	/* CSI c   -- ESC [ c   -- Send Primary Device Attributes */
    {{0x9b,  0,   0,  'e'}, emulCursorDown, 0},		/* CSI e   -- ESC [ e	-- Vertical Postion Relative */
    {{0x9b,  0,   0,  'n'}, emulDSR, 0},		/* CSI n   -- ESC [ n   -- Send Device Status Report - ANSI */
    {{0x9b,  0,   0,  'x'}, emulStatusReport, 10},	/* CSI x   -- ESC [ x   -- Send Terminal Parameters */
    {{0x9b, ' ',  0,  '@'}, emulDecBiFi,    2},		/* CSI sp @		-- Scroll Left */
    {{0x9b, ' ',  0,  'A'}, emulDecBiFi,    3},		/* CSI sp A		-- Scroll Right */
    {{0x9b, '!',  0,  'p'}, emulResetTerm1, 0},		/* CSI ! p		-- Soft reset (DECSTR) */
    {{0x9b, '$',  0,  '|'}, emulSetColumns, 0},		/* CSI n $ p		-- Request Mode/Dec (DECSCPP) */
    {{0x9b, '$',  0,  'p'}, emulReqMode, 0},		/* CSI num $ p		-- Request Mode/Ansi (DECRQM) */
    {{0x9b, '=',  0,  'c'}, emulStatusReport, 2},	/* CSI = c -- ESC [ = c -- Send Tertiary Device Attributes */
    {{0x9b, '>',  0,  'c'}, emulStatusReport, 1},	/* CSI > c -- ESC [ > c -- Send Secondary Device Attributes */
    {{0x9b, '?',  0,  'n'}, emulDSR, 1},		/* CSI ? n -- ESC [ ? n -- Send Device Status Report - DEC */
    {{0x9b, '?', '$', 'p'}, emulReqMode, 1},		/* CSI ? n $ p		-- Request Mode/Dec (DECRQM) */
    {{0x9d,  0,   0,   0 }, emulSetTitle, 0}		/* OSC n ; str ST	-- Set WindowTitle */
};

#define Nsqtab ((sizeof sqtab) / (sizeof sqtab[0]))

static int ETS_compare (const void *pkey, const void *pelem)
{
    const InputParsingState *key= (const InputParsingState *)pkey;
    const SeqTable *elem= (const SeqTable *)pelem;
    int cmp;

    cmp= (key->chSeq > elem->key.chSeq) -
	 (key->chSeq < elem->key.chSeq);
    if (!cmp) cmp= (key->chFirstImd > elem->key.chFirstImd) - 
		   (key->chFirstImd < elem->key.chFirstImd);
    if (!cmp) cmp= (key->chLastImd > elem->key.chLastImd) - 
		   (key->chLastImd < elem->key.chLastImd);
    if (!cmp) cmp= (key->chFinal > elem->key.chFinal) - 
		   (key->chFinal < elem->key.chFinal);

    return cmp;
}

static BOOL emulTableSeq (InputParsingState *pst)
{
    const SeqTable *p;
    BOOL retval;

    p= bsearch (pst, sqtab, Nsqtab, sizeof (SeqTable), ETS_compare);
    if (!p) {
	retval= FALSE;
    } else {
	p->funptr (pst, p->opt1);
	retval= TRUE;
    }
    return retval;
}
