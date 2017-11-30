/* term.c
 * Copyright (c) 1997 David Cole
 *
 * Control and manage the terminal window
 */
#include "preset.h"
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <time.h>
#include <sys/timeb.h>

#include "platform.h"
#include "utils.h"
#include "term.h"
#include "termwin.h"
#include "attrib.h"
#include "lines.h"
#include "dtelnet.h"
#include "socket.h"
#include "raw.h"
#include "font.h"
#include "status.h"
#include "emul.h"
#include "log.h"
#include "connect.h"
#include "termkey.h"
#include "scroll.h"

static const char* termWinClass = "DTermWClass";

Terminal term;			/* The terminal */

static BOOL termHasPalette;	/* can we support palettes? */
static HPALETTE termPalette;	/* the palette for the window */
static UINT termTimerId;	/* selection scrolling timer */
static BOOL haveCaret;		/* do we have the Windows Caret? */
static WindowRect update;	/* current update rectangle (before scroll) */
static BOOL haveUpdate;		/* do we have an update rectangle? */
static BOOL bottomOnOutput;	/* do we scroll to bottom on output? */
static BOOL autoCopy = TRUE;	/* do we automatically copy selection
				 * to clipboard? */
static CursorStyle cursorStyle;	/* terminal cursor style */
static int mousePasteButton= 2;	/* 1/2 = middle/right */

static int vscrollWidth;		/* width of a vertical scrollbar */

/* termMargin:

   'unused' space around the 'useful' space
   left:   0+  (it used to be 0, but for some contexts (antialiasing/ClearType/whatnot)
		it should be set to 1 (is that always enough?))
   top:    0   (for now)
   right:  0+  (window width  - termMargin.left)%charWidth
   bottom: 0+  (window height - termMargin.top)%charHeight
 */
PixelRect termMargin = {1, 0, 0, 0};

/* termBlur:

   when antialiasing/ClearType/whatnot requires,
   we have to invalidate more space than required
 */
static SIZE termBlur = {2, 0};

/* .INI File Strings
 */
static const char termStr[]		= "Terminal";
static const char sizeStr[]		= "Size";
static const char autoCopyStr[]		= "Auto Copy";
static const char bottomOnOutputStr[]	= "Bottom On Output";
static const char cursorStyleStr[]	= "Cursor Style";
static const char attachPrinterStr[]	= "Attached Printer";
static const char printScreenStr[]	= "Enable PrintScreen";
static const char attrStr []		= "Attrib";
static const char pasteKeyStr []	= "Mouse Paste Key";

typedef struct CaptureData {
    int flag;	/* are we in SetCapture mode? 0/1 = no/yes */
		/* even is it is 1, we still check if GetCapture()==termWnd */
    int btn;	/* is so, which button did we use to enter SerCapture mode? (0-2) */
} CaptureData;

/* let's try to concentrate */
typedef struct TermData {
    HWND wnd;			/* terminal window handle */
    CaptureData capture;
    int fNumLock;		/* NumLock is On */
    int fCapsLock;		/* CapsLock is On */
} TermData;

static void termSetLockFlags (const TermKey *tk);

static TermData tsd;

static BOOL inCapture (int *pbtn)
{
    int mybtn;
    BOOL retval;

    if (!pbtn) pbtn= &mybtn;
    if (tsd.capture.flag && GetCapture() == tsd.wnd) {
	*pbtn= tsd.capture.btn;
	retval= TRUE;
    } else {
	tsd.capture.flag= 0;
	*pbtn= -1;
	retval= FALSE;
    }
    return retval;
}

/* #define WRITEDEBUGFILE 1 */
#ifdef WRITEDEBUGFILE
    #include "debuglog.h"
    #define TermDebugF(par) dtelnetDebugF par
#else
    #define TermDebugF(par)
#endif
#define TermDebugPaintF(par) /* TermDebugF (par) */
#define TermDebugFillF(par)  /* TermDebugF (par) */

static void termFuncKeyDown (const TermKey *tk, int nFkey);

/* the numeric keypad has a very specific behaviour,
   which has to be undone very carefully */

static BOOL termNumpadKeyDown (TermKey *tk,
	int localaction,
	int notnum_key,
	const char *num_applic);

/* selectCheckOverlap:
    invalidate selection, if overlaps with the range (window-coordinates))
*/
static void selectInvalidate (const SelectData *sd);
static void selectCheckOverlap (const WindowRange *wr);
static void bufferSelectCheckOverlap (const BufferRange *br);
static void termSelectCheckOverlap (const TerminalRange *tr);

void termSetMouseRep (int mode) /* TERM_MOUSE_REP_* */
{
    term.mouseRep.mouseReporting= mode;
    term.mouseRep.fMoveReported= FALSE;
    term.mouseRep.ptLastReported.x= term.mouseRep.ptLastReported.y= 0;
}

void termSetMouseCoord (int cmode) /* TERM_MOUSE_COORD_* */
{
    term.mouseRep.mouseCoord= cmode;
}

/* User pressed a mouse button
 *
 * Args:
 * nButton- code of the pressed button
 *	  - 0/1/2:   Left/Middle/Right
 *        - 3/4/5/6: WheelUp/Down/Left/Right
 *	  - -1: none (with TSME_MOVE)
 * nAction- 0/1/2: press/release/move (TSME_*)
 * flags -  mouse key flags
 * mousepos- the position of the mouse-cursor
 */
static void termSendMouseEvent (int nButton, int nAction, int flags,
	const TermPosition *mousepos);

#define TSME_PRESS    0
#define TSME_RELEASE  1
#define TSME_MOVE     2
#define TSME_DBLCLICK 3	/* will be sent as press */

/*********************
 * WINDOW MANAGEMENT *
 *********************/

/* Move the caret to the specifed character position
 *
 * Args:
 * x - the terminal column to move the caret to
 * y - the terminal line to move the caret to
 */

void winCaretPos (const TerminalPoint *tCaretPos)
{
    WindowPoint wCaretPos;
    PixelPoint  pCaretPos;

    pointTerminalToWindow (tCaretPos, &wCaretPos);
    pointWindowToPixel (&wCaretPos, &pCaretPos);
    SetCaretPos (pCaretPos.x + term.cursorRect.left, pCaretPos.y + term.cursorRect.top);
}

/* Set the history scrollbar to indicate the visible part of the
 * line array
 */
void winSetScrollbar()
{
#ifdef WIN32
    int range = (term.linebuff.used <= term.winSize.cy)
	? term.winSize.cy : term.linebuff.used;
    SCROLLINFO scrollInfo;
    scrollInfo.cbSize = sizeof(scrollInfo);
    scrollInfo.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_POS | SIF_RANGE;
    scrollInfo.nMin = 0;
    scrollInfo.nMax = range - 1;
    scrollInfo.nPage = term.winSize.cy;
    scrollInfo.nPos = term.topVisibleLine;
    scrollInfo.nTrackPos = 0;
    SetScrollInfo(tsd.wnd, SB_VERT, &scrollInfo, TRUE);
#else
    int range = (term.linebuff.used <= term.winSize.cy)
	? 1 : term.linebuff.used - term.winSize.cy;
    SetScrollRange(tsd.wnd, SB_VERT, 0, range, TRUE);
    SetScrollPos(tsd.wnd, SB_VERT, term.topVisibleLine, TRUE);
#endif
}

/* Force windows to update the part of the window we have changed
 */
void winUpdate()
{
    if (haveUpdate) {
	/* Since the terminal is always at the end of the line array,
	 * it is not possible to scroll down past the terminal.  We
	 * can only scroll up.  We only have to check if the top
	 * updated line is still visible.
	 */
	WindowRect fullwindow, wrectInvalid;
	BOOL fHaveSection;

	fullwindow.left= 0;
	fullwindow.top= 0;
	fullwindow.right= term.winSize.cx;
	fullwindow.bottom= term.winSize.cy;
	fHaveSection= rectSection ((const RECT *)&update, (const RECT *)&fullwindow,
		(RECT *)&wrectInvalid);
	if (fHaveSection) {
	    /* The update rect is visible, build a window rect that
	     * encloses the terminal update rect
	     */
	    TermDebugF ((0, "winUpdate calls winInvalidateRect"
			", update: (t=%d,l=%d,b=%d,r=%d), winrect=(t=%d,l=%d,b=%d,r=%d)\n"
			, update.top, update.left, update.bottom, update.right
			, wrectInvalid.top, wrectInvalid.left, wrectInvalid.bottom, wrectInvalid.right));
	    winInvalidateRect (&wrectInvalid);
	}
	haveUpdate = FALSE;
    }
    /* Force windows to perform any pending updates
     */
    UpdateWindow(tsd.wnd);
}

/* Scroll the entire window a number of lines.  Positive values scroll
 * lines up.
 *
 * Args:
 * numLines - the number of lines to scroll
 */
static void winScrollWindow(int numLines)
{
    ScrollWindow(tsd.wnd, 0, -numLines * term.charSize.cy, NULL, NULL);
}

/* The oldest line in the history buffer has been removed
 */
void winTopLineRemoved()
{
    if (term.topVisibleLine == 0)
	winScrollWindow(1);
    else
	term.topVisibleLine--;

    if (term.select.fHave) {
	selectBufferLinesRemoved (&term.select, 0, 1);
    }
}

/* Add an area to the terminal window update rect,
   from tr->from (included) to tr->to (not included)
 */
void termModifyRange (const TerminalRange *tr)
{
    WindowRange wr;
    WindowRect wrect;

    if (tr) {
	rangeTerminalToWindow (tr, &wr);

    } else {
/* from: the first position */
	wr.from.y= 0;
	wr.from.x= 0;
/* to: the position after the last one */
	wr.to.y= term.winSize.cy-1;
	wr.to.x= term.winSize.cx;
    }
    /* Hide the selection if it overlaps with the modified area.
     */
    selectCheckOverlap (&wr);
    rangeToRect ((const Range *)&wr, (RECT *)&wrect);
    rangeToRect ((const Range *)&wr, (RECT *)&wrect);

    if (!haveUpdate) {
	/* No previous update rect.  The modified area is the update
	 * rect
	 */
	update= wrect;
	haveUpdate = TRUE;

    } else {
	/* Merge the modified area with the update rect
	 */
	rectUnion ((const RECT *)&update, (const RECT *)&wrect, (RECT *)&update);
    }
}

/* numLines>0: scroll up numLines
   numLines<0: scroll down -numLines */
void termLinesScroll (const TerminalInterval *ti, int numLines)
{
    WindowInterval wi;

    intervalTerminalToWindow (ti, &wi);
    winLinesScroll (&wi, numLines);
}

/* numLines>0: scroll up numLines
   numLines<0: scroll down -numLines */
void winLinesScroll (const WindowInterval *pwi, int numLines)
{
    WindowInterval wi;	/* local copy */
    int nTotLines;	/* number of visible lines in 'ti' */
    int nScroll;	/* number of lines to scroll */
    int nClear;		/* number of lines to clear */
    int nStep;		/* number of 'steps' = abs(numLines) */

    if (pwi->bottom <= 0 ||
	pwi->top    >= term.winSize.cy) {
	/* No lines to scroll
	 */
	return;
    } 

    wi= *pwi;
    if (wi.top<0) wi.top= 0;
    if (wi.bottom>term.winSize.cy) wi.bottom= term.winSize.cy;
    nTotLines= wi.bottom - wi.top;

    nStep= abs (numLines);
    if (nStep>=nTotLines) {
	nScroll= 0;
	nClear= nTotLines;
    } else {
	nScroll= nTotLines-nStep;
	nClear= nStep;
    }

    if (nScroll) {
	/* Build scroll rect
	 */
	PixelRect client;
	PixelRect rect;
	int npix;

	GetClientRect (tsd.wnd, (RECT *)&client);
	rect.left  = 0;
	rect.right = client.right;
	if (numLines>0) {
	    rect.top    = termMargin.top + (wi.bottom-nScroll) * term.charSize.cy;
	    rect.bottom = termMargin.top + wi.bottom * term.charSize.cy;
	    npix= -nStep * term.charSize.cy;
	} else {
	    rect.top    = termMargin.top + wi.top * term.charSize.cy;
	    rect.bottom = termMargin.top + (wi.top+nScroll) * term.charSize.cy;
	    npix=  nStep * term.charSize.cy;
	}
	ScrollWindow (tsd.wnd, 0, npix, (RECT *)&rect, NULL);
    }

    if (nClear) {
	/* Invalidate the part of the window exposed
	 */
	WindowRect wrect;

	if (numLines>0) {
	    wrect.top=    wi.bottom - nClear;
	    wrect.bottom= wi.bottom;
	} else {
	    wrect.top=    wi.top;
	    wrect.bottom= wi.top + nClear;
	}
	wrect.left= 0;
	wrect.right= term.winSize.cx;
	winInvalidateRect (&wrect);
    }
}

/* Some characters were inserted into a line.
 */
void winCharsInsert (const TerminalPoint *tp)
{
    TerminalRange tr;

    tr.from= *tp;
    tr.to.y= tp->y;
    tr.to.x= term.winSize.cx;

    /* Hide the selection if it overlaps the inserted characters
     */
    termSelectCheckOverlap (&tr);

    /* Add the affected line, from the insert column to the end to the
     * update rect.
     */
    termModifyRange (&tr);
}

/* Some characters were deleted from a line
 */
void winCharsDelete (const TerminalPoint *tp)
{
    TerminalRange tr;

    tr.from= *tp;
    tr.to.y= tp->y;
    tr.to.x= term.winSize.cx;

    /* Hide the selection if it overlaps the inserted characters
     */
    termSelectCheckOverlap (&tr);

    /* Add the affected line, from the delete column to the end to the
		 * update rect.
     */
    termModifyRange (&tr);
}

/************************
 * SELECTION MANAGEMENT *
 ************************/

/* Invalidate the area of the window that contains the selection
 */

static void selectInvalidate (const SelectData *sd)
{
    BufferRange brange;
    BOOL fSelect;
    WindowRange wrange;
    WindowRect rect= EmptyRect;

    fSelect= selectGetRange (sd, &brange);
    if (! fSelect) return;

    TermDebugF ((0, "selectInvalidate %d:(%d,%d)-(%d,%d): brange=(%d,%d)-(%d,%d)\n"
	, sd->fHave
	, sd->range.from.y, sd->range.from.x, sd->range.to.y, sd->range.to.x
	, brange.from.y, brange.from.x, brange.to.y, brange.to.x));

    rangeBufferToWindow (&brange, &wrange);
    windowRangeToWindowRect (&wrange, &rect);

    TermDebugF ((0, "selectInvalidate (%d,%d)-(%d,%d) calls winInvalidateRect winrect=(t=%d,l=%d,b=%d,r=%d)\n"
	, sd->range.from.y, sd->range.from.x, sd->range.to.y, sd->range.to.x
	, rect.top, rect.left, rect.bottom, rect.right));
    winInvalidateRect (&rect);

    return;
}

static void termSelectCheckOverlap (const TerminalRange *trange)
{
    BufferRange br;

    rangeTerminalToBuffer (trange, &br);
    bufferSelectCheckOverlap (&br);
}

static void selectCheckOverlap (const WindowRange *wrange)
{
    BufferRange br;

    rangeWindowToBuffer (wrange, &br);
    bufferSelectCheckOverlap (&br);
}

static void bufferSelectCheckOverlap (const BufferRange *br)
{
    BufferRange selrange; /* from<=to */
    BOOL fHave;

/*
    TermDebugF ((1, "-> selectCheckOverlap (%d,%d)-(%d,%d)"
	" select=(%d,%d)-(%d,%d),fHave=%d\n"
	, wrange->from.y, wrange->from.x, wrange->to.y, wrange->to.x
	, term.select.range.from.y, term.select.range.from.x
	, term.select.range.to.y,   term.select.range.to.x
	, term.select.fHave));
*/

    fHave= selectGetRange (&term.select, &selrange);
/*
	TermDebugF ((0, "selectCheckOverlap: selectGetRange returned (%d,%d)-(%d,%d),fHave=%d\n"
	, selrange.from.y, selrange.from.x
	, selrange.to.y,   selrange.to.x
	, fHave));
*/
    if (!fHave) {
	/* No selection, or the selection is empty: do nothing */
	goto RETURN;
    }

    if (rangeDistinct ((Range *)&selrange, (Range *)br)) {
/*
	TermDebugF ((0, "selectCheckOverlap these are distinct: (%d,%d)-(%d,%d) and"
	" (%d,%d)-(%d,%d)\n"
	, selrange.from.y, selrange.from.x , selrange.to.y, selrange.to.x
	, br->from.y, br->from.x, br->to.y, br->to.x));
*/
	/* No overlap */
	goto RETURN;
    }

    /* There is an overlap between the area and the selection.  Cancel
     * the selection.
     */
    selectInvalidate(&term.select);
    term.select.fHave = FALSE;

/*
    if (prevSelect.fHave != term.select.fHave) {
	TermDebugF ((0, "selectCheckOverlap (%d,%d)-(%d,%d)"
	" select=(%d,%d)-(%d,%d),fHave=%d -- selection cancelled\n"
	, br.from.y, br.from.x, br.from.y, br.from.x
	, prevSelect.range.from.y, prevSelect.range.from.x
	, prevSelect.range.to.y,   prevSelect.range.to.x
	, prevSelect.fHave));
    }
*/

RETURN:;
/*
    TermDebugF ((-1, "<- selectCheckOverlap (%d,%d)-(%d,%d)"
	" select=(%d,%d)-(%d,%d),fHave=%d\n"
	, wrange->from.y, wrange->from.x, wrange->to.y, wrange->to.x
	, term.select.range.from.y, term.select.range.from.x
	, term.select.range.to.y,   term.select.range.to.x
	, term.select.fHave));
*/
}

void termCancelSelect (int mode)
{
    WindowRange wr;

    if (!term.select.fHave) return;

    if (mode==0) { /* cancel selection unconditionally */
	selectInvalidate(&term.select);
	term.select.fHave = FALSE;

    } else if (mode==1) { /* cancel selection only if overlaps with the visible part */
	wr.from.y= 0;
	wr.from.x= 0;
	wr.to.y= term.winSize.cy-1;
	wr.to.x= term.winSize.cx;
	selectCheckOverlap (&wr);
    }
}

/* Paste the current clipboard contents into the terminal
 */
void termSelectPaste()
{
    /* Get text from the clipboard
     */
    if (OpenClipboard(tsd.wnd)) {
	HANDLE clipHnd;		/* handle to text clipboard data */
	LPSTR text;		/* point to text on clipboard */
	DWORD size;		/* size of text on clipboard */

	if ((clipHnd = GetClipboardData (CF_UNICODETEXT))==NULL) {
	    /* No text on clipboard
	     */
	    CloseClipboard();
	    return;
	}

	if ((text = (LPSTR)GlobalLock(clipHnd))==NULL) {
	    /* Could not get access to the clipboard text
	     */
	    CloseClipboard();
	    return;
	}

	/* Find the size of the clipboard text and then send it to the
	 * remote end
	 */
	size = GlobalSize(clipHnd);
	if (size > 0) {
	    size_t wsize= size / sizeof (uint16_t);
	    if (wsize > 0 && ((uint16_t *)text)[wsize-1]==0) {
		--wsize;
		size= wsize * sizeof (uint16_t);
	    }
	}
	if (size != 0 && size < INT_MAX)
	{
	    ConstBuffData b;

	    b.ptr= text;
	    b.len= size;

	    emulFuncKeyPressB (EFK_LOCAL_SAME, &b, DTCHAR_UTF16);
#if 0
	    /* --- Paste problem identified by toomus vv
	     * break the text into smaller chunks
	     * (this helps if there's a bunch of text to transfer in
	     *   the clipboard)
	     */
	    /* --- Direct sending commented out by LZS on 2002.01.04.
	     * reasons: - local echo
	     *          - charset conversion
	     */
	    /* 2002.01.09 by Enrique Grandes
	     * translate '\r\n' into '\r' for emulKeyPress()
	     */
	    if (!socketConnected() || term.echoKeystrokes) {
		/* When in local echo mode, or not connected,
		   echo the characters
		 */
		emulAddText2 (text, size, DTCHAR_UTF16, TRUE);

	    } else if (socketConnected()) {
		size_t offset = 0;
		size_t chunk_length = 4096;

		while (offset < size) {
		    size_t bytes_remaining = size - offset;

		    if (chunk_length > bytes_remaining)
			chunk_length = bytes_remaining;

		    socketWrite(text+offset, chunk_length);
		    offset += chunk_length;
		}
	    }
#endif
	}
	/* Clean up
	 */
	GlobalUnlock(clipHnd);
	CloseClipboard();
    }
}

/* Copy the selected text into the specified buffer
 *
 * Args:
 * into - buffer (reallocated on demand)
 * cs - returns DTCHAR_xxx
 */
static void termSelectGetText (Buffer *into, int lgcmode, int *cs)
{
    BufferRange selrange;	/* start of the selection */
    int lineNum;		/* iterate over lines in selection */
    ConstBuffData bCrLf;
    ConstBuffData bZero;

#if defined(_WIN32)
    if ((lgcmode&LGC_MODE_CSMASK)==LGC_MODE_UTF16) {
	bCrLf.ptr= (const void *)L"\r\n";
	bCrLf.len= 4;
	bZero.ptr= (const void *)L"";
	bZero.len= 2;
    } else
#endif
    {
	bCrLf.ptr= "\r\n";
	bCrLf.len= 2;
	bZero.ptr= (const void *)L"";
	bZero.len= 1;
    }

    *cs = DTCHAR_ASCII;             /* not defined yet */
    /* Get start and end of the selection
     */
    selectGetRange (&term.select, &selrange);

    /* Iterate over all lines in the selection
     */
    for (lineNum = selrange.from.y; lineNum <= selrange.to.y; ++lineNum) {
	Line* line = linesGetLine (lineNum);

	if (line == NULL)
	    /* Just in case
	     */
	    break;

	if (selrange.from.x >= line->len) {
	    /* Starting at or after end of line - copy \r\n
	     */
	    if (!line->wrapped) {
		BuffAppend (into, &bCrLf);
	    }
	} else {
	    /* Work out how much text to get from line
	     */
	    if (lineNum < selrange.to.y || selrange.to.x > line->len) {
		/* Copying past end of line - include \r\n
		 */
		linesGetChars (lgcmode | LGC_ALLOC, line,
			       selrange.from.x, line->len - selrange.from.x,
			       into, cs);
		if (!line->wrapped) {
		    BuffAppend (into, &bCrLf);
		}

	    } else {
		/* Only copying part of a line
		 */
		linesGetChars (lgcmode | LGC_ALLOC, line,
			       selrange.from.x, selrange.to.x - selrange.from.x,
			       into, cs);
	    }
	}

	/* When moving to next line, reset column
	 */
	selrange.from.x = 0;
    }

    if (into->len) {			/* let's assume that into->len==0 means 'no selection' */
	BuffAppend (into, &bZero);	/* append terminating zero -- it counts into the length */
    }
}

/* Copy the selection onto the clipboard
 */
void termSelectCopy (void)
{
    DynBuffer bSelText = EmptyBuffer;
    HGLOBAL textHnd = NULL;	/* allocate memory to hold selection */
    void *text;			/* point to allocated memory */
    int cs;
    int fCopied= 0;
#if defined (_WIN32)
    int lgcmode= LGC_MODE_UTF16;
    int cf= CF_UNICODETEXT;
#else
    int lgcmode= LGC_MODE_NARROW;
    int cf= 0;
#endif

    /* Find the size of the selection, then allocate enough memory to
     * hold it
     */
    termSelectGetText (&bSelText, lgcmode, &cs);
    if (bSelText.len<=1) { /* length includes the terminator! */
	goto RETURN;
    }

    if ((textHnd = GlobalAlloc (GMEM_MOVEABLE, bSelText.len))==NULL)
	goto RETURN;

    if ((text = GlobalLock(textHnd))==NULL)
	goto RETURN;

    /* Copy the selection into the allocated memory
     */
    memcpy (text, bSelText.ptr, bSelText.len);
    GlobalUnlock(textHnd);
    BuffCutback (&bSelText, 0);

    /* Clear the current contents of the clipboard, and set the text
     * handle to the new text.
     */
    if (OpenClipboard(tsd.wnd)) {
	HANDLE hret;

	if (!cf) {	/* it might be preset */
	    if (cs==DTCHAR_OEM) cf = CF_OEMTEXT;
	    else                cf = CF_TEXT;
	}

	EmptyClipboard ();
	hret= SetClipboardData (cf, textHnd);
	CloseClipboard ();

	fCopied= (hret!=NULL);
    }

RETURN:
    if (!fCopied) {
	if (textHnd) GlobalFree (textHnd);
    }
    if (bSelText.maxlen) BuffCutback (&bSelText, 0);
}

/* Return whether or not there is a selection defined
 */
static BOOL termSelectCheck (void)
{
    BufferRange unused;
    BOOL retval;

    retval= selectGetRange (&term.select, &unused);
    return retval;
}

/* Start a timer to automatically extend the selection when the mouse
 * is above or below the terminal window
 */
static void termSelectStartTimer(void)
{
    if (termTimerId == 0)
	termTimerId = SetTimer(tsd.wnd, ID_TERM_TIMER, 50, NULL);
}

/* Stop the timer that is used to extend the selection
 */
static void termSelectStopTimer(void)
{
    if (termTimerId == 0)
	return;
    KillTimer(tsd.wnd, ID_TERM_TIMER);
		termTimerId = 0;
}

/* Return whether or not the user is currently performing a selection
 */
BOOL termSelectInProgress(void)
{
    return inCapture (NULL);
}

/* Return whether or not there currently is a selection
 */
BOOL termHaveSelection(void)
{
    return term.select.fHave;
}

/***********************
 * TERMINAL MANAGEMENT *
 ***********************/

/* Return handle of the terminal window
 */
HWND termGetWnd(void)
{
    return tsd.wnd;
}

/* Build a caret.  This should really be quite easy (according to the
 * Microsoft documentation), but for some reason Windows 3.1 gives the
 * caret a random color when we use our own palette.
 *
 * After days of pulling what little hair I have left out of my head,
 * I arrived at this wonderfully elegant, and intuitively obvious
 * code.  Read it and weep.
 *
 * I would comment the code, but I do not have the stomach for it.
 */
static void termMakeCaretBitmap(void)
{
    HDC dc;
    HDC memDc;
    HBRUSH brush;
    HBRUSH oldBrush;
    HBITMAP oldBitmap;
    HPALETTE oldPalette= NULL;

    if (term.caretBitmap != NULL)
	DeleteObject(term.caretBitmap);

    switch (cursorStyle)
    {
    case cursorBlock:
	SetRect((RECT *)&term.cursorRect, 0, 0, term.charSize.cx, term.charSize.cy);
	break;
    case cursorVertLine:
	SetRect((RECT *)&term.cursorRect, 0, 0, 2, term.charSize.cy);
	break;
    case cursorUnderLine:
	SetRect((RECT *)&term.cursorRect, 0, term.charSize.cy-2, term.charSize.cx, 2);
	break;
    }
    dc = GetDC(tsd.wnd);
    term.caretBitmap
	= CreateCompatibleBitmap(dc, term.cursorRect.right, term.cursorRect.bottom);

    memDc = CreateCompatibleDC(dc);
    oldBitmap = (HBITMAP)SelectObject(memDc, term.caretBitmap);
    if (termHasPalette) {
	oldPalette = SelectPalette(memDc, termPalette, 0);
	RealizePalette(memDc);
	brush = CreateSolidBrush(PALETTEINDEX(INTENSE+White));
    } else {
	brush = CreateSolidBrush(colors[INTENSE+White]);
    }
    oldBrush = (HBRUSH)SelectObject(memDc, brush);

    PatBlt(memDc, 0, 0, term.cursorRect.right, term.cursorRect.bottom, PATCOPY);

    SelectObject(memDc, oldBrush);
    DeleteObject(brush);
    if (termHasPalette)
	SelectObject(memDc, oldPalette);
    SelectObject(memDc, oldBitmap);
    DeleteDC(memDc);

    ReleaseDC(tsd.wnd, dc);
}

/* Set the font used to draw the terminal.  Propagate the window
 * resize up to the application window.
 */
static void termSetFontPart1 (void)
{
    TEXTMETRIC tm;		/* get metrics of new terminal font */

    TermDebugF ((1, "-> termSetFontPart1 start\n"
	, term.winSize.cy, term.winSize.cx));

    /* Create the font that is used in the terminal window
     */
    fontDestroy (term.fonts);

    fontCreate (tsd.wnd, &tm, term.fonts);
    term.fontCharSet = tm.tmCharSet;
    term.charSize.cy = tm.tmHeight;
    term.charSize.cx = tm.tmAveCharWidth;

    /* Build a new caret with the new character size
     */
    termMakeCaretBitmap();

    TermDebugF ((-1, "<- termSetFontPart1 exit (charSize=%d-%d)\n",
	term.charSize.cy, term.charSize.cx));
}

static void termSetFontPart2 (void)
{
    SIZE size;

    TermDebugF ((1, "-> termSetFontPart2 start winSize=%d-%d\n"
	, term.winSize.cy, term.winSize.cx));

    /* Tell our parent window how big we want to be
     */

    size.cx = termMargin.left + term.winSize.cx * term.charSize.cx + vscrollWidth;
    size.cy = termMargin.top  + term.winSize.cy * term.charSize.cy;

    TermDebugF ((0, "termSetFont before telnetTerminalSize (%d,%d) winSize=%d-%d charSize=%d-%d\n"
	, size.cy, size.cx
	, term.winSize.cy, term.winSize.cx, term.charSize.cy, term.charSize.cx));

    telnetTerminalSize(size.cx, size.cy);
    TermDebugF ((0, "termSetFont after telnetTerminalSize (%d,%d)\n"
	, size.cy, size.cx));

    /* Now make ourselves the requested size.  It seems that if we do
     * not say fRepaint = TRUE the scrollbar is not redrawn???!?!
     *
     * I LOVE WINDOWS
     */
    TermDebugF ((0, "termSetFont before MoveWindow (%d,%d)\n"
	, size.cy, size.cx
	));
    MoveWindow(tsd.wnd, 0, 0, size.cx, size.cy, TRUE);

    /* Make sure the entire client area is redrawn with the new font
     */
    InvalidateRect(tsd.wnd, NULL, FALSE);

    TermDebugF ((-1, "<- termSetFontPart2 exit (size=%d-%d)\n",
	size.cy, size.cx));
}

void termSetFont(void)
{
    termSetFontPart1 ();
    termSetFontPart2 ();
}

static BOOL extractGeometry(const char *str, CharSize *size, PixelSize *pos)
{
    int num;
    char xrel, yrel;
    long x1, y1, x2, y2; /* to avoid Win16/Win32 incompatibility in SIZE-members */

    num = sscanf(str, "%ldx%ld%c%ld%c%ld",
		 &x1, &y1, &xrel, &x2, &yrel, &y2);
    size->cx= (int)x1;
    size->cy= (int)y1;
    pos->cx= (int)x2;
    pos->cy= (int)y2;

    if (num == 6
	&& (xrel == '-' || xrel == '+')
	&& (yrel == '-' || yrel == '+')) {
	if (xrel == '-')
	    pos->cx = -pos->cx;
	if (yrel == '-')
			pos->cy = -pos->cy;
	return TRUE;
    }
    if (num < 2) {
	size->cx = 80;
	size->cy = 25;
    }
    pos->cx = 0;
    pos->cy = 0;
    return FALSE;
}

// Khader
/* Checks if windows size is according to defined limits
 */
BOOL termCheckSizeLimits(CharSize *size)
{
    BOOL fChanged= FALSE;
    /* Limit the terminal width to TERM_MIN_X .. TERM_MAX_X
     */
    if (size->cx < TERM_MIN_X) { /* Khader - replaced consts with defines */
	size->cx = TERM_MIN_X;
	fChanged= TRUE;
    }
    if (size->cx > TERM_MAX_X) {
	size->cx = TERM_MAX_X;
	fChanged= TRUE;
    }

    /* Limit the terminal height to TERM_MIN_Y .. TERM_MAX_Y
     */
    if (size->cy < TERM_MIN_Y) {
	size->cy = TERM_MIN_Y;
	fChanged= TRUE;
    }
    if (size->cy > TERM_MAX_Y) {
	size->cy = TERM_MAX_Y;
	fChanged= TRUE;
    }

    return fChanged;
}

/* Set the geometry of the terminal window (in characters)
 *
 * Args:
 * str - new window geometry in form WxH or WxH+X+Y
 */
void termSetGeometry (char* str, OptSource source)
{
    static OptSource actsource= OptFromDefault;

    if (source>=actsource) {
    /* Extract the size and position from str
     */
	term.useInitPos = extractGeometry(str, &term.winSize, &term.winInitPos);
	termCheckSizeLimits(&term.winSize);	// Khader

	actsource= source;
    }
}

/* We are about to receive window focus - realise our palette and
 * force a repaint of the window if necessary
 *
 * Return the number of changes
 */
static int termPaletteChanged(void)
{
    HDC dc;			/* terminal window device context */
    HPALETTE oldPalette;	/* remember original palette in dc */
    UINT numChanges;		/* number of changes in mapping system
				 * palette since our palette last
				 * realised */

    if (!termHasPalette)
	/* We are not using a palette, nothing to do
	 */
	return 0;

    /* Realise our logical palette in the system palette and determine
     * if there are any changes in the mapping
     */
		dc = GetDC(tsd.wnd);
    oldPalette = SelectPalette(dc, termPalette, FALSE);
    numChanges = RealizePalette(dc);
    SelectPalette(dc, oldPalette, FALSE);
    ReleaseDC(tsd.wnd, dc);

    /* There were changes in the palette mapping - repaint the window
     */
    if (numChanges > 0)
        InvalidateRect(tsd.wnd, NULL, FALSE);

    /* Return the number of changes in the mapping
     */
    return (int)numChanges;
}

/* Build a palette for our terminal window
 */
static void termBuildPalette(void)
{
    NPLOGPALETTE palette;
    unsigned idx;
    HANDLE hnd = LocalAlloc(LMEM_FIXED, sizeof(LOGPALETTE)
			    + PaletteSize * sizeof(PALETTEENTRY));

    if (!hnd)
	return;

    palette = (NPLOGPALETTE)LocalLock(hnd);
    palette->palVersion = 0x300;
    palette->palNumEntries = PaletteSize;
    for (idx = 0; idx < PaletteSize; idx++) {
	COLORREF color = colors[idx];
	palette->palPalEntry[idx].peRed = GetRValue(color);
	palette->palPalEntry[idx].peGreen = GetGValue(color);
	palette->palPalEntry[idx].peBlue = GetBValue(color);
	palette->palPalEntry[idx].peFlags = 0;
    }
    termPalette = CreatePalette((LPLOGPALETTE)palette);
    LocalUnlock((HLOCAL)palette);
    LocalFree(hnd);
}

/* Set the attribute to be used for blank characters
 */
void termSetBlankAttr(const AttrDefaultData *attr, OptSource source)
{
    static OptSource actsource= OptFromDefault;

    if (source>=actsource) {
	term.defattr = *attr;
	actsource= source;
    }
}

#if 0
/* Get the attribute to be used for blank characters
 */
Attrib termGetBlankAttr(void)
{
    return term.blankAttr.attr;
}
#endif

/* Set the application window title
 */
void termSetTitle (const char* title, int dtcc)
{
    if (title) term.title.len= strnlen (title, sizeof term.title.title - 1);
    else       term.title.len= 0;

    if (term.title.len) memcpy (term.title.title, title, term.title.len);
    term.title.title[term.title.len]= '\0';

    term.title.dtcc= dtcc;

    telnetSetTitle();
}

/* Return the application window title
 */
char* termGetTitle (unsigned *len, int *dtcc)
{
    if (len)  *len=  term.title.len;
    if (dtcc) *dtcc= term.title.dtcc;
    return term.title.title;
}

/* Return whether or not there is a application window title set
 */
BOOL termHasTitle()
{
    return term.title.len != 0;
}

/* Scroll the window a number of lines.  Positive values scroll lines
 * up.
 *
 * Args:
 * numLines - the number of lines to scroll
 */
static void termDoScroll(int numLines)
{
    if (!numLines)
	/* Nothing to do
	 */
	return;

    if (abs(numLines) >= term.winSize.cy)
	/* Scrolling replaces the entire window - repaint it all
	 */
	InvalidateRect(tsd.wnd, NULL, TRUE);
    else
	/* Scrolling retains some of the window
	 */
	winScrollWindow(numLines);

    /* Update the top visible line and the history scrollbar
     */
		term.topVisibleLine += numLines;
    SetScrollPos(tsd.wnd, SB_VERT, term.topVisibleLine, TRUE);
}

/* Try scrolling the terminal a number of lines.  Positive values
 * scroll lines up.
 *
 * Args:
 * numLines - the number of lines to scroll
 */
static void termTryScroll(int numLines)
{
    if (numLines > 0) {
	/* Scrolling lines up
	 */
	if (term.topVisibleLine < winTerminalTopLine()) {
	    /* We can scroll lines up.  Make sure we do not scroll
	     * more lines than exist.
	     */
	    if (term.topVisibleLine + numLines < winTerminalTopLine())
		termDoScroll(numLines);
	    else
		termDoScroll(winTerminalTopLine() - term.topVisibleLine);
	}
    } else {
	/* Scrolling lines down
	 */
	if (term.topVisibleLine > 0) {
	    /* We can scroll lines down.  Make sure wr do not scroll
	     * more lines than exist.
	     */
	    if (term.topVisibleLine + numLines >= 0)
		termDoScroll(numLines);
	    else
		termDoScroll(-term.topVisibleLine);
	}
    }
}

#ifdef WIN32
/* termWheelAction -- handle WM_MOUSEWHEEL messages
   also processes messages WM_CREATE and WM_SETTOMGCHANGE
 */

void termWheelAction (UINT msg, WPARAM wparam, LPARAM lparam)
{
    static UINT wheel_scroll_lines= 1; /* warning: it can be negative *sigh* */
    static UINT wheel_scroll_chars= 1; /* warning: it can be negative *sigh* */

    switch (msg) {
    case WM_CREATE:
    case WM_SETTINGCHANGE:
	SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &wheel_scroll_lines, 0);
	SystemParametersInfo (SPI_GETWHEELSCROLLCHARS, 0, &wheel_scroll_chars, 0);
	break;

    case WM_MOUSEWHEEL: {
	short zDelta;
	WORD flags;
	unsigned action;
	int amount;

	zDelta= GET_WHEEL_DELTA_WPARAM (wparam);
	flags = LOWORD (wparam);

	if (socketConnected() && term.mouseRep.mouseReporting) { /* shift undoes mouse reporting */
	    if (!(flags&MK_SHIFT)) {
		PixelRect r;
		TermPosition mousepos;
		int xpos;
		int ypos;

		GetWindowRect (tsd.wnd, (RECT *)&r);
		xpos= GET_X_LPARAM(lparam) - r.left;
		ypos= GET_Y_LPARAM(lparam) - r.top;
		termFillPos (xpos, ypos, &mousepos);
		termSendMouseEvent (zDelta>0? 3: 4, TSME_PRESS, flags, &mousepos);
		return;
	    }
	/*  flags &= ~MK_SHIFT; gives warning*/
	}

	if (zDelta>0) {
	    if ((int)wheel_scroll_lines>0) {
		action= SB_LINEUP;
		amount= (zDelta + WHEEL_DELTA/2)/WHEEL_DELTA * wheel_scroll_lines;
	    } else {
		action= SB_PAGEUP;
		amount= (zDelta + WHEEL_DELTA/2)/WHEEL_DELTA;
	    }
	} else {
	    if ((int)wheel_scroll_lines>0) {
		action= SB_LINEDOWN;
		amount=  (-zDelta + WHEEL_DELTA/2)/WHEEL_DELTA * wheel_scroll_lines;
	    } else {
		action= SB_PAGEDOWN;
		amount= (-zDelta + WHEEL_DELTA/2)/WHEEL_DELTA;
	    }
	}

	termVerticalScroll (action, amount);
	}
	break;

    case WM_MOUSEHWHEEL: { /* note: it is MOUSE_H_WHEEL with a 'H' in it */
	short zDelta;
	WORD flags;

	zDelta= GET_WHEEL_DELTA_WPARAM (wparam);
	flags = LOWORD (wparam);

	if (socketConnected() && term.mouseRep.mouseReporting) { /* shift undoes mouse reporting */
	    if (!(flags&MK_SHIFT)) {
		PixelRect r;
		TermPosition mousepos;
		int xpos;
		int ypos;

		GetWindowRect (tsd.wnd, (RECT *)&r);
		xpos= GET_X_LPARAM(lparam) - r.left;
		ypos= GET_Y_LPARAM(lparam) - r.top;
		termFillPos (xpos, ypos, &mousepos);
		termSendMouseEvent (zDelta>0? 5: 6, TSME_PRESS, flags, &mousepos);
		return;
	    }
	/*  flags &= ~MK_SHIFT; gives warning*/
	}

	/* there is no local action for horizontal scroll, yet */
	}
	break;
    }
}
#endif

static void termLeftButtonDown   (UINT flags, int nClick, const TermPosition *mousepos);
static void termLeftButtonUp     (UINT flags, const TermPosition *mousepos);

static void termLeftButton     (int nAction, int nClick,
	UINT flags, const TermPosition *mousepos);
static void termPasteButton    (int nButton, int nAction, UINT flags, const TermPosition *mousepos);
static void termNonPasteButton (int nButton, int nAction, int nClick,
	UINT flags, const TermPosition *mousepos);
/* depending on 'mousePasteButton'
   either 'Paste'=Right and 'NonPaste'=Middle Button or vice versa
 */

static void termMouseMove (UINT flags, const TermPosition *mousepos);

/* termMouseAction -- handle WM_***mouse*** messages
   also processes messages WM_CREATE and WM_SETTOMGCHANGE
 */

LRESULT termMouseAction (UINT message, WPARAM wparam, LPARAM lparam, const MSG *msg)
{
    static int dblclick_xmax, dblclick_ymax;
    static DWORD dblmaxtime;
    TermPosition mousepos;
    int nAction, nButton;
    UINT flags;
    int capture_btn;
    char mouse_handling;	/* 'L'/'R' = local/remote */

static struct {
	BOOL fFilled;			/* if FALSE, the rest is ignored */
	DWORD time;			/* time of the last click [ms] */
	PixelPoint point;		/* position of the last click */
	UINT flags;			/* flags at the last click */
	int  nButton;
	int  nClick;			/* number of clicks */
    } clkcnt;				/* double/tripleclick data */

    if (message != msg->message &&
	message != WM_CREATE) {		/* when WM_CREATE, struct 'msg' is not filled */
	fprintf (stderr, "*** term.c:%d we have to redesign this part\n", __LINE__);
	TermDebugF ((0, "termMouseAction: *** message=%d != msg->message=%d"
		, (int)message, (int)msg->message));
    }

    switch (message) {
    case WM_CREATE:
    case WM_SETTINGCHANGE: {
	dblclick_xmax= GetSystemMetrics (SM_CXDOUBLECLK) /2;
	dblclick_ymax= GetSystemMetrics (SM_CYDOUBLECLK) /2;
	dblmaxtime= GetDoubleClickTime ();
	break; }

    case WM_LBUTTONDOWN:   case WM_MBUTTONDOWN:   case WM_RBUTTONDOWN:
    case WM_LBUTTONUP:     case WM_MBUTTONUP:     case WM_RBUTTONUP:
    case WM_LBUTTONDBLCLK: case WM_MBUTTONDBLCLK: case WM_RBUTTONDBLCLK:

	if (message==WM_LBUTTONDOWN || message==WM_MBUTTONDOWN || message==WM_RBUTTONDOWN) {
	    nAction= TSME_PRESS;

	} else if (message==WM_LBUTTONUP || message==WM_MBUTTONUP || message==WM_RBUTTONUP) {
	    nAction= TSME_RELEASE;

	} else {
	    nAction= TSME_DBLCLICK;
	}

	if (message==WM_LBUTTONDOWN || message==WM_LBUTTONUP || message==WM_LBUTTONDBLCLK) {
	    nButton= 0; /* left */

	} else if (message==WM_MBUTTONDOWN || message==WM_MBUTTONUP || message==WM_MBUTTONDBLCLK) {
	    nButton= 1; /* middle */

	} else {
	    nButton= 2;	/* right */
	}

	termFillPos (GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), &mousepos);
	flags= wparam;

	if (nAction != TSME_RELEASE) {
	    if (clkcnt.fFilled) {
		if (clkcnt.nButton != nButton ||
		    clkcnt.flags   != flags ||
		    msg->time - clkcnt.time > dblmaxtime ||
		    msg->pt.x < clkcnt.point.x - dblclick_xmax || msg->pt.x > clkcnt.point.x + dblclick_xmax ||
		    msg->pt.y < clkcnt.point.y - dblclick_ymax || msg->pt.y > clkcnt.point.y + dblclick_ymax) {
		    clkcnt.fFilled= FALSE;
		} else {
		    clkcnt.time= msg->time;	/* time of the last click */
		    clkcnt.point.x= msg->pt.x;	/* position of the last click */
		    clkcnt.point.y= msg->pt.y;	/* position of the last click */
		    clkcnt.nClick += 1; 	/* was: (nAction==TSME_DBLCLICK)? 2: 1; */
		}
	    }
	    if (!clkcnt.fFilled) {
		clkcnt.fFilled= TRUE;
		clkcnt.time= msg->time;		/* time of the last click */
		clkcnt.point.x= msg->pt.x;	/* position of the last click */
		clkcnt.point.y= msg->pt.y;	/* position of the last click */
		clkcnt.flags= flags;
		clkcnt.nButton= nButton;
		clkcnt.nClick= nAction==TSME_DBLCLICK? 2: 1;
	    }
	}

/* perhaps we should report (or ignore) this mouse-event? */

/* Releasing capture has priority over sending */
	if (inCapture (&capture_btn) && capture_btn==nButton) {
	    mouse_handling= 'L';

	} else if (socketConnected() && term.mouseRep.mouseReporting) {
	    if (flags&MK_SHIFT) {	/* shift prevents mouse-reporting */
		flags &= ~MK_SHIFT;
		mouse_handling= 'L';

	    } else {
		mouse_handling= 'R';
	    }

	} else {
	    mouse_handling= 'L';
	}

	if (mouse_handling=='R') {
	    termSendMouseEvent (nButton, nAction, flags, &mousepos);
	    return 0;
	}

/* process it locally */
	if (nButton==0) 
	    termLeftButton (nAction, clkcnt.nClick, flags, &mousepos);

	else if (nButton==mousePasteButton) 
	    termPasteButton (nButton, nAction, flags, &mousepos);

	else
	    termNonPasteButton (nButton, nAction, clkcnt.nClick, flags, &mousepos);
	return 0;

    case WM_MOUSEMOVE:
	termFillPos (GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam), &mousepos);
	termMouseMove (wparam, &mousepos);
	return 0;

    }
    return 0;
}

/* User moved the history scrollbar.  Scroll the terminal window.
 *
 * Args:
 * code - the type of scroll performed
 * pos -  the scrollbar position
 */
static void termUseScrollbar(UINT code, UINT pos)
{
    if (term.linebuff.used <= term.winSize.cy)
	/* Cannot scroll if we have used less lines than the height of
	 * the window.
	 */
	return;

    switch (code) {
    case SB_BOTTOM:
    case SB_TOP:
	termVerticalScroll (code, 0);
	break;

    case SB_LINEDOWN:
    case SB_LINEUP:
    case SB_PAGEDOWN:
    case SB_PAGEUP:
	termVerticalScroll (code, 1);
	break;

    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
	termVerticalScroll (code, pos);
	break;

    default:
	break;
    }
}

/*  Scroll the terminal window vertically
 *
 * Args:
 * code  - SB_****: the type of scroll performed
 * param - depends on 'code': amount of lines / amount of pages / position
 */
void termVerticalScroll (UINT code, UINT param)
{
    if (term.linebuff.used <= term.winSize.cy)
	/* Cannot scroll if we have used less lines than the height of
	 * the window.
	 */
	return;

    switch (code) {
    case SB_BOTTOM:
	termDoScroll(winTerminalTopLine() - term.topVisibleLine);
	break;
    case SB_ENDSCROLL:
	break;
    case SB_LINEDOWN:
	termTryScroll((int)param);
	break;
    case SB_LINEUP:
	termTryScroll(-(int)param);
	break;
    case SB_PAGEDOWN:
	termTryScroll(term.winSize.cy * (int)param);
	break;
    case SB_PAGEUP:
	termTryScroll(-term.winSize.cy * (int)param);
	break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: {
	int pos= (int)param;
	if (pos > winTerminalTopLine())
	    pos = winTerminalTopLine();
	termDoScroll(pos - term.topVisibleLine);
	break; }
    case SB_TOP:
	termDoScroll(-term.topVisibleLine);
	break;
    }

    /* Redraw the caret at the correct position
     */
    winCaretPos(&term.cursor.pos);
}

/* Scroll the terminal to the bottom of the history
 */
void termScrollToBottom()
{
    if (term.linebuff.used > term.winSize.cy)
	termDoScroll(winTerminalTopLine() - term.topVisibleLine);
}

/* Toggle whether or not terminal output forces a scroll to bottom
 */
void termToggleBottomOnOutput()
{
    bottomOnOutput = !bottomOnOutput;
}

/* Return whether or not terminal output forces a scroll to bottom
 */
BOOL termBottomOnOutput()
{
		return bottomOnOutput;
}

/* Toggle the state of auto-copy on selection
 */
void termToggleAutoCopy()
{
		autoCopy = !autoCopy;
}

/* Return the state of auto-copy on selection
 */
BOOL termAutoCopy()
{
		return autoCopy;
}

/* Return the state of xterm-style attached printer */
BOOL termAttachedPrinter(void)
{
    return term.attachedPrinter;
}

/* Toggle the state of xterm-style attached printer */
void termToggleAttachedPrinter(void)
{
    term.attachedPrinter = !term.attachedPrinter;
}

/* Return the state of printscreen */
BOOL termPrintScreenEnabled(void)
{
    return term.enablePrintScreen;
}

/* Toggle the state of printscreen */
void termTogglePrintScreen(void)
{
    term.enablePrintScreen = !term.enablePrintScreen;
}

/* Return the current cursor style */
CursorStyle termCursorStyle(void)
{
    return cursorStyle;
}

/* Set the cursor style */
void termSetCursorStyle(CursorStyle style)
{
    /* hide and destroy the old cursor */
    termHideCaret();
    termKillFocus();
    /* create the new caret bitmap */
    cursorStyle = style;
    termMakeCaretBitmap();
    /* restore the cursor */
    termSetFocus();
}

/*  Get button number (0-2) from flags, or -1
 *  if the user has pressed more than one buttons, we return the 'first' one
 */
static int termGetButton (int flags)
{
    int nButton= -1;

    if      (flags&MK_LBUTTON) nButton= 0;
    else if (flags&MK_MBUTTON) nButton= 1;
    else if (flags&MK_RBUTTON) nButton= 2;

    return nButton;
}

static void termSendMouseEvent (int nButton, int nAction, int flags,
	const TermPosition *mousepos)
{
    char evstr[256], *pev= evstr;
    ConstBuffData bev;
    unsigned encoded_button, button_modifier, modified_button;
    TerminalPoint termPoint;

    if (nAction==TSME_DBLCLICK) nAction= TSME_PRESS;

    if (!term.mouseRep.mouseReporting ||
	(term.mouseRep.mouseReporting==TERM_MOUSE_REP_X10 && (nAction!=TSME_PRESS || nButton>=3)) ||
	(nAction==TSME_RELEASE && nButton>=3) ||
	(term.mouseRep.mouseReporting<TERM_MOUSE_REP_X11_BTN_EVENT && nAction==TSME_MOVE) ||
	(nButton<0 && nAction!=TSME_MOVE)) {
	goto RETURN;
    }

    pointWindowToTerminal (&mousepos->win.p, &termPoint);
    pointClipTerminalPoint (&termPoint, &termPoint, 2);

    if (nAction==TSME_MOVE &&
	termPoint.x==term.mouseRep.ptLastReported.x &&
	termPoint.y==term.mouseRep.ptLastReported.y) {
	goto RETURN;
    }
    
    encoded_button = 0x20;
    switch (nAction) {
    case TSME_PRESS:
	if (nButton<=2) encoded_button += nButton;
	else		encoded_button += 0x40 + (nButton-3);
	break;

    case TSME_RELEASE:
	if (nButton<=2) encoded_button += 3; /* 3: generic 'buttonup' code */
	else		return;      /* but only for buttons 0-2, not for 3-7 (the wheels) */
	break;

    case TSME_MOVE:
	encoded_button += 0x20;		/* drag/move */
	if      (nButton<0)  encoded_button += 3;	/* no button is/was pressed */
	else if (nButton<=2) encoded_button += nButton; /* button 0-1-2 is/was pressed */
	else		     encoded_button += 0x40 + (nButton-3); /* button 3-6 is/was pressed */
	break;
    }

    button_modifier= 0;
    if (term.mouseRep.mouseReporting>=TERM_MOUSE_REP_X11) {
	if (flags&MK_SHIFT)   button_modifier += 0x04;
/*	if (flags&MK_ALT)     button_modifier += 0x08; won't work, instead: */
	if (GetKeyState (VK_MENU) &0x80) button_modifier += 0x08;
	if (flags&MK_CONTROL) button_modifier += 0x10;
    }
    modified_button = encoded_button + button_modifier;

    if (term.mouseRep.mouseCoord==TERM_MOUSE_COORD_STD) {
	unsigned xshift= termPoint.x + 1 + 32;
	unsigned yshift= termPoint.y + 1 + 32;

	if (xshift>=255) xshift= 255;
	if (yshift>=255) yshift= 255;

	evstr[0] = '\x1b';
	evstr[1] = '[';
	evstr[2] = 'M';
	evstr[3] = (char)modified_button;
	evstr[4] = (char)xshift;
	evstr[5] = (char)yshift;
	evstr[6] = (char)0;

	bev.ptr= evstr;
	bev.len= 6;
	emulFuncKeyPressB (EFK_LOCAL_SKIP, &bev, DTCHAR_RAW_BINARY);

    } else if (term.mouseRep.mouseCoord==TERM_MOUSE_COORD_UTF8) {
	unsigned xshift= termPoint.x + 1 + 32;
	unsigned yshift= termPoint.y + 1 + 32;

	if (xshift>2047) xshift= 2047;
	if (yshift>2047) yshift= 2047;

	*pev++ = '\x1b';
	*pev++ = '[';
	*pev++ = 'M';
	if (modified_button < 128) {	/* It should be less than 128! */
	    *pev++ = (char)modified_button;
	} else {
	    *pev++ = 0xc0 | (modified_button>>6);
	    *pev++ = 0x80 | (modified_button&0x3f);
	}
	if (xshift < 128) {
	    *pev++ = (char)xshift;
	} else {
	    *pev++ = 0xc0 | (xshift>>6);
	    *pev++ = 0x80 | (xshift&0x3f);
	}
	if (yshift < 128) {
	    *pev++ = (char)yshift;
	} else {
	    *pev++ = 0xc0 | (yshift>>6);
	    *pev++ = 0x80 | (yshift&0x3f);
	}
	bev.ptr= evstr;
	bev.len= pev - evstr;
	emulFuncKeyPressB (EFK_LOCAL_SKIP, &bev, DTCHAR_RAW_BINARY);

    } else if (term.mouseRep.mouseCoord==TERM_MOUSE_COORD_SGR) {
	char endchar;

	if (nAction==TSME_RELEASE && 0<=nButton && nButton<=2) endchar= 'm';
	else endchar= 'M';

	pev += sprintf (pev, "\x1b[<%d;%d;%d%c"
	    , nButton<0 ? 0 :
	      nButton<3 ? (int)(nButton+button_modifier):
	      (int)(0x40 + (nButton-3) + button_modifier)
	    , (int)(termPoint.x+1)
	    , (int)(termPoint.y+1)
	    , endchar);

	bev.ptr= evstr;
	bev.len= pev - evstr;
	emulFuncKeyPressB (EFK_LOCAL_SKIP, &bev, DTCHAR_ASCII);

    } else if (term.mouseRep.mouseCoord==TERM_MOUSE_COORD_URXVT) {
	char endchar= 'M';

	pev += sprintf (pev, "\x1b[%d;%d;%d%c"
	    , modified_button
	    , (int)(termPoint.x+1)
	    , (int)(termPoint.y+1)
	    , endchar);

	bev.ptr= evstr;
	bev.len= pev - evstr;
	emulFuncKeyPressB (EFK_LOCAL_SKIP, &bev, DTCHAR_ASCII);
    }

    term.mouseRep.fMoveReported= (nAction==TSME_MOVE);
    term.mouseRep.ptLastReported = termPoint;

RETURN:
    return;
}

/* Return the state of mouse event reporting
 */
BOOL termMouseReporting()
{
    return term.mouseRep.mouseReporting;
}

static void termLeftButton (int nAction, int nClick,
	UINT flags, const TermPosition *mousepos)
{
    switch (nAction) {
    case TSME_PRESS:
    case TSME_DBLCLICK:
	termLeftButtonDown (flags, nClick, mousepos);
	break;

    case TSME_RELEASE:
	termLeftButtonUp (flags, mousepos);
	break;
    }
}

/* User clicked the left mouse button (one or more times)
 */
static void termLeftButtonDown (UINT flags, int nClick, const TermPosition *mousepos)
{
    int act;

    TermDebugF ((1, "-> termLeftButtonDown(nClick=%d) flags=%0x pixpos=(%d,%d)"
	" %s\n"
	, nClick
	, (int)flags
	, mousepos->pixel.p.y, mousepos->pixel.p.x
	, SelectData_toString (&term.select)
	));

	/* Capture the mouse until the button is released
	 */
	SetCapture(tsd.wnd);
	if (GetCapture() != tsd.wnd) {
	    tsd.capture.flag= 0;
	    TermDebugF ((-1, "-> termLeftButtonDown: SetCapture failed\n"));
	    goto RETURN;
	}
	tsd.capture.flag= 1;
	tsd.capture.btn= 0;

	if (flags&MK_SHIFT) act= SWE_MERGESEL;	/* shift+leftclick: extend selection */
	else		    act= SWE_NEWSEL;

	if (nClick >= 2) {
	    selectActionModeShift (mousepos, act);

	} else {		/* single click */
	    if (! (flags&MK_SHIFT)) {
		selectSetMode (&term.select, SEL_MODE_CHAR);
	    }
	    selectAction (mousepos, act);
	}

RETURN:
    TermDebugF ((-1, "<- termLeftButtonDown(nClick=%d) mp=(row=%d,col=%d)"
	" %s\n"
	, nClick
	, mousepos->win.p.y, mousepos->win.p.x
	, SelectData_toString (&term.select)
	));
}

/* User released the left mouse button
 *
 * Args:
 * flags - key flags (from Windows message)
 * xpos -  x-position of mouse in pixels
 * ypos -  y-position of mouse in pixels
 */
static void termLeftButtonUp (UINT flags, const TermPosition *mousepos)
{
    TermDebugF ((1, "-> termLeftButtonUp pisxpos=(%d,%d)"
	" %s\n"
	, mousepos->pixel.p.y, mousepos->pixel.p.x
	, SelectData_toString (&term.select)
	));

	if (!inCapture (NULL)) {
	/* If we do not own the mouse, ignore this event
	 */
	    TermDebugF ((-1, "-> termLeftButtonUp: we don't own the mouse\n"));
	    goto RETURN;
	}

	/* Release the mouse capture and stop the selection timer
	 */
	ReleaseCapture();
	termSelectStopTimer();
	tsd.capture.flag= 0;

/*	selectAction (mousepos, SWE_SETTO | SWE_WORDEXPFLAG); */

	if (autoCopy && termSelectCheck()) {
	/* If there is a selection defined, copy it to the clipboard
	 */
	    termSelectCopy();
	}

RETURN:
    TermDebugF ((-1, "<- termLeftButtonUp mp=(row=%d,col=%d)"
	" %s\n"
	, mousepos->win.p.y, mousepos->win.p.x
	, SelectData_toString (&term.select)
	));
}

/* either middle or right button */
static void termPasteButton (int nButton, int nAction, UINT flags, const TermPosition *mousepos)
{
    switch (nAction) {
    case TSME_PRESS:
    case TSME_DBLCLICK:
	termSelectPaste();
	break;

    case TSME_RELEASE:
	break;
    }
}

/* either middle or right button */
static void termNonPasteButton (int nButton, int nAction, int nClick,
	UINT flags, const TermPosition *mousepos)
{
    switch (nAction) {
    case TSME_PRESS:
    case TSME_DBLCLICK:
	/* Capture the mouse until the button is released */
	SetCapture(tsd.wnd);
	if (GetCapture() != tsd.wnd) {
	    tsd.capture.flag= 0;
	    goto RETURN;
	}
	tsd.capture.flag= 1;
	tsd.capture.btn= nButton;

	/* Extend the current selection */
	if (nClick >= 2) {
	    selectActionModeShift (mousepos, SWE_MERGESEL);
	} else {
	    selectAction (mousepos, SWE_MERGESEL);
	}
	break;

    case TSME_RELEASE:
	if (!inCapture (NULL)) {
	/* If we do not own the mouse, ignore this event
	 */
	    goto RETURN;
	}

	/* Release the mouse capture and stop the selection timer
	 */
	ReleaseCapture();
	termSelectStopTimer();
	tsd.capture.flag= 0;

	/* Extend the current selection
	 */
/*	selectAction (mousepos, SWE_SETTO); */

	if (autoCopy && termSelectCheck()) {
	/* If there is a selection defined, copy it to the clipboard
	 */
	    termSelectCopy();
	}
	break;
    }
RETURN:;
}

/* User moved the mouse in the terminal window,
 * and we have captured the mouse
 *
 * Args:
 * flags - key flags (from Windows message)
 * xpos -  x-position of mouse in pixels
 * ypos -  y-position of mouse in pixels
 */
static void termMouseMoveCapture (UINT flags, const TermPosition *mousepos)
{
    TermPosition mp;

    TermDebugF ((1, "-> termMouseMoveCapture: pixpos=(%d,%d) sel=(%d,%d)-(%d,%d)\n"
		, mousepos->pixel.p.y,      mousepos->pixel.p.x
		, term.select.range.from.y, term.select.range.from.x
		, term.select.range.to.y,   term.select.range.to.x));

    /* I don't really get why is this necessary
     */
    mp= *mousepos;
    mp.win.p.x = mp.win.clip.x;     /* Limit the x-position to valid character columns */

    if (mp.win.out & MP_OUT_Y) {
	/* When the mouse is above or below the window, we start the
	 * select timer to trigger automatic scrolling
	 */
	termSelectStartTimer();
    } else {
	/* When mouse is in the terminal window, stop the timer and
	 * extend the selection to include the character/word the
	 * mouse is over
	 */
	termSelectStopTimer();
	selectAction (&mp, SWE_SETTO);
    }
    TermDebugF ((-1, "<- termMouseMoveCapture: pixpos=(%d,%d) sel=(%d,%d)-(%d,%d)\n"
		, mousepos->pixel.p.y,      mousepos->pixel.p.x
		, term.select.range.from.y, term.select.range.from.x
		, term.select.range.to.y,   term.select.range.to.x));
}

static void termMouseMove (UINT flags, const TermPosition *mousepos)
{
    if (GetCapture() == tsd.wnd) {
	termMouseMoveCapture (flags, mousepos);

    } else if (term.mouseRep.mouseReporting >= TERM_MOUSE_REP_X11_BTN_EVENT) {
	int nButton;

	nButton= termGetButton (flags);
	if (nButton>=0) {	/* for now, we work in ESC[1003h mode as in ESC[1002h mode */
	    termSendMouseEvent (nButton, TSME_MOVE, flags, mousepos);
	}
    }

    return;
}

/* We received a timer event - perform automatic scrolling and extend
 * the selection
 */
static void termTimer(void)
{
    POINT point;		/* position of the mouse */
    TermPosition mp;

    /* Get the position of the mouse in screen coordinates and convert
     * it to terminal window coordinates (in pixels)
     */
    GetCursorPos(&point);
    ScreenToClient(tsd.wnd, &point);

    /* Convert the mouse position to character cells directly to avoid
     * clipping in selectAdjustPos()
     */

    termFillPos (point.x, point.y, &mp);

    mp.win.p.x = mp.win.clip.x; /* Limit the x-position to valid character columns */

    if (mp.win.p.y < 0) {
	/* Mouse is above window - scroll and select backwards one line
	 */
	termTryScroll(-1);
	mp.win.p.y= 0;
	selectAction (&mp, SWE_SETTO);

    } else if (point.y >= term.winSize.cy) {
	/* Mouse is below window - scroll and select forwards one line
	 */
	termTryScroll(1);
	mp.win.p.y= mp.win.stclip.y;
	selectAction (&mp, SWE_SETTO);

    } else
	return;

    /* Redraw the caret at the correct position
     */
    winCaretPos(&term.cursor.pos);
}

static BOOL termNumpadKey (TermKey *tk)
{
    BOOL retval= FALSE;

    switch (tk->keycode) {
    case VK_NUMPAD9:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_SKIP, VK_PRIOR, "\x1bOy");
	break;

    case VK_NUMPAD3:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_SKIP, VK_NEXT, "\x1bOs");
	break;

    case VK_NUMPAD8:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_UP, VK_UP, "\x1bOx");
	break;

    case VK_NUMPAD2:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_DOWN, VK_DOWN, "\x1bOr");
	break;

    case VK_NUMPAD6:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_RIGHT, VK_RIGHT, "\x1bOv");
	break;

    case VK_NUMPAD4:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_LEFT, VK_LEFT, "\x1bOt");
	break;

    case VK_NUMPAD7:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_HOME, VK_HOME, "\x1bOw");
	break;

    case VK_NUMPAD1:	
	retval= termNumpadKeyDown (tk, EFK_LOCAL_END, VK_END, "\x1bOq");
	break;

    case VK_NUMPAD0:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_INSERT, VK_INSERT, "\x1bOp");
	break;
	
    case VK_NUMPAD5:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_SKIP, VK_CLEAR, "\x1bOu");
	break;

    case VK_DECIMAL:
	retval= termNumpadKeyDown (tk, EFK_LOCAL_DELETE, VK_DELETE, "\x1bOn");
	break;
    }
    return retval;
}

BOOL termKeyDown (TermKey *tk, const ConstBuffData *pmap)
{
    BOOL retval= FALSE;
    BOOL isRepeat = (tk->flags & KF_REPEAT);
static const TermKeyMapCond ShiftOnly = tkmpcShiftOnly; /* we use this when checking for Shift+PgUp,
							   Shift+PgDn, Shift+Insert */
static const TermKeyMapCond CtrlOnly  = tkmpcCtrlOnly;  /* we use this when checking for Ctrl+Insert */
    const char *actionstr;

    if (!term.autoRepeat && isRepeat)
	goto RETURN;

    TermDebugF ((0, "term.termKeyDown: key=0x%x,scan=0x%x,flags=0x%x,mods=0x%x,locks=0x%x,classe=%d"
		, tk->keycode, tk->scancode, tk->flags
		, tk->modifiers, tk->locks, tk->classe
		));

/*  20120816.LZS: testing for keymap */
    if (pmap) {	/* found mapping */
	TermDebugF ((0, "term.termKeyDown: pmap='%.*s'(len=%d)"
		, (int)pmap->len, pmap->ptr
		, (int)pmap->len
		));
	if (pmap->len) {
	    emulFuncKeyPressB (EFK_LOCAL_SAME, pmap, DTCHAR_ANSI);	/* <FIXME> unicode */
	}
	retval= TRUE;	/* Ignore next WM_CHAR */
	goto RETURN;
    }

    if (tk->keycode==VK_SHIFT ||
	tk->keycode==VK_CONTROL ||
	tk->keycode==VK_MENU) {
	retval= TRUE;
	goto RETURN;
    }

#if 0
    /* Now we 'release' the ALT key -- but we still have 'tkorig' */
    tk->modifiers        &= ~TMOD_ALT;
    tk->keyvect[VK_MENU] &= 0x7f;
#endif
    
    if (tk->classe==TKC_NUMPAD_NUM) {
	retval= termNumpadKey (tk);
	goto RETURN;
    }

    switch (tk->keycode) {
    case VK_PRIOR:
	if (TermKeyMapCondMatch (tk, &ShiftOnly)) {
	    /* Shift-PageUp acts like xterm - scroll back in history
	     * half a terminal height
	     */
	    termTryScroll(-term.winSize.cy / 2);

	} else {
	    TermKey tktmp= *tk;

	    if (connectGetVtKeyMapOption()) tktmp.keycode= VK_DELETE;	/* VTxxx: PAGEUP->REMOVE */
	    actionstr= emulGetKeySeq (&tktmp, term.ansiCursorKeys);
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, actionstr, DTCHAR_ISO88591);
	}
	break;

    case VK_NEXT:
	if ((tk->keycode==VK_NEXT) && TermKeyMapCondMatch (tk, &ShiftOnly)) {
	    /* Shift-PageDown acts like xterm - scroll forwards in
	     * history half a terminal height
	     */
	    termTryScroll(term.winSize.cy / 2);

	} else {
	    TermKey tktmp= *tk;

	    if (connectGetVtKeyMapOption()) tktmp.keycode= VK_NEXT;		/* VTxxx: PAGEDOWN->NEXTSCREEN (this one is identical) */
	    actionstr= emulGetKeySeq (&tktmp, term.ansiCursorKeys);
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, actionstr, DTCHAR_ISO88591);
	}
	break;

    case VK_UP:
	actionstr= emulGetKeySeq (tk, term.ansiCursorKeys);
	emulFuncKeyPress2 (EFK_LOCAL_UP, actionstr, DTCHAR_ISO88591);
	break;

    case VK_DOWN:
	actionstr= emulGetKeySeq (tk, term.ansiCursorKeys);
	emulFuncKeyPress2 (EFK_LOCAL_DOWN, actionstr, DTCHAR_ISO88591);
	break;

    case VK_RIGHT:
	actionstr= emulGetKeySeq (tk, term.ansiCursorKeys);
	emulFuncKeyPress2 (EFK_LOCAL_RIGHT, actionstr, DTCHAR_ISO88591);
	break;

    case VK_LEFT:
	actionstr= emulGetKeySeq (tk, term.ansiCursorKeys);
	emulFuncKeyPress2 (EFK_LOCAL_LEFT, actionstr, DTCHAR_ISO88591);
	break;

    case VK_HOME: {
	TermKey tktmp= *tk;

	if (connectGetVtKeyMapOption()) tktmp.keycode= VK_INSERT;	/* VTxxx: HOME->INSERT */
	actionstr= emulGetKeySeq (&tktmp, term.ansiCursorKeys);
	emulFuncKeyPress2 (EFK_LOCAL_HOME, actionstr, DTCHAR_ISO88591);
	break; }

    case VK_END: {
	TermKey tktmp= *tk;

	if (connectGetVtKeyMapOption()) tktmp.keycode= VK_PRIOR;		/* VTxxx: END->PREVSCREEN */
	actionstr= emulGetKeySeq (&tktmp, term.ansiCursorKeys);
	emulFuncKeyPress2 (EFK_LOCAL_END, actionstr, DTCHAR_ISO88591);
	break; }

    case VK_INSERT:
	if (TermKeyMapCondMatch (tk, &ShiftOnly)) {
	    /* Shift-Insert pastes the selection
	     */
	    termSelectPaste();

	} else if (TermKeyMapCondMatch (tk, &CtrlOnly)) {
	    /* Control-Insert copies the selection
	     */
	    termSelectCopy();

	} else {
	    TermKey tktmp= *tk;

	    if (connectGetVtKeyMapOption()) tktmp.keycode= VK_HOME;	/* VTxxx: INSERT->HOME */
	    actionstr= emulGetKeySeq (&tktmp, term.ansiCursorKeys);
	    emulFuncKeyPress2 (EFK_LOCAL_INSERT, actionstr, DTCHAR_ISO88591);
	}
	break;

    case VK_DELETE: {
	TermKey tktmp= *tk;

	if (connectGetVtKeyMapOption()) tktmp.keycode= VK_END;		/* VTxxx: DELETE->SELECT */
	actionstr= emulGetKeySeq (&tktmp, term.ansiCursorKeys);
	emulFuncKeyPress2 (EFK_LOCAL_DELETE, actionstr, DTCHAR_ISO88591);
	break; }

    case VK_RETURN:
	if ((tk->flags&KF_EXTENDED) && KPM_CHK_OTHERKEY(&term) &&
	     !(tk->modifiers&TMOD_SHIFT)) {
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1bOM", DTCHAR_ISO88591);
	    retval= TRUE;
	}
	break;

    case VK_ADD:
	if (KPM_CHK_OTHERKEY(&term) && !(tk->modifiers&TMOD_SHIFT)) {
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1bOl", DTCHAR_ISO88591);
	    retval= TRUE;
	}
	break;

    case VK_SUBTRACT:
	if (KPM_CHK_FUNCKEY(&term) && !(tk->modifiers&TMOD_SHIFT)) {
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1bOS", DTCHAR_ISO88591);
	    retval= TRUE;
	}
	break;

    case VK_MULTIPLY:
	if (KPM_CHK_FUNCKEY(&term) && !(tk->modifiers&TMOD_SHIFT)) {
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1bOR", DTCHAR_ISO88591);
	    retval= TRUE;
	}
	break;

    case VK_DIVIDE:
	if (KPM_CHK_FUNCKEY(&term) && !(tk->modifiers&TMOD_SHIFT)) {
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1bOQ", DTCHAR_ISO88591);
	    retval= TRUE;
	}
	break;

    case VK_NUMLOCK:
/* if any modifier (Shift, Control, Alt) is pressed,
   or the scancode is zero,
   then let NumLock work as it should */
	if (KPM_CHK_FUNCKEY(&term) &&
	    tk->scancode!=0 && !tk->modifiers) {
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1bOP", DTCHAR_ISO88591);
#ifdef WIN32
/* dirty, dirty, DIRTY hack: undoing NumLock with repeating it */
	    keybd_event (VK_NUMLOCK, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	    keybd_event (VK_NUMLOCK, 0, KEYEVENTF_EXTENDEDKEY, 0);
	    termKeyHackEmulate (tk, VK_NUMLOCK, TKHACK_BOTH); /* change 'tk' accordingly */
#endif
	}
	if (tk->scancode!=0) termSetLockFlags (tk);
	break;

    case VK_CAPITAL:
	termSetLockFlags (tk);
	break;

    case VK_F1:
    case VK_F2:
    case VK_F3:
    case VK_F4:
    case VK_F5:
    case VK_F6:
    case VK_F7:
    case VK_F8:
    case VK_F9:
    case VK_F10:
    case VK_F11:
    case VK_F12:
    case VK_F13:
    case VK_F14:
    case VK_F15:
    case VK_F16:
	termFuncKeyDown (tk, tk->keycode + 1 - VK_F1);
	break;

    case VK_BACK: {
/* 20120823.LZS: Shift and/or Control changes to/from AltBackSpace */
	int f1= (tk->modifiers & (TMOD_SHIFT|TMOD_CONTROL)) != 0;
	int f2= connectGetBs2DelOption() != 0;
	const char *seq;

/*	if (tk->modifers&TMOD_ALT) ESC already sent. Or not? */

	if (f1 != f2) seq= emulGetTerm()->keyAltBackSpace;
	else	      seq= emulGetTerm()->keyBackSpace;
	emulFuncKeyPress2 (EFK_LOCAL_BACKSPACE, seq, DTCHAR_ISO88591);
	retval= TRUE;
	break;
	}

    case VK_SCROLL:	/* 20140325.LZS: VK_SCROLL sends ESC[28~ ('Help' for VT) */
			/* no modifiers for now */
#ifdef WIN32
	if (tk->scancode!=0) {
/* dirty, dirty, DIRTY hack: undoing ScrollLock with repeating it */
	    keybd_event (VK_SCROLL, 0, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
	    keybd_event (VK_SCROLL, 0, KEYEVENTF_EXTENDEDKEY, 0);
	    termKeyHackEmulate (tk, VK_SCROLL, TKHACK_BOTH); /* change 'tk' accordingly */
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1b[28~", DTCHAR_ISO88591);
	}
#else
	emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1b[28~", DTCHAR_ISO88591);
#endif
	retval= TRUE;
	break;

    case VK_PAUSE:	/* 20140325.LZS: VK_PAUSE sends ESC[29~ ('Do' for VT) */
			/* no modifiers for now */
	emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\x1b[29~", DTCHAR_ISO88591);
	retval= TRUE;
	break;

    case '2': case '3': case '4': case '5': 
    case '6': case '7': case '8':

/* if Ctrl is pressed, but neither Shift or Win is pressed */
/* 20160412: 'Alt' cannot be allowed, beacuse of AltGr=Ctrl+Alt */
	if ((tk->modifiers&(TMOD_CONTROL|TMOD_ALT|TMOD_SHIFT|TMOD_WIN))==TMOD_CONTROL) {
	    char mybuff [2];
	    BuffData bd;

	    bd.len= 0;
	    bd.ptr= mybuff;

/* ALT means: insert an ESCAPE before it */
	    if ((tk->modifiers&TMOD_ALT)==TMOD_ALT) {
		bd.ptr[bd.len++]= '\x1b';
	    }

	    if      (tk->keycode == '2') bd.ptr[bd.len++]= '\0';	/* Ctrl+2 --> ^@ 00H */
	    else if (tk->keycode == '8') bd.ptr[bd.len++]= '\x7f';	/* Ctrl+8 --> ^? 7fH */
	    else			 bd.ptr[bd.len++]= (char)(tk->keycode - '3' + 0x1B);
								/* Ctrl+3 --> ^[ 1BH */
								/* Ctrl+7 --> ^_ 1FH */
	    emulFuncKeyPressB (EFK_LOCAL_SKIP, (ConstBuffData *)&bd, DTCHAR_ISO88591);
	    retval= TRUE;
	}
	break;
    }
RETURN:
    return retval;
}

/* User pressed a numpad or extended key
   return: TRUE if processed */

static BOOL termNumpadKeyDown (TermKey *tk,
	int localaction,
	int notnum_key,
	const char *num_applic)
{
    const char *actionstr;
    enum {ACT_NOTHING, ACT_NOTNUM, ACT_NUMAPPL, ACT_TOASCII} action_code= ACT_NOTHING;

    if (tk->flags&KF_EXTENDED) { /* Not-numpad key: send 'notnum' string */
	action_code= ACT_NOTNUM;

    } else /* Numpad */
    if (KPM_CHK_OTHERKEY(&term)) { /* in application mode */
	if (!(tk->modifiers&TMOD_SHIFT)) {
	    action_code= ACT_NUMAPPL;

	} else { /* a Shift reverts to numeric mode */
	    /* Now we 'release' the SHIFT key */
	    tk->modifiers &= ~TMOD_SHIFT;
	    tk->keyvect[VK_SHIFT] = 0;

	    if (tk->locks&LOCK_NUM) {
		action_code= ACT_TOASCII;

	    } else {
		action_code= ACT_NOTNUM;
	    }
	}
    } else { /* in numeric mode */
	if (((tk->locks&LOCK_NUM) && !(tk->modifiers&TMOD_SHIFT)) ||
	   (!(tk->locks&LOCK_NUM) &&  (tk->modifiers&TMOD_SHIFT))) {

	    /* Now we 'release' the SHIFT key */
		tk->modifiers  &= ~TMOD_SHIFT;
		tk->keyvect[VK_SHIFT] = 0;

	    /* Now we 'press' the NUMLOCK key */
		tk->locks |= LOCK_NUM;
		tk->keyvect[VK_NUMLOCK] = 1;

	    action_code= ACT_TOASCII;

	} else { /* no numlock and no shift, or numlock and shift */
	    if ((tk->locks&LOCK_NUM) && (tk->modifiers&TMOD_SHIFT)) {
		/* Now we 'release' the SHIFT key */
		tk->modifiers         &= ~TMOD_SHIFT;
		tk->keyvect[VK_SHIFT] = 0;
	    }

	    action_code= ACT_NOTNUM;
	}
    }

    switch (action_code) {
    case ACT_NOTNUM:
	tk->keycode= (unsigned short)notnum_key;
	actionstr= emulGetKeySeq (tk, term.ansiCursorKeys);
	emulFuncKeyPress2 (localaction, actionstr, DTCHAR_ISO88591);
	break;

    case ACT_NUMAPPL:
	emulFuncKeyPress2 (EFK_LOCAL_SKIP, num_applic, DTCHAR_ISO88591);
	break;

    case ACT_TOASCII:
	termToAscii (tk);
	if (tk->nascii) emulKeyPress ((char)tk->ascii[0], DTCHAR_ANSI);
	break;

    default:
	break;
    }

    return TRUE;
}

/* User pressed a function key (F1-F12)
 *
 * Args:
 * tmodifiers   - TMOD_SHIFT, ...
 * nFkey        - number of the function key (1..)
 */
static void termFuncKeyDown (const TermKey *tk, int nFkey)
{
    Emul *emul;
    char tmp [16];
    const char *pseq= NULL;

    emul = emulGetTerm();

    if ((emul->flags&ETF_SHCTRL_MASK)==ETF_SHCTRL_XTERM_NEW ||
	(emul->flags&ETF_SHCTRL_MASK)==ETF_SHCTRL_KONSOLE) {
	if (nFkey > emul->nMaxFkey) return;

	pseq= emul->keyFn[nFkey-1].ptr;
	pseq= emulShCtrlMagic (tk, pseq, tmp, emul->flags);

    } else {
	TermKey mytkv;

	if (tk->modifiers&TMOD_SHIFT) {
	    mytkv= *tk;
	    tk= &mytkv;

	    nFkey += 10;          /* Dirty hard-code by Lorinczy Zsigmond */
	    ((TermKey *)tk)->modifiers &= ~TMOD_SHIFT;
	}
	if (nFkey > emul->nMaxFkey) return;
	pseq= emul->keyFn[nFkey-1].ptr;

	if ((emul->flags&ETF_SHCTRL_MASK)==ETF_SHCTRL_RXVT) {
	    pseq= emulShCtrlMagic (tk, pseq, tmp, emul->flags);
	}
    }

    if (pseq && *pseq) {
	emulFuncKeyPress2 (EFK_LOCAL_SKIP, pseq, DTCHAR_ISO88591);
    }
}

/* WM_CHAR: User pressed a normal character key
 *
 * Args:
 * key -   key that was pressed
 * flags - key data from Windows message
 */
void termWmChar (WPARAM key, LPARAM flags, int ccode)
{
    /* Get state of Shift and Control keys when message was sent
     */
    BOOL controlPressed = (GetKeyState(VK_CONTROL) & KF_UP) != 0;
    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & KF_UP) != 0;
    BOOL isRepeat = (HIWORD(flags) & KF_REPEAT) != 0;

    if (!term.autoRepeat && isRepeat)
	return;

    if (key == '\t') {
	if (shiftPressed)
	    /* Cannot rely on Windows for Shift+Tab since it passes
	     * Shift+Tab as Tab.
	     */
	    emulFuncKeyPress2 (EFK_LOCAL_HOME, emulGetTerm()->keyBacktab, DTCHAR_ISO88591);
	else
	    emulKeyPress ('\t', DTCHAR_ASCII);

    } else if (controlPressed && key == ' ') {
	    /* Cannot rely on Windows for Ctrl+Key since it passes
	     * Ctrl-Space as Space.  Not good for Emacs, it expects
	     * nul.
	     */
	    emulKeyPress (0, DTCHAR_ASCII);

    } else if (key == '\b') { /* 20120823.LZS. VK_BACK-logic has been moved into termKeyDown */
	emulFuncKeyPress2 (EFK_LOCAL_BACKSPACE, "\b", DTCHAR_ISO88591);

    } else if (key == '\r') {
	if (term.echoKeystrokes || !socketConnected()) { /* this 'if' should be in emul.c */
	    emulFuncKeyPress2 (EFK_LOCAL_SEND, 0, DTCHAR_ISO88591);
	} else {
	    emulKeyPress ('\r', DTCHAR_ASCII);
	}

/*  } else if (key == '\n') {
	emulFuncKeyPress2 (EFK_LOCAL_SKIP, "\n");
*/
    } else {
#if defined(_WIN32)
	emulKeyPress (key, DTCHAR_UNICODE);
#else
	emulKeyPress (key, DTCHAR_ANSI);
#endif
    }
}

/* User pressed a system key (Alt+Key)
 *
 * Args:
 * key -   the key pressed
 * flags - key data from Windows message
 */

#define AltAcceptable(chr) \
(((chr)==0x0d) || \
 ((chr)>=' '  && (chr) <= '~') || \
 ((chr)>=0x80 && (chr) <= 0xff))

BOOL termSysKeyDown (TermKey *tk, const ConstBuffData *pmap)
{
    BOOL isRepeat = (tk->flags & KF_REPEAT) != 0;
    char str [16], *stp;
    BOOL retval= TRUE;  /* ignore next WM_CHAR (if there were any) */
static const TermKeyMapCond LeftAltOnly = tkmpcLeftAltOnly; /* we use this when checking for Alt+number */

    if (!term.autoRepeat && isRepeat)
	return TRUE;

/*  20120818.LZS: testing for keymap */
    if (pmap) {	/* found mapping */
	if (pmap->len) {
	    emulFuncKeyPressB (EFK_LOCAL_SAME, pmap, DTCHAR_ANSI);	/* <FIXME> Unicode */
	}
	return TRUE;	/* Ignore next WM_CHAR */
    }

    if (tk->keycode == VK_MENU || tk->keycode == VK_SHIFT) {
	return TRUE; /* LZs, 2000.10.20 --- should be tested */
    }

    if (! (tk->modifiers&TMOD_ALT)) {
	/* When the user presses F10, Windows sends the event as a
	 * system key - I am glad that Windows makes this stuff *so*
	 * easy.
	 */
	termKeyDown (tk, NULL);

    } else {
	TermKey tkorigv, *tkorig= &tkorigv;

	*tkorig= *tk;

	if (tk->classe==TKC_NUMPAD_NUM) {
	    if (TermKeyMapCondMatch (tkorig, &LeftAltOnly)) { /* do nothing, let ALT+number work */
		retval= TRUE;
	    } else {
		retval= termNumpadKey (tk);
	    }
	    goto RETURN;
	}

	/* Now we 'release' the ALT key */
	tk->modifiers        &= ~TMOD_ALT;
	tk->keyvect[VK_MENU] &= 0x7f;
	termToAscii (tk);

	if (tk->nascii == 0) {     /* LZS 2012.08.23: perhaps ESC shouldn't be sent from here? */
	    /* emulKeyPress ('\x1b'); */ /* LZS 20140120: commented out */
	    termKeyDown (tkorig, NULL);
	    return TRUE;
	}

	stp = str;
	if (tk->nascii>=1 && AltAcceptable (tk->ascii[0])) {
	    *stp++ = 0x1b;
	    *stp++ = (char)tk->ascii[0];
	}
	if (tk->nascii>=2 && AltAcceptable (tk->ascii[1])) {
	    *stp++ = 0x1b;
	    *stp++ = (char)tk->ascii[1];
	}
	if (stp != str) {
	    *stp = 0;
	    emulFuncKeyPress2 (EFK_LOCAL_SKIP, str, DTCHAR_ISO88591);
	}
    }
RETURN:
    return retval;
}

/* Process a file drag-dropped onto the terminal
 */
static void termAcceptDrag(WPARAM wparam)
{
    static char droppedFileName[_MAX_PATH + 1];
    char fbuffer[4096];
    FILE* f;

    if (socketConnected())
    {
        /* To maintain predictability, we only handle 1 file at a time */
	if (DragQueryFile((HDROP) wparam, (UINT)-1, (LPSTR) NULL, 0)==1)
	{
	    DragQueryFile((HDROP) wparam, 0, droppedFileName, sizeof(droppedFileName));
	    /* got filename, paste text from file's content */
	    f = fopen(droppedFileName,"r");
	    fgets (fbuffer, sizeof(fbuffer), f);
            while (!feof(f)){
                socketWrite(fbuffer, strlen(fbuffer));
		fgets (fbuffer, sizeof(fbuffer), f);
	    }
	    fclose(f);
	}
	else
	{
	    /* Warn user */
	    MessageBox(termGetWnd(), "Multiple drop unsupported", "DTelnet",
		MB_APPLMODAL | MB_ICONINFORMATION);
	}
    }
    else
    {
	/* Punish user */
	MessageBox(termGetWnd(), "Not connected to host", "DTelnet",
	   MB_APPLMODAL | MB_ICONHAND);
    }
    DragFinish((HDROP) wparam);
}

/* Hide the caret in the terminal window
 */
void termHideCaret(void)
{
    HideCaret(tsd.wnd);
}

/* Show the caret in the terminal window
 */
void termShowCaret(void)
{
    ShowCaret(tsd.wnd);
}

/* Set the caret visibility in the terminal window
 *
 * Args:
 * visible - should the caret be visible?
 */
void termSetCursorMode(BOOL visible)
{
    if (haveCaret && visible != term.cursorVisible) {
	/* We have the caret and the visibility status has changed
	 */
	if (visible)
	    ShowCaret(tsd.wnd);
	else
	    HideCaret(tsd.wnd);
    }
    term.cursorVisible = visible;
}

/* Application has just gained focus
 */
void termSetFocus(void)
{
    /* Rebuild our caret and restore its position
     */
    CreateCaret(tsd.wnd, term.caretBitmap, term.cursorRect.right, term.cursorRect.bottom);
    winCaretPos (&term.cursor.pos);

    /* Display the caret if necessary
     */
    if (term.cursorVisible)
	ShowCaret(tsd.wnd);

    haveCaret = TRUE;
}

/* Application has just lost focus
 */
void termKillFocus(void)
{
		/* Destroy the caret
     */
    DestroyCaret();
    haveCaret = FALSE;
}

 /* Make sure the terminal scroll region makes sense
  */
static void termAdjustScrollRegion (void)
{
    if (!term.scroll.fFullScreen) {
	if (term.winSize.cy < term.scroll.lines.bottom) {
	    term.scroll.lines.bottom = term.winSize.cy;
	    term.scroll.lines.bottom = term.winSize.cy;
	}
	if (term.scroll.lines.bottom <= term.scroll.lines.top) {
	    term.scroll.fFullScreen= TRUE;
	}
    }
    if (term.scroll.fFullScreen) {
	term.scroll.lines.top = 0;
	term.scroll.lines.bottom = term.winSize.cy;
    }
}

/* The application window has been resized - set the terminal window size
 *
 * Args:
 * cx - pixel width of the terminal window
 * cy - pixel height of the terminal window
 */
void termSetWindowSize (int cx, int cy)
{
    CharSize newSize;		/* new terminal size in characters */
    CharSize oldSize;		/* old terminal size in characters */
    TerminalLineNo growLines;	/* number of lines terminal grew */
    PixelRect newMargin;
    PixelRect pixrectClientArea;

#ifdef WRITEDEBUGFILE
    {
	PixelRect client;
	PixelRect parent;

	GetClientRect (tsd.wnd, (RECT *)&client);
	GetClientRect (telnetGetWnd(), (RECT *)&parent);
	TermDebugF ((1, "-> termSetWindowSize: in=%d-%d winSize=%d-%d"
		" client=%d-%d parent=%d-%d\n"
		, cy, cx
		, term.winSize.cy, term.winSize.cx
		, client.bottom, client.right
		, parent.bottom, parent.right
		));
    }
#endif

    /* Get the new terminal window dimensions in characters
     */
    newSize.cx = (cx - vscrollWidth - termMargin.left) / term.charSize.cx;
    newSize.cy = (cy - termMargin.top) / term.charSize.cy;

    {
        BOOL fChanged= termCheckSizeLimits (&newSize);	/* Khader */

	if (fChanged) { /* doh -- <FIXME> */
	    cx= newSize.cx * term.charSize.cx + vscrollWidth + termMargin.left;
	    cy= newSize.cy * term.charSize.cy + termMargin.top;
	}
    }
    
    /* In Windows16, we get better results if we calculate the margins
       from the output of GetClientRect (see below)
    * newMargin = termMargin;
    * newMargin.right = cx - termMargin.left- newSize.cx*term.charSize.cx - vscrollWidth;
    * newMargin.bottom= cy - termMargin.top - newSize.cy*term.charSize.cy;
    * termMargin= newMargin;
    */

/* No change in dimensions - nothing to do
 * 20130502.LZS: I think that statement was overly optimistic
 *
 *  if (newSize.cx == term.winSize.cx && newSize.cy == term.winSize.cy) {
 *	return;
 *  }
 */

    /* Calculate the number of lines that the window was grown.
     * Negative values mean that the window was shrunk
     */
    growLines = newSize.cy - term.winSize.cy;

		/* Resize the terminal window */
    MoveWindow(tsd.wnd, 0, 0, cx, cy, TRUE);

    GetClientRect (tsd.wnd, (RECT *)&pixrectClientArea);
    newMargin.left = termMargin.left;
    newMargin.right = pixrectClientArea.right
			- termMargin.left
			- newSize.cx*term.charSize.cx;
    newMargin.top = termMargin.top;
    newMargin.bottom= pixrectClientArea.bottom
			- termMargin.top
			- newSize.cy*term.charSize.cy;
    termMargin= newMargin;

    oldSize = term.winSize;

    if (growLines > 0) {
	scrollAddLinesToBottom (growLines, oldSize.cy, newSize.cy);

    } else if (growLines < 0) {
	scrollDelLinesFromBottom (-growLines, oldSize.cy, newSize.cy);

	{
	    BOOL fCrsrMove= FALSE;

	    if (term.cursor.pos.y >= newSize.cy) {
		term.cursor.pos.y = newSize.cy-1;
		fCrsrMove= TRUE;
	    }
	    if (term.cursor.pos.x >= newSize.cx) {
		term.cursor.pos.x = newSize.cx-1;
		fCrsrMove= TRUE;
	    }
	    if (fCrsrMove) winCaretPos(&term.cursor.pos);
	}
    }

    term.winSize = newSize;	/* 20140514.LZS this line got moved down here */

    if (newSize.cx > oldSize.cx) {
	/* Invalidate the extra columns at the right edge of the window
	 */
	winInvalidate (0, oldSize.cx, -1, newSize.cx);
    }

    termAdjustScrollRegion ();

    /* Tell the telnet server about the new size
     */
    if (term.winSize.cx > 0 && term.winSize.cy > 0)
	rawSetWindowSize(/* term.winSize.cx, term.winSize.cy */);

    /* Adjust the history scrollbar
     */
    winSetScrollbar();

#ifdef WRITEDEBUGFILE
    {
	PixelRect client;
	PixelRect parent;

	GetClientRect (tsd.wnd, (RECT *)&client);
	GetClientRect (telnetGetWnd(), (RECT *)&parent);
	TermDebugF ((-1, "<- termSetWindowSize: in=%d-%d winSize=%d-%d (oldsize=%d-%d)"
		" client=%d-%d parent=%d-%d\n"
		, cy, cx
		, term.winSize.cy, term.winSize.cx
		, oldSize.cy, oldSize.cx
		, client.bottom, client.right
		, parent.bottom, parent.right
		));
    }
#endif
}

/* Windows is going to change application window size.
 *
 * Args:
 * winSize - input:  the proposed size of the terminal window
 *           output: incremented values (if too small)
 */
void termWindowPosChanging(SIZE* winSize)
{
    SIZE newSize;

    newSize.cx = (winSize->cx - vscrollWidth - termMargin.left) / term.charSize.cx;
    newSize.cy = (winSize->cy - termMargin.top) / term.charSize.cy;

    if (newSize.cx<10) {
	newSize.cx= 10;
    	winSize->cx = termMargin.left + newSize.cx*term.charSize.cx + vscrollWidth;
    }

    if (newSize.cy<2) {
	newSize.cy= 2;
	winSize->cy = termMargin.top + newSize.cy*term.charSize.cy;
    }
}

/* Format the terminal window geometry
 */
void termFormatSize(char* str)
{
    if (term.useInitPos) {
        sprintf(str, "%dx%d%c%d%c%d", (int)term.winSize.cx, (int)term.winSize.cy,
		(term.winInitPos.cx < 0) ? '-' : '+', abs(term.winInitPos.cx),
		(term.winInitPos.cy < 0) ? '-' : '+', abs(term.winInitPos.cy));
    } else
	sprintf(str, "%dx%d", (int)term.winSize.cx, (int)term.winSize.cy);
}

/* The same for the status windows: reserve order (rows*columns), to be 'stty size'  and 'resize' compatible 
   return string-length */
int termFormatSizeStatus(char* str)
{
    return sprintf(str, "%dx%d", (int)term.winSize.cy, (int)term.winSize.cx);
}

/* Define all of the parameters we need to perform a repaint of the
 * terminal window
 */
typedef struct {
    HDC dc;			/* paint device context */
    ColorRefPair curColors;	/* current text attributes */
    ColorRefPair oldColors;	/* current text attributes */
    BOOL oldColors_is_filled;	/* initialize to FALSE */
    HGDIOBJ oldBrush;		/* remember old brush */
    COLORREF brushColor;	/* current brush color */
    RECT clientRect;		/* window client rectangle */
    WindowLineNo wline;		/* line-no (0 = top line of the window) */
    BufferLineNo bline;		/* line-no (== wline + topVisibleLine) */
    PixelRect lineRect;		/* current line bounds */
    WindowRange reverse;	/* inverse video start */
    Line *line;			/* the line we just paint */
} PaintData;

/* Set the text color attributes
 *
 * Args:
 * pd -   point to paint data
 * attr - new text attributes to set
 * inverse - in the selection reverse the fg and bg
 */
static void termPaintSetAttr (PaintData* pd, ColorRefPair *colors)
{
    COLORREF oldFg, oldBg;

    pd->curColors= *colors;

    oldFg= SetTextColor (pd->dc, pd->curColors.fg);
    oldBg= SetBkColor   (pd->dc, pd->curColors.bg);

    if (!pd->oldColors_is_filled) {
	/* No attributes set yet.  Save the original attributes.
	 */
	pd->oldColors.fg= oldFg;
	pd->oldColors.bg= oldBg;
	pd->oldColors_is_filled= TRUE;
    }
    TermDebugPaintF ((0, "termPaintSetAttr: attr: fg=%06lx bg=%06lx\n"
		, (long)pd->curColors.fg, (long)pd->curColors.bg));
}

/* Set the brush color
 *
 * Args:
 * pd -    point to paint data
 * color - new bush color to set
 */
static void termPaintSetBrush (PaintData* pd, COLORREF color)
{
    if (pd->oldBrush == NULL || pd->brushColor != color) {
	HBRUSH windowBrush;

	windowBrush = CreateSolidBrush(color);

	if (pd->oldBrush == 0)
	    pd->oldBrush = SelectObject(pd->dc, windowBrush);
	else {
	    HBRUSH brush = (HBRUSH)SelectObject(pd->dc, windowBrush);
	    DeleteObject(brush);
	}
	pd->brushColor = color;
    }
}

/* Note: single line, ie wr->from.y==wr->to.y */
static void termTextOut (HDC hdc, 
	WindowLineNo wline, const Line *ln, ColumnInterval *ci,
	Attrib attr)
{
    char sbuff [4096];  /* should be enough for now */
    Buffer buff;
    HFONT hfont;
    POINT upleft;
    int defCharSet;	/* DTCHAR_ANSI/DTCHAR_OEM depending on term.fontCharSet */
    ColumnInterval ciRest= *ci;
    int lgcAction;
    int leave;

    buff.ptr= sbuff;
    buff.len= 0;
    buff.maxlen= sizeof sbuff;

#if defined(_WIN32)
    lgcAction= LGC_MODE_UTF16;
    defCharSet= 0;			/* not relevant */
#else
    if (term.fontCharSet==OEM_CHARSET) {/* shouldn't happen */
	lgcAction= LGC_MODE_NARROW;
	defCharSet= DTCHAR_OEM;		/* we are forced to use 'OEM' */
    } else {
	lgcAction= LGC_MODE_NARROW_NOCONV;
	defCharSet= DTCHAR_ASCII;	/* both 'ANSI' and 'OEM' are ok */
    }
#endif

    for (leave= 0; !leave && !columnIntervalIsEmpty (&ciRest); ) {
	int fontindex;
	int charSet;
	ColumnInterval ciCurrent;

	charSet= defCharSet;
	buff.len= 0;
	linesGetCharsExt (lgcAction, ln, &ciRest, &ciCurrent, &ciRest, &buff, &charSet);
	if (columnIntervalIsEmpty (&ciCurrent) || buff.len==0) {
	    leave= 1;
	    continue;
	}

	if (charSet==DTCHAR_OEM) {
	    if (term.fontCharSet==OEM_CHARSET) fontindex= FONT_MAIN;
	    else                               fontindex= FONT_OEM;
	} else {
	    fontindex = FONT_MAIN;
	}
	if (attr&ATTR_FLAG_UNDERLINE)
	    fontindex |= FONT_UNDERLINE;

	hfont= term.fonts[fontindex];
	SelectObject (hdc, hfont);

	upleft.y = termMargin.top  + wline          * term.charSize.cy;
	upleft.x = termMargin.left + ciCurrent.left * term.charSize.cx;

	TermDebugF ((0, "termTextOut: (%d,%d) str='%.*s'\n"
		, wline, ciCurrent.left, (int)buff.len, buff.ptr));

#if defined(_WIN32)
	TextOutW (hdc, upleft.x, upleft.y, (wchar_t *)buff.ptr, buff.len/2);
#else
	TextOut (hdc, upleft.x, upleft.y, buff.ptr, buff.len);
#endif
    }
}

/* brush should be already set */
static void termPaintEmptyPart (PaintData* pd, const WindowRange *wr)
{
    PixelPoint upleft;
    SIZE  size;

    pointWindowToPixel (&wr->from, &upleft);

    size.cy= term.charSize.cy;
    size.cx= (wr->to.x-wr->from.x) * term.charSize.cx;

    if (wr->from.y==0) { /* include termMargin.top */
	upleft.y= 0; /* or: -= termMargin.top */
	size.cy += termMargin.top;
    }
    if (wr->from.x==0) { /* include termMarin.left */
	upleft.x= 0; /* or: -= termMargin.left; */
	size.cx += termMargin.left;
    }
    if (wr->to.x==term.winSize.cx) { /* include termMarin.right */
	size.cx += termMargin.right;
    }

    TermDebugPaintF ((0, "termPaintEmptyPart: wr=(%d,%d)-(%d,%d), pos=(%d,%d) size=(%d,%d)\n"
		, wr->from.y, wr->from.x, wr->to.y, wr->to.x
		, upleft.y, upleft.x, size.cy, size.cx));

    if (size.cx) {
	PatBlt(pd->dc, upleft.x, upleft.y, size.cx, size.cy, PATCOPY);
    }
}

static void TermPaintLinePart (PaintData *pd, const WindowRange *wr, BOOL in_select)
{
    const Attrib *pattr;
    int  segbeg;
    int  pos, poslim;
    int  lineLen;
    WindowRange wrseg= *wr;
    ColorRefPair end_colors, seg_colors;
    COLORREF brushcolor;

    if (! pd->line) {
	lineLen= 0;
	pattr= NULL;
	getAttrToDisplay (&end_colors, ATTR_FLAG_DEF_FG|ATTR_FLAG_DEF_BG,
		&term.attr, &term.defattr, in_select);

    } else {
	lineLen= pd->line->len;
	pattr= pd->line->attr;
	getAttrToDisplay (&end_colors, pd->line->end_attr,
		&term.attr, &term.defattr, in_select);
    }

    pos= wr->from.x;
    poslim= wr->to.x;
    if (poslim > lineLen) poslim= lineLen;

/* text-part: TextOut */
    while (pos<poslim) {
	Attrib segattr;
	ColumnInterval ci;

	segbeg= pos;
	segattr= pattr[segbeg];
	while (++pos<poslim && pattr[pos]==segattr);

	getAttrToDisplay (&seg_colors, segattr,
	    &term.attr, &term.defattr, in_select);

/* dirty hack to avoid garbage caused by termMargin.left,
   when ClearType is not active */
	if (segbeg==0 && termMargin.left) {
	    wrseg.from.x= 0;
	    wrseg.to.x= 0;
	    brushcolor= seg_colors.bg;
	    termPaintSetBrush (pd, brushcolor);
	    termPaintEmptyPart (pd, &wrseg);
	}
/* end of dirty hack */

	termPaintSetAttr (pd, &seg_colors);
	ci.left= segbeg;
	ci.right= pos;
	termTextOut (pd->dc, wr->from.y, pd->line,
		&ci, segattr);
    }

/* empty-part: PatBlt
   draw this if the logical line is short or there is
   a right margin
 */
    if (pos<wr->to.x ||
	(wr->to.x==term.winSize.cx && termMargin.right)) {
	wrseg.to.x= wr->to.x;
	wrseg.from.x= pos;
	if (pos>0 && pos==term.winSize.cx && pd->line && pd->line->len >= pos) {
	    /* there was a character on the right margin */
	    getAttrToDisplay (&end_colors, pd->line->attr[pos-1],
		&term.attr, &term.defattr, in_select);
	}

	brushcolor= end_colors.bg;
	termPaintSetBrush (pd, brushcolor);
	termPaintEmptyPart (pd, &wrseg);
    }
}

/* Draw a line in the terminal window
 *
 * Args:
 * pd -      point to paint data
 * lineNum - line array index of line to draw
 */
static void termPaintLine (PaintData* pd)
{
    WindowRange wrLine, wrBefSel, wrSel, wrAftSel;
    int bsplit;

    wrLine.from.y= pd->wline;
    wrLine.from.x= 0;
    wrLine.to.y= pd->wline;
    wrLine.to.x= term.winSize.cx;

    TermDebugPaintF ((0, "termPaintLine: wrLine=(%d,%d)-(%d,%d) wl=%d bl=%d reverse=(%d,%d)-(%d,%d) linelen=%d\n"
		, wrLine.from.y, wrLine.from.x, wrLine.to.y, wrLine.to.x
		, pd->wline, pd->bline
		, pd->reverse.from.y, pd->reverse.from.x, pd->reverse.to.y, pd->reverse.to.x
		, pd->line? pd->line->len: -1));

    bsplit= rangeSplit ((const Range *)&wrLine, (const Range *)&pd->reverse,
			(Range *)&wrBefSel, (Range *)&wrSel, (Range *)&wrAftSel);

    TermDebugPaintF ((0, "termPaintLine: split=%d (%d,%d)-(%d,%d),(%d,%d)-(%d,%d),(%d,%d)-(%d,%d)\n"
		, bsplit
		, wrBefSel.from.y, wrBefSel.from.x, wrBefSel.to.y, wrBefSel.to.x
		, wrSel.from.y,    wrSel.from.x,    wrSel.to.y,    wrSel.to.x
		, wrAftSel.from.y, wrAftSel.from.x, wrAftSel.to.y, wrAftSel.to.x));

    if (bsplit&1) {
	TermPaintLinePart (pd, &wrBefSel, FALSE);
    }
    if (bsplit&2) {
	TermPaintLinePart (pd, &wrSel, TRUE);
    }
    if (bsplit&4) {
	TermPaintLinePart (pd, &wrAftSel, FALSE);
    }
}

/* Paint the terminal window
 */
static void termPaint(void)
{
    PaintData pd;		/* painting parameters */
    PAINTSTRUCT paint;
    HGDIOBJ oldFont;		/* remember which font to restore */
    HPALETTE oldPalette= NULL;	/* remember which palette to restore */

    /* Initialise the painting parameters
     */
    memset (&pd, 0, sizeof (pd));

    if (term.select.fHave) {
	/* To paint the selection, we reverse FG and BG attributes
	 */
	BufferRange brTmp;
	if (selectGetRange (&term.select, &brTmp)) {
	    rangeBufferToWindow (&brTmp, &pd.reverse);
	}
    }

    TermDebugPaintF ((1, "-> termPaint: reverse=(%d,%d)-(%d,%d)"
		" %s\n"
		, pd.reverse.from.y, pd.reverse.from.x
		, pd.reverse.to.y,   pd.reverse.to.x
		, SelectData_toString (&term.select)
		));

    pd.oldColors_is_filled= FALSE;	/* initialize to FALSE */
    pd.oldBrush = 0;
    pd.brushColor = 0;

    pd.dc = BeginPaint(tsd.wnd, &paint);
    TermDebugPaintF ((0, "termPaint: BeginPaint.rcPaint=(t=%d,l=%d,b=%d,r=%d)\n"
	, paint.rcPaint.top, paint.rcPaint.left, paint.rcPaint.bottom, paint.rcPaint.right));

    if (termHasPalette) {
	oldPalette = SelectPalette(pd.dc, termPalette, 0);
	RealizePalette(pd.dc);
    }
    oldFont = SelectObject(pd.dc, term.fonts[0]);

    GetClientRect(tsd.wnd, &pd.clientRect);
    TermDebugPaintF ((0, "termPaint: ClientRect=(t=%d,l=%d,b=%d,r=%d)\n"
	, pd.clientRect.top, pd.clientRect.left, pd.clientRect.bottom, pd.clientRect.right));

    pd.wline= 0;
    pd.bline= term.topVisibleLine;
    SetRect((RECT *)&pd.lineRect, 0, 0, pd.clientRect.right, term.charSize.cy);

    /* Start from the top line in the window and iterate over all
     * lines.  Paint each line as necessary
     */
    while (pd.lineRect.top <= pd.clientRect.bottom) {
	/* Display the line if necessary
	 */
	if (RectVisible(pd.dc, (RECT *)&pd.lineRect)) {
	    pd.line= linesGetLine (pd.bline);
	    termPaintLine(&pd);
	}

	/* Move line rectangle down
	 */
	++pd.wline;
	++pd.bline;
	pd.lineRect.top += term.charSize.cy;
	pd.lineRect.bottom += term.charSize.cy;
    }

    /* Erase to bottom of window
     */
    TermDebugPaintF ((0, "termPaint: pd.clientRect=(l=%d,t=%d,r=%d,b=%d)"
		" pd.lineRect=(l=%d,t=%d,r=%d,b=%d)\n"
		, pd.clientRect.left, pd.clientRect.top
		, pd.clientRect.right, pd.clientRect.bottom
		, pd.lineRect.left, pd.lineRect.top
		, pd.lineRect.right, pd.lineRect.bottom
		));

   if (pd.lineRect.top < pd.clientRect.bottom)
	PatBlt(pd.dc,
	       termMargin.left,
	       termMargin.top + pd.lineRect.top,
	       pd.clientRect.right,
	       pd.clientRect.bottom - pd.lineRect.top,
	       PATCOPY);

    /* Clean up all painting parameters
     */
    if (pd.oldBrush != 0) {
	HBRUSH brush = (HBRUSH)SelectObject(pd.dc, pd.oldBrush);
	DeleteObject(brush);
    }
    if (pd.oldColors_is_filled) {
	SetTextColor (pd.dc, pd.oldColors.fg);
	SetBkColor (pd.dc,   pd.oldColors.bg);
    }

    SelectObject(pd.dc, oldFont);
    if (termHasPalette)
	SelectObject(pd.dc, oldPalette);
    EndPaint(tsd.wnd, &paint);

    TermDebugPaintF ((-1, "<- termPaint: exiting\n"
		));
}

static const char notSet [] = "*NOTSET";

void termGetDefAttr (void)
{
    char str[64];
    const char *inifile= telnetIniFile();

    GetPrivateProfileString (termStr, attrStr, notSet, str, sizeof (str), inifile);
    if (strcmp (str, notSet) != 0) {
	AttrDefaultData attr;
	int rc;

	rc= parseAttrib (str, &attr);
	if (rc==0) {
	    termSetBlankAttr (&attr, OptFromIniFile);
	}
    }
}

void termGetProfile(void)
{
    char str[32];
    const char *inifile= telnetIniFile();

    GetPrivateProfileString(
	    termStr, sizeStr, "80x25", str, sizeof(str), inifile);
    termSetGeometry (str, OptFromIniFile);

    bottomOnOutput = GetPrivateProfileInt(
	termStr, bottomOnOutputStr, 1, inifile);
    autoCopy = GetPrivateProfileInt(
	termStr, autoCopyStr, 1, inifile);
    cursorStyle = GetPrivateProfileInt(
	termStr, cursorStyleStr, cursorBlock, inifile);
    term.attachedPrinter = GetPrivateProfileInt(
	termStr, attachPrinterStr, FALSE, inifile);
    term.enablePrintScreen = GetPrivateProfileInt(
	termStr, printScreenStr, FALSE, inifile);

/* 20140814.LZS I dunno if this is the right place... */
    term.attr.current= ATTR_FLAG_DEF_FG | ATTR_FLAG_DEF_BG;
    term.attr.inverseVideo= FALSE;

    termGetDefAttr ();

    mousePasteButton = GetPrivateProfileInt (termStr, pasteKeyStr, 1, inifile);
}

void termSaveDefAttr (const AttrDefaultData *from)
{
    char str[64];
    const char *inifile= telnetIniFile();
    if (!from) from= &term.defattr;

    tostrAttrib (0, from, str);
    WritePrivateProfileString (termStr, attrStr, str, inifile);
}

void termSaveProfile(void)
{
    char str[64];
    const char *inifile= telnetIniFile();

    termFormatSize(str);
    WritePrivateProfileString(termStr, sizeStr, str, inifile);

    sprintf(str, "%d", bottomOnOutput);
    WritePrivateProfileString(
	termStr, bottomOnOutputStr, str, inifile);
    sprintf(str, "%d", autoCopy);
    WritePrivateProfileString(
	termStr, autoCopyStr, str, inifile);
    sprintf(str, "%d", cursorStyle);
    WritePrivateProfileString(
	termStr, cursorStyleStr, str, inifile);
    sprintf(str, "%d", term.attachedPrinter);
    WritePrivateProfileString(
	termStr, attachPrinterStr, str, inifile);
    sprintf(str, "%d", term.enablePrintScreen);
    WritePrivateProfileString(
	termStr, printScreenStr, str, inifile);
    termSaveDefAttr(NULL);
    sprintf(str, "%d", mousePasteButton);
    WritePrivateProfileString (termStr, pasteKeyStr, str, inifile);
}

/* Window procedure for the terminal window
 */
static LRESULT CALLBACK termWndProc
	(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MOUSEMOVE:
	return termMouseAction (message, wparam, lparam, telnetLastMsg());

    case WM_TIMER:
	termTimer();
	return 0;

    case WM_VSCROLL: {
	WORD action;
	WORD position;
#ifdef WIN32
	action= LOWORD(wparam);
	position= HIWORD(wparam);
#else
	action= wparam;
	position= LOWORD(lparam);
#endif
	termUseScrollbar(action, position);
	return 0; }

    case WM_PALETTECHANGED:
	if ((HWND)wparam == termGetWnd())
    	    return 0;
    case WM_QUERYNEWPALETTE:
	/* About to receive window focus - realise our palette
	 */
	return termPaletteChanged();

    case WM_PAINT:
	termPaint();
	return 0;
    case WM_ERASEBKGND:
	/* We have no background to fill, just do nothing */
	return 1;

    case WM_DROPFILES:
        termAcceptDrag(wparam);
        return 0;

    case WM_CREATE:
        DragAcceptFiles(wnd, TRUE);
	return 0;

    case WM_DESTROY:
	/* Window is being destroyed, clean up
	 */
        DragAcceptFiles(wnd, FALSE);

/*	termSaveProfile(); */

	fontDestroy (term.fonts);

	if (termHasPalette)
	    DeleteObject(termPalette);

	linesFreeList (&term.linebuff);
	linesFreeList (&term.alt.linebuff);

	if (term.caretBitmap != NULL)
	    DeleteObject(term.caretBitmap);
	if (termTimerId != 0)
	    KillTimer(tsd.wnd, ID_TERM_TIMER);
	return 0;
    }
    return DefWindowProc(wnd, message, wparam, lparam);
}

/* Create the terminal window
 *
 * Args:
 * instance - application instance handle
 * parent -   parent window (application window)
 */
BOOL termCreateWnd(HINSTANCE instance, HWND parent)
{
    HDC dc;			/* check if we can use a palette */
    RECT client;		/* get size of parent window client area */
    int devcaps;

    TermDebugF ((1, "-> termCreateWnd start\n"));

    termSetLockFlags (NULL);

    {
	TermKey tk;

	termFillTermKey (0, 0, &tk);
	tsd.fCapsLock= (tk.locks & LOCK_CAPS) != 0;
	tsd.fNumLock=  (tk.locks & LOCK_NUM)  != 0;
    }

    /* Query Windows for width of a scrollbar
     */
    vscrollWidth = GetSystemMetrics(SM_CXVSCROLL);

    /* Allocate our line array - initially empty
     */
    term.maxLines = MAX_TERM_LINES;
    linesAllocList (&term.linebuff, term.maxLines);

    /* Create the terminal window initially taking all of the parent
     * window client area
     */
    GetClientRect(parent, &client);
    TermDebugF ((0, "termCreateWnd: client-rect: %d-%d\n"
	, client.bottom, client.right));

    tsd.wnd = CreateWindow(termWinClass, NULL, WS_CHILD|WS_VSCROLL|WS_VISIBLE,
			   0, 0, client.right, client.bottom,
			   parent, NULL, instance, NULL);
    if (!tsd.wnd)
	return FALSE;

    /* Check if the window can support a palette
     */
    dc = GetDC(tsd.wnd);
    devcaps= GetDeviceCaps(dc, RASTERCAPS);
    termHasPalette= (devcaps & RC_PALETTE) != 0; /* RC_PALETTE==0x100 */
    ReleaseDC(tsd.wnd, dc);
    /* Build the palette if necessary
     */
    if (termHasPalette)
	termBuildPalette();

    TermDebugF ((0, "termCreateWnd before calling termSetFont\n"));

    /* Set the window font - this will set the real terminal window size
     */
    termSetFontPart1();
    /* Initialise the application window title
     */
    telnetSetTitle();
    emulResetTerminal (TRUE);

    winSetScrollbar();
/*
    TermDebugF ((0, "termCreateWnd before calling ShowWindow\n"));
    ShowWindow(tsd.wnd, SW_SHOW);
    TermDebugF ((0, "termCreateWnd before calling UpdateWindow\n"));
    UpdateWindow(tsd.wnd);
*/
#ifdef WRITEDEBUGFILE
    {
	PixelRect client;
	PixelRect parent;

	GetClientRect (tsd.wnd, (RECT *)&client);
	GetClientRect (telnetGetWnd(), (RECT *)&parent);
	TermDebugF ((-1, "<- termCreateWnd exit winSize=%d-%d"
		" client=%d-%d parent=%d-%d\n"
		, term.winSize.cy, term.winSize.cx
		, client.bottom, client.right
		, parent.bottom, parent.right
		));
    }
#endif

    return TRUE;
}

/* Register the terminal window class
 */
BOOL termInitClass(HINSTANCE instance)
{
    WNDCLASS wc;

    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = termWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = termWinClass;

    return RegisterClass(&wc);
}

int setTermCharSet (TermCharSet *to, int n)
{
    int rc= 0;

    switch (n) {
    case TCSN_UK:
	to->flags = TCSF_GRAPH_NO;
	to->charset = ukCharSet;
	break;

    case TCSN_US:
	to->flags = TCSF_GRAPH_NO;
	to->charset = usCharSet;
	break;

    case TCSN_ALT:
	to->flags = TCSF_GRAPH_NO;
	to->charset = altCharSet;
	break;

    case TCSN_GRAPHICS:
	to->flags = TCSF_GRAPH_YES;
	to->charset = graphicsCharSet;
	break;

    case TCSN_ALTGRAPH:
	to->flags = TCSF_GRAPH_YES;
	to->charset = altGraphicsCharSet;
	break;

    default:
	rc= -1;
    }
    return rc;	
}

int termGetAltBuff (void)
{
    return term.altbuff;
}

void termSetAltBuff (int sel /* 0/1 = std/alt */)
{
    int y;

    if (sel==term.altbuff) return;
    term.altbuff= sel;

    if (term.topVisibleLine + term.winSize.cy > term.linebuff.allocated) {
	linesAllocList (&term.linebuff, term.topVisibleLine + term.winSize.cy);
    }
    if (term.winSize.cy > term.alt.linebuff.allocated) {
	linesAllocList (&term.alt.linebuff, term.winSize.cy);
    }

    for (y= 0; y<term.winSize.cy; ++y) {
	Line *ltosave, *ltorestore;

	ltosave= term.linebuff.list[term.topVisibleLine + y];
	ltorestore= term.alt.linebuff.list[y];

	if (ltorestore==NULL) {
	    ltorestore= linesAllocInit (NULL);

	} else if (y > term.alt.linebuff.used) {
	    linesInit (ltorestore);
	}

	term.linebuff.list[term.topVisibleLine + y] = ltorestore;
	term.alt.linebuff.list[y] = ltosave;
    }
    term.alt.linebuff.used= term.winSize.cy;
}

void termClearAltBuff (void)
{
    term.altbuff= 0;		/* default buffer */
    term.alt.linebuff.used= 0;	/* nothing is saved */
}

/* Invalidate an area specified by character coordinates (window-based coordinates)
 */
void winInvalidateRect (const WindowRect *rect)
{
    PixelRect myrect;			/* build window rect to invalidate */

    if (rect->top<=0) myrect.top= 0;
    else              myrect.top= rect->top*term.charSize.cy + termMargin.top - termBlur.cy;

    if (rect->bottom<0 ||
	rect->bottom>=term.winSize.cy) {
	myrect.bottom= term.winSize.cy*term.charSize.cy + termMargin.top + termMargin.bottom;
    } else {
	myrect.bottom= rect->bottom*term.charSize.cy + termMargin.top + termBlur.cy;
    }

    if (rect->left<=0) myrect.left= 0;
    else	       myrect.left= rect->left*term.charSize.cx + termMargin.left - termBlur.cx;

    if (rect->right<0 || rect->right>=term.winSize.cx) {
	myrect.right = term.winSize.cx*term.charSize.cx + termMargin.left + termMargin.right;
    } else {
	myrect.right = rect->right*term.charSize.cx + termMargin.left + termBlur.cx;
    }

    TermDebugF ((0, "winInvalidateRect: winrect=(t=%d,l=%d,b=%d,r=%d) xrect=(t=%d,l=%d,b=%d,r=%d)\n"
		, rect->top, rect->left, rect->bottom, rect->right
		, myrect.top, myrect.left, myrect.bottom, myrect.right));

    InvalidateRect(tsd.wnd, (RECT *)&myrect, FALSE);
}

void winInvalidateAll (void)
{
    InvalidateRect(tsd.wnd, NULL, FALSE);
}

void termFillPos (int xpos, int ypos, TermPosition *into)
{
    into->pixel.p.x= xpos;
    into->pixel.p.y= ypos;

    pointPixelToWindow (&into->pixel.p, &into->win.p, &into->pixel.mod);

    into->win.out= 0;

    pointClipWindowPoint (&into->win.p, &into->win.clip,   1); /* clip into 0..winSize.cy, 0..winSize.cx */
    pointClipWindowPoint (&into->win.p, &into->win.stclip, 2); /* clip into 0..winSize.cy-1, 0..winSize.cx-1 */

    if (into->win.p.x < 0) {
	into->win.out |= MP_OUT_LEFT;

    } else if (into->win.p.x >= term.winSize.cx) {
	into->win.out |= MP_OUT_RIGHT;
    }

    if (into->win.p.y<0) {
	into->win.out |= MP_OUT_TOP;

    } else if (into->win.p.y>=term.winSize.cy) {
	into->win.out |= MP_OUT_BOTTOM;
    }

    pointWindowToBuffer (&into->win.p, &into->log.p);
    into->log.line   = NULL;
    into->log.linelen= 0;
    into->log.out    = 0;

    into->log.sel    = into->log.p;

    if (into->win.p.y>=0 && into->win.p.y<term.winSize.cy) {
	if (into->log.p.y  <  0) {
	    into->log.out |= MP_OUT_TOP;
	    into->log.sel.x= 0;

	} else if (into->log.p.y >= term.linebuff.used) {
	    into->log.out |= MP_OUT_BOTTOM;
	    into->log.sel.x= term.winSize.cx;

	} else {
	    into->log.line= linesGetLine (into->log.p.y);
	    into->log.linelen = into->log.line->len;

	    if (into->log.p.x < 0) {
		into->log.out |= MP_OUT_LEFT;
		into->log.sel.x= 0;

	    } else if (into->log.p.x > into->log.linelen) {
		into->log.out |= MP_OUT_RIGHT;
		into->log.sel.x= term.winSize.cx;
	    }
	}
    }
    TermDebugFillF ((0, "termFillPos: y=%d,x=%d -> row=%d(+%dpx), col=%d(+%dpx)"
		 "\n"
		, ypos, xpos
		, into->win.p.y, into->pixel.mod.y
		, into->win.p.x, into->pixel.mod.x));
}

/* is this line the last of the scrolling region? */
BOOL termIsScrollBottom (TerminalLineNo tline)
{
    BOOL f;

    if (term.scroll.fFullScreen)
	f= tline==term.winSize.cy-1;
    else
	f= tline==term.scroll.lines.bottom-1;
    return f;
}

/* is this line the last of the screen? */
BOOL termIsScreenBottom (TerminalLineNo tline)
{
    BOOL f;

    f= tline==term.winSize.cy-1;
    return f;
}

void termCalculatePixelSize (const CharSize *charSize, PixelSize *pixelSize, BOOL fWithScrollbar)
{
    if (!charSize) charSize= (CharSize *)&term.winSize;

    pixelSize->cy = charSize->cy * term.charSize.cy + termMargin.top;
    pixelSize->cx = charSize->cx * term.charSize.cx + termMargin.left;
    if (fWithScrollbar) pixelSize->cx += vscrollWidth;
}

void termCalculateCharSize (const PixelSize *pixelSize, CharSize *charSize, BOOL fWithScrollbar)
{
    SIZE_CX tmpx;

    if (!charSize) charSize= (CharSize *)&term.winSize;

    tmpx= pixelSize->cx;
    if (fWithScrollbar) tmpx -= vscrollWidth;
    
    charSize->cy = (pixelSize->cy - termMargin.top) / term.charSize.cy;
    charSize->cx = (tmpx - termMargin.left) / term.charSize.cx;
}

/* is this line in the scrolling region? */
BOOL termIsInScrollRegion (TerminalLineNo tline)
{
    BOOL f;

    f= term.scroll.fFullScreen ||
	(tline >= term.scroll.lines.top && tline < term.scroll.lines.bottom);
    return f;
}

/* termSetKeyPadMode:

    sets bit#01 according to fset;
    sets *fStatusChange to 1, if the status-line should be changed
 */
void termSetKeyPadMode (unsigned char into[2], int fAppl, int *fStatusChange)
{
    int fChange= 0;
    int i;

    if (!into) into= term.keypadMode;
    if (fAppl) fAppl= 1;		/* just to be safe */

    for (i=0; i<2; ++i) {
	if (!(into[i]&KPM_PRESET_FLAG) && 
            (into[i]&KPM_SET_APPLI) != fAppl) fChange= 1;
        into[i] = (unsigned char)((into[i] & ~KPM_SET_APPLI) | fAppl);
    }
    if (fStatusChange && fChange) *fStatusChange= 1;
}

int termKeyApplicationMode(const unsigned char pfrom[2], int index)
{
    int retval;
    const unsigned char *from;

    if (pfrom) from= pfrom;
    else       from= term.keypadMode;

    if (from[index]&KPM_PRESET_FLAG) retval= from[index]&KPM_PRESET_APPLI;
    else			     retval= from[index]&KPM_SET_APPLI;

    return retval;
}

int termKeypadModeStr(const unsigned char pfrom[2], char reply[3])
/* one or two characters, 
eg. "A" = every key is preset to 'application'
    "n" = set to 'numeric'
    "Nn" = top four keys are preset to 'numeric', the rest are set to 'numeric'
 */
{
    int i, retlen;
    const unsigned char *from;

    if (pfrom) from= pfrom;
    else       from= term.keypadMode;

    for (i=0; i<2; ++i) {
	if (from[i]&KPM_PRESET_FLAG) reply[i]= (char)((from[i]&KPM_PRESET_APPLI)? 'A': 'N');
	else			     reply[i]= (char)((from[i]&KPM_SET_APPLI)?    'a': 'n');
    }
    if (reply[1]==reply[0]) retlen= 1;
    else		    retlen= 2;
    reply[retlen]= '\0';
    return retlen;
}

int termKeypadModeStatus (const unsigned char from[2], char reply[])
{
    char tmp[3];
    int tmplen, i;
    char *q= reply;

    tmplen= termKeypadModeStr (from, tmp);

    for (i=0; i<tmplen; ++i) {
	if (i>0) *q++ = '/';
	switch (tmp[i]) {
	case 'A': q = stpcpy (q, "Appl"); break;
	case 'a': q = stpcpy (q, "appl"); break;
	case 'N': q = stpcpy (q, "Num");  break;
	case 'n': q = stpcpy (q, "num");  break;
	default:  q += sprintf (q, "x%02x", (unsigned char)tmp[i]); break;
	}
    }
    return (int)(q-reply);
}

/* status of NumLock an CapsLock for status line */
void termKeyLockStatus (char *buff)
{
    char *q= buff;

    if (tsd.fCapsLock) q= stpcpy (q, "Caps");
    if (q!=buff) *q++ = ' ';
    if (tsd.fNumLock)  q= stpcpy (q, "Num");
    *q= '\0';
}

int termGetPasteButton (void)
{
    return mousePasteButton;	/* 1/2 = middle/right */
}

int termSetPasteButton (int n)
{
    int oldval;

    if (n<1 || n>2) return -1;
    oldval= mousePasteButton;
    mousePasteButton= n;
    return oldval;
}

static void termSetLockFlags (const TermKey *tk)
{
    int fCapsLock, fNumLock;
    TermKey mytk;

    if (!tk) {
	termFillTermKey (0, 0, &mytk);
	tk= &mytk;
    }

    fCapsLock= (tk->locks & LOCK_CAPS) != 0;
    fNumLock=  (tk->locks & LOCK_NUM)  != 0;

    if (fCapsLock != tsd.fCapsLock ||
	fNumLock  != tsd.fNumLock) statusInvalidate ();

    tsd.fCapsLock = fCapsLock;
    tsd.fNumLock  = fNumLock;
}
