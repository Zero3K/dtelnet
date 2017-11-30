/* select.c */

#include "preset.h"
#include <windows.h>
#include <ctype.h>
#include <string.h>

#include "termwin.h"
#include "term.h"
#include "select.h"

/* #define WRITEDEBUGFILE 1 */
#ifdef WRITEDEBUGFILE
    #include "debuglog.h"
    #define TermDebugF(par) dtelnetDebugF par
#else
    #define TermDebugF(par)
#endif

const char *SelectData_toString (const SelectData *from)
{
    static char *strbuff= NULL;

    if (!strbuff) strbuff= xmalloc (128);
    sprintf (strbuff,
	" select=(%d,%d)-(%d,%d),have=%d,mode=%d"
	, (int)from->range.from.y, (int)from->range.from.x
	, (int)from->range.to.y,   (int)from->range.to.x
	, from->fHave, from->nMode);
    return strbuff;
}

/* Give back the start and end of the selection (start <= end)
 *
 * Return:  TRUE if there is selection, and the interval is non-empty
 */
static BOOL selectDataToWindow (const SelectRange *from, BufferRange *into)
{
    BOOL retval;
    int cmp;

    cmp= pointCmp ((POINT *)&from->from,
		   (POINT *)&from->to);
    if (!cmp) { /* empty range */
	*into= *(BufferRange *)from;
	retval= FALSE;
	goto RETURN;
    }

    if (cmp<=0) {
	into->from= from->from;
	into->to=   from->to;
    } else {
	into->from= from->to;
	into->to=   from->from;
    }

/*  TermDebugF ((0, "termSelectGetRange into1=(%d,%d)-(%d,%d)\n"
	, into->from.y, into->from.x, into->to.y, into->to.x)); */

/* we might have to hack the interval a bit, eg:
	(window=(0..24)x(0..79))
		(0,80)-(1,20) --> (1, 0)-(1,20)
		(1,40)-(2, 0) --> (1,40)-(1,80)
 */
    if (into->from.x >= term.winSize.cx) {
	into->from.x= 0;
	++into->from.y;
    }
    if (into->to.x <= 0) {
	into->to.x= term.winSize.cx;
	--into->to.y;
    }
/*  TermDebugF ((0, "termSelectGetRange into2=(%d,%d)-(%d,%d)\n"
	, into->from.y, into->from.x, into->to.y, into->to.x)); */

    cmp= pointCmp ((POINT *)&into->from, (POINT *)&into->to);
    retval= cmp<0;

RETURN:
    return retval;
}

BOOL selectGetRange (const SelectData *seldat, BufferRange *into)
{
    BOOL retval;

    if (seldat->fHave) {
	retval= selectDataToWindow (&seldat->range, into);
    } else {
	memset (into, 0, sizeof (*into));
	retval= FALSE;
    }

    return retval;
}

BOOL selectRangeContainsPoint (const SelectData *seldat, const BufferPoint *bp)
{
    BufferRange brSel;
    BOOL retval;

    retval= seldat->fHave &&
	    selectGetRange (seldat, &brSel) &&
	    bufferRangeContainsPoint (&brSel, bp);

    return retval;
}

BOOL selectGetInterval (const SelectData *seldat, BufferInterval *into)
{
    BufferRange brange;
    BOOL fHave;

    fHave= selectGetRange (seldat, &brange);
    if (!fHave) {
	into->top= 0;
	into->bottom= 0;

    } else {
	into->top=    brange.from.y;
	into->bottom= brange.to.y + 1;
    }
    return fHave;
}

#define SWE_NEWSEL	  0
#define SWE_MERGESEL	  1
#define SWE_SETTO	  2

#ifndef max
#define max(v1,v2) ((v1)>(v2) ? (v1): (v2))
#define min(v1,v2) ((v1)<(v2) ? (v1): (v2))
#endif

#define IsSpaceAtIndex(line,n) linesIsSpaceAtIndex((line),(n))

static void TSA_WordExt_Empty (BufferRange *pr,
	const Line *line_from, const Line *line_to)
{
    if (!line_from) {
	pr->from.x= 0;
	pr->to.x  = term.winSize.cx;

    } else if (pr->from.x >= line_from->len ||
	       pr->from.x >= term.winSize.cx) {
	pr->from.x= min (line_from->len, term.winSize.cx);
	pr->to.x  = term.winSize.cx;

    } else {
	++pr->to.x;
    }
}

static void TSA_WordExt_Left (BufferPoint *bptFrom, const Line *line_from)
{
    BufferPoint bptTmp= *bptFrom;

    if (line_from && bptTmp.x>=0 && bptTmp.x < line_from->len) {
	BOOL leave= FALSE;
	BOOL fSpace;
	const Line *ln= line_from;

	fSpace= IsSpaceAtIndex (ln, bptTmp.x);

	while (!leave) {
	    if (bptTmp.x<=0) {
		ln= linesGetPossiblePrevLine (bptTmp.y);
		if (ln==NULL) {
		    leave= TRUE;
		} else {
		    --bptTmp.y;
		    bptTmp.x= ln->len;
		}
	    } else {
		--bptTmp.x;
		if (IsSpaceAtIndex (ln, bptTmp.x) == fSpace) {
		    *bptFrom= bptTmp;
		} else {
		    leave= TRUE;
		}
	    }
	}
    }
}

static void TSA_WordExt_Right (BufferPoint *bptTo, const Line *line_to)
{
    BufferPoint bptTmp= *bptTo;

    if (line_to && bptTmp.x>0 && bptTmp.x <= line_to->len) {
	BOOL leave= FALSE;
	BOOL fSpace;
	const Line *ln= line_to;

	fSpace= IsSpaceAtIndex (ln, bptTmp.x-1);

	while (!leave) {
	    if (bptTmp.x==ln->len) {
		ln= linesGetPossibleNextLine (bptTmp.y);
		if (ln==NULL) {
		    leave= TRUE;
		} else {
		    ++bptTmp.y;
		    bptTmp.x= 0;
		}
	    } else {
		if (IsSpaceAtIndex (ln, bptTmp.x) == fSpace) {
		    ++bptTmp.x;
		    *bptTo= bptTmp;
		} else {
		    leave= TRUE;
		}
	    }
	}
    }
}

static void TSA_WordExt (BufferRange *pr, BOOL fEmpty,
	const Line *line_from, const Line *line_to)
{
    if (fEmpty) {
	TSA_WordExt_Empty (pr, line_from, line_to);
    }

    TSA_WordExt_Left  (&pr->from, line_from);
    TSA_WordExt_Right (&pr->to,   line_to);
}

static void TSA_LineExt (BufferRange *pr, BOOL fEmpty,
	const Line *line_from, const Line *line_to)
{
    pr->from.x= 0;
    pr->to.x= term.winSize.cx;
}

static void TSA_ParaExtLeft (BufferPoint *pt, const Line *line)
{
    int leave;

    if (!line) {
	pt->x=  0;
	goto RETURN;
    }

    for (leave= 0; !leave; ) {
	Line *testline;

	testline= linesGetPossiblePrevLine (pt->y);
	if (testline) --pt->y;
	else	      leave= 1;
    }
    pt->x= 0;

RETURN:;
}

static void TSA_ParaExtRight (BufferPoint *pt, const Line *line)
{
    int leave;

    if (!line) {
	pt->x= term.winSize.cx;
	goto RETURN;
    }

    for (leave= 0; !leave; ) {
	Line *testline;

	testline= linesGetPossibleNextLine (pt->y);
	if (testline) ++pt->y;
	else	      leave= 1;
    }

    pt->x= term.winSize.cx;

RETURN:;
}

static void TSA_ParaExt (BufferRange *pr, BOOL fEmpty,
	const Line *line_from, const Line *line_to)
{
    TSA_ParaExtLeft (&pr->from, line_from);
    TSA_ParaExtRight (&pr->to, line_to);
}

static void TSA_Extension (SelectRange *pr, int selmode)
{
    BOOL fInverse;
    BOOL fEmpty;
    int cmp;
    const Line *line_from, *line_to;
    BufferRange wr;

    if (selmode==SEL_MODE_CHAR) goto RETURN;

    cmp= bufferPointCmp (&pr->from, &pr->to);
    fInverse= cmp>0;
    fEmpty= cmp==0;

    if (fInverse) {
	wr.from= pr->to;
	wr.to=   pr->from;
    } else {
	wr.from= pr->from;
	wr.to=   pr->to;
    }

    line_from= linesGetLine (wr.from.y);
    line_to=   linesGetLine (wr.to.y);

    switch (selmode) {
    case SEL_MODE_WORD:
	TSA_WordExt (&wr, fEmpty, line_from, line_to);
	break;

    case SEL_MODE_LINE:
/* it could be this way:
 *	TSA_WordExt (&wr, fEmpty, line_from, line_to);
 *	line_from= linesGetLine (wr.from.y);
 *	line_to=   linesGetLine (wr.to.y);
 */
	TSA_LineExt (&wr, fEmpty, line_from, line_to);
	break;

    case SEL_MODE_PARA: TSA_ParaExt (&wr, fEmpty, line_from, line_to); break;
    default: break; }

    if (fInverse) {
	pr->from= wr.to;
	pr->to=   wr.from;
    } else {
	pr->from= wr.from;
	pr->to=   wr.to;
    }

RETURN:;
}

typedef struct TSA_Data {
    SelectData *seldat;		/* often, but not always &term.select */
    BufferPoint *slleft;	/* &seldat->from or &seldat->to: the leftmost */
    BufferPoint *slright;	/* &seldat->from or &seldat->to: the rightmost */
} TSA_Data;

static void TSA_SetTo (const BufferPoint *pt, const BufferRange *ptrange,
		       TSA_Data *tsd)
{
	int fromcmp;

	fromcmp= bufferPointCmp (pt, &tsd->seldat->range.from);

/*	if (fromcmp==0) <FIXME> what's now?! */

	if (fromcmp < 0) {
	    /* new 'to' is before 'from' */
	    tsd->seldat->range.to = ptrange->from;

	    /* we might have to change 'from' too */
	    if (bufferPointCmp (&ptrange->to, &tsd->seldat->range.from) > 0) {
		tsd->seldat->range.from= ptrange->to;
	    }

	} else {
	    /* new 'to' is after 'from' (or the same) */
	    tsd->seldat->range.to = ptrange->to;

	    /* we might have to change 'from' too */
	    if (bufferPointCmp (&ptrange->from, &tsd->seldat->range.from) < 0) {
		tsd->seldat->range.from= ptrange->from;
	    }
	}
	if (tsd->seldat->nMode != SEL_MODE_CHAR) {
	    TSA_Extension (&tsd->seldat->range, tsd->seldat->nMode);
	}
}

static void TSA_Reduce (const BufferPoint *pt, const BufferRange *ptrange,
			TSA_Data *tsd)
{
    int cmp;
    BufferRange brTest1, brTest2;

    TermDebugF (( 1, "TSA_Reduce: pt=(%d,%d), ptrange=(%d,%d)-(%d,%d) %s"
	, pt->y, pt->x
	, ptrange->from.y, ptrange->from.x
	, ptrange->to.y,   ptrange->to.x
	, SelectData_toString (tsd->seldat)
	));

    /* clicked inside the selctio, */
    /* we have to choose which part of selection we keep */

    selectGetRange (tsd->seldat, &brTest1);
    brTest2= brTest1;

    bufferPointMin (tsd->slleft, &ptrange->from, &brTest1.from);
    brTest1.to= ptrange->to;

    brTest2.from= ptrange->from;
    bufferPointMax (tsd->slright, &ptrange->to, &brTest2.to);

    /* now let's see which one is the bigger */
    cmp= bufferRangeCompareSize (&brTest1, &brTest2, term.winSize.cx);

    if (cmp>=0) {	/* keep the first part (brTest1) */
	tsd->seldat->range= brTest1;

    } else {		/* keep the second part (bsTest2) */
			/* but in reverse order */
	tsd->seldat->range.from= brTest2.to;
	tsd->seldat->range.to=   brTest2.from;
    }

    TermDebugF ((-1, "TSA_Reduce: pt=(%d,%d), ptrange=(%d,%d)-(%d,%d) %s"
	, pt->y, pt->x
	, ptrange->from.y, ptrange->from.x
	, ptrange->to.y,   ptrange->to.x
	, SelectData_toString (tsd->seldat)
	));
}

static void TSA_Union (const BufferPoint *pt, const BufferRange *ptrange,
			TSA_Data *tsd)
{
    BufferRange oldrange, newrange;

    TermDebugF (( 1, "TSA_Union: pt=(%d,%d), ptrange=(%d,%d)-(%d,%d) %s"
	, pt->y, pt->x
	, ptrange->from.y, ptrange->from.x
	, ptrange->to.y,   ptrange->to.x
	, SelectData_toString (tsd->seldat)
	));

    oldrange.from= *tsd->slleft;
    oldrange.to=   *tsd->slright;

    bufferRangeUnion (&oldrange, ptrange, &newrange);

    *tsd->slleft = newrange.from;
    *tsd->slright= newrange.to;

    TermDebugF ((-1, "TSA_Union: pt=(%d,%d), ptrange=(%d,%d)-(%d,%d) %s"
	, pt->y, pt->x
	, ptrange->from.y, ptrange->from.x
	, ptrange->to.y,   ptrange->to.x
	, SelectData_toString (tsd->seldat)
	));
}

void selectAction (const TermPosition *mp, int action)
{
    SelectData sdBefore= term.select;

    selectPerformAction (&term.select, mp, action);
    selectInvalidateDiff (&sdBefore, &term.select);
}

void selectPerformAction (SelectData *seldat,
			  const struct TermPosition *mp, int action)
{
    BufferPoint pt;
    BufferRange ptrange;	/* the point you clicked might get extended to */
				/* a word/line/paragraph */
    TSA_Data tsd;

    TermDebugF ((1, "-> selectPerformAction action=%d mp: orig=(%d,%d) hacked=(%d,%d)"
	" %s\n"
	, action
	, mp->log.p.y,   mp->log.p.x
	, mp->log.sel.y, mp->log.sel.x
	, SelectData_toString (seldat)
	));

    pt = mp->log.sel;	/* the selected position */

/* extension, if requested */
    ptrange.from= pt;
    ptrange.to=   pt;
    if (seldat->nMode != SEL_MODE_CHAR) {
	TSA_Extension (&ptrange, seldat->nMode);
    }

    TermDebugF ((0, "selectPerformAction: pt=(%d,%d) ptrange=(%d,%d)-(%d,%d)\n"
	, pt.y,		  pt.x
	, ptrange.from.y, ptrange.from.x
	, ptrange.to.y,   ptrange.to.x));

    if (! seldat->fHave ||
	action==SWE_NEWSEL) {
	seldat->range= ptrange;
	seldat->fHave= TRUE;
	goto RETURN;
    }

/* get pointers to the minimum and the maximum of the current select-range */

    tsd.seldat= seldat;
    if (bufferPointCmp (&seldat->range.from, &seldat->range.to) <= 0) {
	tsd.slleft=  &seldat->range.from;
	tsd.slright= &seldat->range.to;

    } else {
	tsd.slleft=  &seldat->range.to;
	tsd.slright= &seldat->range.from;
    }
    TermDebugF ((0, "selectPerfomAction slleft=(%d,%d) slright=(%d,%d)\n"
	, tsd.slleft->y,  tsd.slleft->x
	, tsd.slright->y, tsd.slright->x));

    if (action==SWE_MERGESEL) {
	if (tsd.seldat->nMode!=SEL_MODE_CHAR) {
	    TSA_Extension (&tsd.seldat->range, tsd.seldat->nMode);

	    TermDebugF ((0, "selectPerfomAction: after extension: slleft=(%d,%d) slright=(%d,%d)\n"
		, tsd.slleft->y,  tsd.slleft->x
		, tsd.slright->y, tsd.slright->x));
	}

/* we do reduction reduction if the whole 'ptrange' lies inside
   the extended 'seldat.range' */
	if (selectRangeContainsPoint (seldat, &ptrange.from) &&
	    selectRangeContainsPoint (seldat, &ptrange.to)) {
	    /* Shift+Click (or Right Click) in the selection */
	    TSA_Reduce (&pt, &ptrange, &tsd);

	} else {
	    /* click outside the selection: actual merge */
	    TSA_Union (&pt, &ptrange, &tsd);
	}

    } else if (action==SWE_SETTO) {
	TSA_SetTo (&pt, &ptrange, &tsd);
    }

RETURN:
    TermDebugF ((-1, "<- selectPerformAction mp=(row=%d,col=%d)"
	" %s\n"
	, mp->win.p.y, mp->win.p.x
	, SelectData_toString (seldat)
	));
}

/* Invalidate the area that is in 'before' and not in 'after' (or vice versa)
 */

void selectInvalidateDiff (const SelectData *before, const SelectData *after)
{
    BufferRange srBefore= EmptyRange, srAfter= EmptyRange;
    BOOL fSelBef, fSelAft;
    BufferRange srDiff=  EmptySelectRange;
    BufferRect rect= EmptyRect;
    WindowRect wrect;

    fSelBef= selectGetRange (before, &srBefore);
    fSelAft= selectGetRange (after,  &srAfter);

    TermDebugF ((1, "-> selectInvalidateDiff"
	" before=(%d:(%d,%d)-(%d,%d)), after=(%d:(%d,%d)-(%d,%d))\n"
	, fSelBef, srBefore.from.y, srBefore.from.x, srBefore.to.y, srBefore.to.x
	, fSelAft, srAfter.from.y,  srAfter.from.x,  srAfter.to.y,  srAfter.to.x));

    if (!fSelBef) {
	if (!fSelAft) goto RETURN;
	else srDiff= srAfter;

    } else {
	if (!fSelAft) srDiff= srBefore;
	else {
	    int nFromCmp, nToCmp;

	    nFromCmp= pointCmp ((POINT *)&srBefore.from, (POINT *)&srAfter.from);
	    nToCmp=   pointCmp ((POINT *)&srBefore.to,   (POINT *)&srAfter.to);
	    TermDebugF ((0, "nFromCmp=%d, nToCmp=%d\n", nFromCmp, nToCmp));

	    if (nFromCmp==0) {
		if (nToCmp<0) {
		    srDiff.from= srBefore.to;
		    srDiff.to=   srAfter.to;

		} else if (nToCmp>0) {
		    srDiff.from= srAfter.to;
		    srDiff.to=   srBefore.to;

		} else goto RETURN;

	    } else if (nToCmp==0) {
		if (nFromCmp<0) {
		    srDiff.from= srBefore.from;
		    srDiff.to=   srAfter.from;

		} else if (nFromCmp>0) {
		    srDiff.from= srAfter.from;
		    srDiff.to=   srBefore.from;
		}

	    } else {
		if (nFromCmp<0) srDiff.from= srBefore.from;
		else		srDiff.from= srAfter.from;

		if (nToCmp<0)   srDiff.to= srAfter.to;
		else		srDiff.to= srBefore.to;
	    }
	}
    }

    rect.top=  srDiff.from.y;
    rect.left= srDiff.from.x;

    if (srDiff.to.x!=0) {
	rect.bottom= srDiff.to.y;
	rect.right=  srDiff.to.x;

    } else {				/* beginning of the next line -> end of the prev line */
	rect.bottom= srDiff.to.y-1;
	rect.right=  -1;
    }

    if (rect.top != rect.bottom) {	/* complete the rectangle */
	rect.left=  0;			/* the beginning of the 'from-line' */
	rect.right= -1;			/* the end of the 'to-line' */
    }
    if (rect.bottom>=0) ++rect.bottom;	/* rect.bottom is not included, so we have to increment it!
					   (but only if non-negative) */

    rectBufferToWindow (&rect, &wrect);

    TermDebugF ((0, "selectInvalidateDiff calls winInvalidateRect"
		" (t=%d,l=%d,b=%d,r=%d)\n"
		, wrect.top, wrect.left, wrect.bottom, wrect.right));
    winInvalidateRect (&wrect);

RETURN:
    TermDebugF ((-1, "<- selectInvalidateDiff"
	" diff=((%d,%d)-(%d,%d)) rect=((%d,%d),(%d,%d))\n"
	, srDiff.from.y, srDiff.from.x, srDiff.to.y, srDiff.to.x
	, rect.top, rect.left, rect.bottom, rect.right));

    return;
}

void selectBufferLinesRemoved (SelectData *sd, BufferLineNo first, BufferLineNo ndel)
{
    if (!sd->fHave) goto RETURN;

    bufferPointAdjustAfterLinesRemove (&sd->range.from, first, ndel);
    bufferPointAdjustAfterLinesRemove (&sd->range.to,   first, ndel);

    if (bufferPointCmp (&sd->range.from, &sd->range.to)==0) {
	sd->fHave = FALSE;
    }
RETURN:;
}

void selectBufferLinesInserted (SelectData *sd, BufferLineNo first, BufferLineNo nins)
{
    if (!sd->fHave) goto RETURN;

    bufferPointAdjustAfterLinesInsert (&sd->range.from, first, nins);
    bufferPointAdjustAfterLinesInsert (&sd->range.to,   first, nins);

RETURN:;
}

void selectSetMode (SelectData *sd, int selmode)
{
    TermDebugF (( 1, "selectSetMode(%d) %s"
	, (int)selmode, SelectData_toString (sd)
	));

    if (selmode==SETSEL_MODE_SAME) {		/* do nothing */
    
    } else if (selmode==SETSEL_MODE_NEXT) {	/* next */
	sd->nMode= (term.select.nMode+1)%SEL_MODE_LIMIT;

    } else {
	sd->nMode= selmode; /* SEL_MODE_CHAR/WORD/LINE/PARA */
    }

    TermDebugF ((-1, "selectSetMode(%d) %s"
	, (int)selmode, SelectData_toString (sd)
	));
}

/* let's try to decide if the action changed the selection */
static BOOL selectEqual (const SelectData *l, const SelectData *r)
{
    BOOL lempty, rempty, retval;
    BufferRange lrange;
    BufferRange rrange;

    lempty= !selectGetRange (l, &lrange);
    rempty= !selectGetRange (r, &rrange);

    if (lempty && rempty) {
	retval= TRUE;

    } else if (lempty != rempty) {
	retval= FALSE;

    } else {
	int fcmp, tcmp;

	fcmp= bufferPointCmp (&lrange.from, &rrange.from);
	tcmp= bufferPointCmp (&lrange.to,   &rrange.to);
	retval= fcmp==0 && tcmp==0;
    }

    return retval;
}

void selectActionModeShift (const struct TermPosition *mp, int action)
{
    SelectData sdwork= term.select;

    TermDebugF (( 1, "selectActionModeShift(%d) %s"
	, (int)action, SelectData_toString (&sdwork)
	));

    do {
	sdwork.nMode= (sdwork.nMode+1)%SEL_MODE_LIMIT;
	selectPerformAction (&sdwork, mp, action);
    } while (selectEqual (&sdwork, &term.select) &&
	     sdwork.nMode != term.select.nMode);
/* infinite loops aren't welcome */

    selectInvalidateDiff (&term.select, &sdwork);
    term.select= sdwork;

    TermDebugF ((-1, "selectActionModeShift(%d) %s"
	, (int)action, SelectData_toString (&sdwork)
	));
}
