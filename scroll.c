/* scroll.c */

#include <windows.h>

#include "scroll.h"
#include "term.h"

static void scrollSecureBufferSpace (BufferLineNo nLines)
{
    BufferLineNo nAvailable;
    BufferLineNo nDelFromHistory;

/* this function is called at window-resize too,
   so nLines > term.winSize.cy is not necessarily a bug
 */
    ASSERT (nLines >= 0 && term.winSize.cy+nLines <= term.maxLines);
    if (nLines==0) goto RETURN;

    nAvailable= linesAvailable ();
    if (nAvailable>=nLines) goto RETURN;

    nDelFromHistory= nLines - nAvailable;

    scrollDeleteFromHistory (nDelFromHistory);

RETURN:
    return;
}

void scrollDeleteFromHistory (BufferLineNo nDelFromHistory)
{
    BufferLineNo nHistory;
    BufferInterval biDelFromHistory;
    BufferInterval biSelect;
    BOOL fHaveSel, fDelSel;

    nHistory= linenoTerminalToBuffer (0);

    if (nDelFromHistory > nHistory) nDelFromHistory= nHistory;
    if (nDelFromHistory==0) goto RETURN;

    biDelFromHistory.top= 0;
    biDelFromHistory.bottom= nDelFromHistory;

/*  Will the deletion be visible?
    If so, we do some scrolling (winUpdate + winLinesScroll)
    and set topVisibleLine to zero

    Otherwise we only decrement topVisibleLine
 */
    if (term.topVisibleLine < nDelFromHistory) {
	LineNo nStep= nDelFromHistory - term.topVisibleLine;
	WindowInterval wiScroll;

	winUpdate ();				/* repaint before scroll */
	wiScroll.top= 0;
	wiScroll.bottom= term.winSize.cy;
	winLinesScroll (&wiScroll, nStep);

	term.topVisibleLine= 0;

    } else {
	term.topVisibleLine -= nDelFromHistory;
    }

/* if the selection intersects with the area to be erased,
   we cancel the selection
 */

    fHaveSel= selectGetInterval (&term.select, &biSelect);
    if (fHaveSel) {
	fDelSel= intervalHasCommonElements ((Interval *)&biSelect, (Interval *)&biDelFromHistory);
	if (fDelSel) {
	    termCancelSelect (0);
	} else {
	    term.select.range.from.y -= nDelFromHistory;
	    term.select.range.to.y   -= nDelFromHistory;
	}
    }

/* and finally 'linesDeleteOldestLines' performs the actual deletion */

    linesDeleteOldestLines (nDelFromHistory);

RETURN:
    return;
}

void scrollAddLinesToBottom (LineNo nNewLines, LineNo cyBefore, LineNo cyAfter)
{
    ASSERT (nNewLines>=0 && nNewLines<=term.maxLines);
    ASSERT (cyAfter == cyBefore+nNewLines && 	/* this part might be dropped in an extended version */
	    cyBefore<= term.maxLines &&
 	    cyAfter <= term.maxLines);

    if (nNewLines<=0) goto RETURN;

    scrollSecureBufferSpace (nNewLines);

    linesNewLines (nNewLines, NULL);
    term.winSize.cy = cyAfter;

RETURN:
    return;
}

void scrollDelLinesFromBottom (LineNo nDelLines, LineNo cyBefore, LineNo cyAfter)
{
    ASSERT (nDelLines>=0 && nDelLines <= cyBefore);
    ASSERT (cyAfter == cyBefore - nDelLines && 	/* this part might be dropped in an extended version */
	    cyBefore<= term.maxLines &&
 	    cyAfter <= term.maxLines);
    
    if (nDelLines<=0) goto RETURN;
    linesDeleteBottomLines (nDelLines);

RETURN:
    return;
}

#ifdef MORE_COPY_TO_HISTORY
static void scrollCopyToHistory (const TerminalInterval *tiFrom)
{
    BufferLineNo nCopy, nAvailable;
    BufferLineNo nInsPos;

    ASSERT (0 <= tiFrom->top && 
	    tiFrom->top <= tiFrom->bottom && 
	    tiFrom->bottom <= term.winSize.cy);

    nCopy= tiFrom->bottom - tiFrom->top;
    if (nCopy==0) goto RETURN;

    nAvailable= linesAvailable ();
    if (nAvailable < nCopy) {
	ASSERT (nCopy + term.winSize.cy <= term.maxLines);
	scrollSecureBufferSpace (nCopy);
    }

/* if selection overlaps with the top of the window, we cancel it */
    {
	BufferInterval biSelect;
	BOOL fHaveSel;

	fHaveSel= selectGetInterval (&term.select, &biSelect);
	if (fHaveSel) {
	    BufferLineNo nTopLine= linenoTerminalToBuffer (0);

	    if (biSelect.top <= nTopLine && nTopLine < biSelect.bottom) {
		termCancelSelect (0);
	    }
	}
    }

/* create empty space in 'linebuff' right before the 'terminal' */
    nInsPos=  linenoTerminalToBuffer (0);
    linesInsertArea (&term.linebuff, nInsPos, nCopy);
    term.topVisibleLine += nCopy;

    {
	BufferLineNo nFromPos, nToPos;

	nToPos=  linenoTerminalToBuffer (-nCopy);		/* negative means before the terminal */
	nFromPos= linenoTerminalToBuffer (tiFrom->top);

/* copy lines to the new empty space */
	linesDupArea (&term.linebuff, nToPos, nFromPos, nCopy);
    }

/* correct the selection, too */
    selectBufferLinesInserted (&term.select, nInsPos, nCopy);

RETURN:
    return;
}
#endif

static void ScrollNoHistory (const TerminalInterval *tiScroll, LineNo numLines);
static void ScrollToHistory (const TerminalInterval *tiScroll, LineNo numLines);

void Scroll (const TerminalInterval *tiScroll, LineNo numLines)
{
    BOOL fGoToHistory;

    fGoToHistory= numLines>0 && tiScroll->top==0;

    if (fGoToHistory) ScrollToHistory (tiScroll, numLines);
    else	      ScrollNoHistory (tiScroll, numLines);
}

static void ScrollToHistory (const TerminalInterval *tiScroll, LineNo numLines)
{
    BufferInterval biSelect;
    BOOL fHaveSel, fUppSel, fLowSel;
    LineNo nTotLines;	/* number of visible lines in 'ti' */
    LineNo nStep;	/* number of 'steps' = max (abs(numLines),nTotLines) */
    BufferInterval biUpperPart;
    BufferInterval biLowerPart;

    nTotLines= tiScroll->bottom - tiScroll->top;
    nStep= numLines; if (nStep<0) nStep= -nStep;
    if (nStep>=nTotLines) nStep= nTotLines;

    scrollSecureBufferSpace (nStep);

    biUpperPart.top=    0;
    biUpperPart.bottom= linenoTerminalToBuffer (tiScroll->bottom);
    biLowerPart.top=    biUpperPart.bottom;
    biLowerPart.bottom= linenoTerminalToBuffer (term.winSize.cy);

    fHaveSel= selectGetInterval (&term.select, &biSelect);
    if (fHaveSel) {
	fUppSel= intervalHasCommonElements ((Interval *)&biSelect, (Interval *)&biUpperPart);
	fLowSel= intervalHasCommonElements ((Interval *)&biSelect, (Interval *)&biLowerPart);

	if (fUppSel &&fLowSel) {
	    termCancelSelect (0);
	    fHaveSel= FALSE;
	}
    }
    winUpdate ();

/* allocate 'nStep' lines at the bottom of buffer */
    linesNewLines (nStep, NULL);

    if (!intervalIsEmpty ((Interval *)&biLowerPart)) { /* move new lines up */
	TerminalInterval tiLowerPart;

	intervalBufferToTerminal (&biLowerPart, &tiLowerPart);
	linesInsert (tiLowerPart.top, tiLowerPart.bottom+nStep, nStep);
    }
    biLowerPart.bottom += nStep;
    biLowerPart.top    += nStep;
    biUpperPart.bottom += nStep;

    if (termBottomOnOutput ()) {
	term.topVisibleLine += nStep;
	termLinesScroll (tiScroll, numLines);
    }

    if (fHaveSel && fLowSel) {
	term.select.range.from.y += nStep;
	term.select.range.to.y   += nStep;
    }
}

static void ScrollNoHistory (const TerminalInterval *tiScroll, LineNo numLines)
{
    BufferInterval biSelect;
    TerminalInterval tiSelect;
    TerminalInterval tiMoveFrom; /* the lines we move (line-numbers before moving) */
/*  TerminalInterval tiMoveTo;      the lines we move (line-numbers after moving) */
    TerminalInterval tiLose;     /* the lines we overwrite */
/*  TerminalInterval tiNew;         the lines we empty after moving */
    BOOL fHaveSel, fSelectNested;
    LineNo nTotLines;	/* number of visible lines in 'ti' */
    LineNo nMove;	/* number of lines to scroll */
    LineNo nClear;		/* number of lines to clear */
    LineNo nStep;		/* number of 'steps' = abs(numLines) */

    if (numLines==0) return; /* very funny */

    nTotLines= tiScroll->bottom - tiScroll->top;
    nStep= numLines; if (nStep<0) nStep= -nStep;
    if (numLines>0) {
	if (numLines>nTotLines) {
	    numLines= nTotLines;
	    nStep= numLines;
	}
    } else {
	if (numLines<-nTotLines) {
	    numLines= -nTotLines;
	    nStep= -numLines;
	}
    }
    nMove= nTotLines-nStep;
    nClear= nStep;


    if (numLines>0) {	/* scroll up */
	tiMoveFrom.top=     tiScroll->bottom - nMove;
	tiMoveFrom.bottom=  tiScroll->bottom;
/*	tiMoveTo.top=       tiScroll->top;
	tiMoveTo.bottom=    tiScroll->top + nMove; */
	tiLose.top=	    tiScroll->top;
	tiLose.bottom=      tiScroll->top + nClear;
/*	tiNew.top=          tiScroll->bottom - nClear;
	tiNew.bottom=       tiScroll->bottom; */

    /* 20140805.LZS: Perhaps we still could copy these lines into the history */
    /* 20150220.LZS: Didn't like the result */
#ifdef MORE_COPY_TO_HISTORY
	{ /* could depend on some option... */
	    scrollCopyToHistory (&tiLose);
	}
#endif

    } else {		/* scroll down */
	tiMoveFrom.top=     tiScroll->top;
	tiMoveFrom.bottom=  tiScroll->top + nMove;
/*	tiMoveTo.top=       tiScroll->bottom - nMove;
	tiMoveTo.bottom=    tiScroll->bottom; */
	tiLose.top=         tiScroll->bottom - nClear;
	tiLose.bottom=      tiScroll->bottom;
/*	tiNew.top=          tiScroll->top;
	tiNew.bottom=       tiScroll->top + nClear; */
    }

    fHaveSel= selectGetInterval (&term.select, &biSelect);
    if (fHaveSel) {
	intervalBufferToTerminal (&biSelect, &tiSelect);
	fSelectNested= intervalIsNested ((Interval *)&tiSelect, (Interval *)&tiMoveFrom);

	if (intervalHasCommonElements ((Interval *)&tiSelect, (Interval *)&tiLose) ||
	    (intervalHasCommonElements ((Interval *)&tiSelect, (Interval *)&tiMoveFrom) &&
	     !fSelectNested)) {
	    termCancelSelect (0);
	    fHaveSel= FALSE;
	    fSelectNested= FALSE;
	}
    }
    winUpdate ();

    if (numLines>0) {
	linesDelete (tiScroll->top, tiScroll->bottom, numLines);
    } else {
	linesInsert (tiScroll->top, tiScroll->bottom,-numLines);
    }
    termLinesScroll (tiScroll, numLines);

    if (fHaveSel && fSelectNested) {
	term.select.range.from.y -= numLines;	/* check for negative ?! */
	term.select.range.to.y   -= numLines;
    }
}
