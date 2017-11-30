/* editor.c */

#include <windows.h>

#include "editor.h"

#include "emul.h"
#include "term.h"
#include "lines.h"
#include "resource.h"

#define CM_FREE ID_CM_FREE /* free cursor movement */
#define CM_TEXT ID_CM_TEXT /* restricted cursor movement */

static int nCursorMovement = CM_TEXT;

int editGetCMMode (void)
{
    return nCursorMovement;
}

int editSetCMMode (int nCM)
{
    int nOldCM;

    nOldCM = nCursorMovement;
    nCursorMovement = nCM;
    return nOldCM;
}

/* Move the cursor
 *
 * in the comments 'epl' is end of physical line (term.winSize.cx-1)
 *                 'eol' is end of logical line (line->len -1)
 */
void editMoveCursor(int direction)
{
    int lineIdx;
    Line *line;

    switch (direction) {
    case EFK_LOCAL_UP:
	if (term.cursor.pos.y==0) { /* <FIXME> Scroll */
	    break;
	}
	lineIdx = linesTerminalToLine(term.cursor.pos.y);
	if (lineIdx==0) return;
	--lineIdx;
	--term.cursor.pos.y;
	linesCreateTo(lineIdx);
	line = linesGetLine(lineIdx);
        if (nCursorMovement != CM_FREE  && term.cursor.pos.x > line->len) {
	    term.cursor.pos.x = line->len;
	}
	break;

    case EFK_LOCAL_DOWN:
	if (term.cursor.pos.y>=term.winSize.cy) { /* <FIXME> Scroll */
	    break;
	}
	lineIdx = linesTerminalToLine(term.cursor.pos.y);
	++lineIdx;
	line = linesGetLine(lineIdx);
	if (!line) break;
	++term.cursor.pos.y;
	linesCreateTo(lineIdx);
	line = linesGetLine(lineIdx);
        if (nCursorMovement != CM_FREE  && term.cursor.pos.x > line->len) {
	    term.cursor.pos.x = line->len;
	}
	break;

    case EFK_LOCAL_LEFT:
	if (term.cursor.pos.x > 0) {
	    term.cursor.pos.x--;
	    break;
	}
	if (term.cursor.pos.y==0) { /* <FIXME> Scroll */
	    break;
	}

	    lineIdx = linesTerminalToLine(term.cursor.pos.y);
	    if (lineIdx==0) return;
	    --lineIdx;
	    linesCreateTo(lineIdx);
	    line = linesGetLine(lineIdx);

	    if (line->len > term.winSize.cx ||
		(line->len == term.winSize.cx && ! line->wrapped)) {
		term.cursor.pos.x = term.winSize.cx;        /* eopl+1*/

	    } else if (nCursorMovement == CM_FREE){
		term.cursor.pos.x = term.winSize.cx - 1;    /* eopl */

	    } else if (line->wrapped && line->len>0) {
		term.cursor.pos.x = line->len -1;           /* eoll */

	    } else {
		term.cursor.pos.x = line->len;              /* eoll+1 */
	    }
	    --term.cursor.pos.y;
	    break;

    case EFK_LOCAL_RIGHT:
	lineIdx = linesTerminalToLine(term.cursor.pos.y);
	linesCreateTo(lineIdx);
	line = linesGetLine(lineIdx);

	if (((term.cursor.pos.x < term.winSize.cx -1) &&
	    (nCursorMovement == CM_FREE ||
	     term.cursor.pos.x < line->len -1 ||
	     (term.cursor.pos.x == line->len -1 && ! line->wrapped))) ||
	    (term.cursor.pos.x == term.winSize.cx -1 &&
	     line->len >= term.winSize.cx && ! line->wrapped)) {
	    ++term.cursor.pos.x;

	} else {
	    ++lineIdx;
	    line = linesGetLine(lineIdx);
	    if (!line) break;
	    term.cursor.pos.x = 0;
	    ++term.cursor.pos.y;
	}
	break;

    case EFK_LOCAL_HOME:
	lineIdx = linesTerminalToLine(term.cursor.pos.y);
	linesCreateTo(lineIdx);

	while (lineIdx > 0 &&
	       (line = linesGetLine (lineIdx-1))->wrapped) {
	    --term.cursor.pos.y;
	    --lineIdx;
	}
	term.cursor.pos.x = 0;
	break;

    case EFK_LOCAL_END:
	lineIdx = linesTerminalToLine(term.cursor.pos.y);
	linesCreateTo(lineIdx);
	line = linesGetLine(lineIdx);
	while (line->wrapped) {
	    ++term.cursor.pos.y;
	    ++lineIdx;
	    line = linesGetLine(lineIdx);
	}
	term.cursor.pos.x = line->len;
	break;
    }
}

