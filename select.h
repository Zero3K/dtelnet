/* select.h */

#ifndef SELECT_H
#define SELECT_H

#include "termwin.h"

/* SelectRange: the first and last positions of selections:
   lines (y-values) are 'linebuff-based', ie
   you have to subtract topVisibleLine to get screen-lines

   if from< to, the selected range is [from, to-1]
   if from==to, the selected range is empty
   if from> to, the selected range is [to, from-1]

   or:
   if from!=to, the range is [min(from,to), max(from,to)-1]

   20130503.LZS:
	SelectRange is now based on BufferRange (termwin.h),
	but the 'to'<'from' case is different (see above)
 */

typedef BufferRange SelectRange;

#define EmptySelectRange EmptyRange

typedef struct SelectData {
    SelectRange range;
    int  nMode;		/* 0/1/2/3 = characters/words/lines/paragraphs */
    BOOL fHave;		/* do we have a selection at the moment? */
} SelectData;

#define SEL_MODE_CHAR	0	/* select characters */
#define SEL_MODE_WORD	1	/* select words (doubleclick) */
#define SEL_MODE_LINE	2	/* select lines (tripleclick) */
#define SEL_MODE_PARA	3	/* select paragraphs (quadrupleclick) */
#define SEL_MODE_LIMIT	4	/* valid range: [0,LIMIT) */

/* for debugging: */
const char *SelectData_toString (const SelectData *from);
/* returns static buffer, read only */

/* Give back the start and end of the selection (start <= end)
 *
 * Return:  TRUE if there is selection, and it is non-empty
 */
BOOL selectGetRange (const SelectData *seldat, BufferRange *into);

BOOL selectGetInterval (const SelectData *seldat, BufferInterval *into);

BOOL selectRangeContainsPoint (const SelectData *seldat, const BufferPoint *bp);

#define SWE_NEWSEL	  0
#define SWE_MERGESEL	  1
#define SWE_SETTO	  2

struct TermPosition;

/* addition to SEL_MODE_*** (not actual modes) */
#define SETSEL_MODE_SAME   1000	/* do not change */
#define SETSEL_MODE_NEXT   1001	/* char->word; word->line, ..., para->char */

void selectSetMode (SelectData *sd, int selmode);

void selectAction (const struct TermPosition *mp, int action);
/* calls selectPerformAction (&term.select) */

void selectPerformAction (SelectData *seldat, const struct TermPosition *mp, int action);

void selectActionModeShift (const struct TermPosition *mp, int action);
/* calls selectSetMode (SETSEL_MODE_NEXT)
   and  selectPerformAction
   until gets a different selection.
   This depends on the context:
	are we in an empty line?
	are we in inside a word?
	is the current line a single word?
	is the current line part of a paragraph?
*/

/* Invalidate the area that is in 'before' and not in 'after' (or vice versa)
 */

void selectInvalidateDiff (const SelectData *before, const SelectData *after);

/* adjust select-range after deleting lines from the Buffer */

void selectBufferLinesRemoved (SelectData *sd, BufferLineNo first, BufferLineNo ndel);

/* adjust select-range after inserting lines into the Buffer */

void selectBufferLinesInserted (SelectData *sd, BufferLineNo first, BufferLineNo nins);

#endif
