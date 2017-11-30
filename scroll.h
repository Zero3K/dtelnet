/* scroll.h */

#ifndef SCROLL_H
#define SCROLL_H

#include "termwin.h"

/* Scroll integrates these functions:
	termCancelSelectBeforeScroll
	winUpdate
	linesDelete/Insert
	winLinesScroll
 */

void Scroll (const TerminalInterval *tScroll, LineNo numLines);

/* scrollAddLinesToBottom,DelLinesFromBottom: called by termSetWindowSize
 * adjusts term.winSize.cy,
 *        term.topVisibleLine
 *	  term.select
 */
void scrollAddLinesToBottom (LineNo nNewLines, LineNo cyBefore, LineNo cyAfter);
void scrollDelLinesFromBottom (LineNo nDelLines, LineNo cyBefore, LineNo cyAfter);

/* scrollDeleteFromHistory:
 *    delete the first lines from history,
 *    (but no more than the total amount)
 *
 *    called by:
 *	scrollAddLinesToBottom
 *      CSI [ 3 J
 */
void scrollDeleteFromHistory (BufferLineNo nDelFromHistory);

#endif
