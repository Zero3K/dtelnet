/* lines.h
 * Copyright (c) 1997 David Cole
 *
 * Manage line array for terminal with history
 */
#ifndef __lines_h
#define __lines_h

#include "platform.h"
#include "termwin.h"
#include "attrib.h"

/* Keep all lines in an array of pointers to lines.
 * Each line has the following attributes;
 * len	- need to known where the end of line is for selection
 * text	- array of [len] characters representing the line text
 * attr	- array of [len] bytes representing the attributes of
 *	  the characters in the line. 
 * end_attr - defines attributes to width of window
 */
#define MaxLineLen 256

#define MAXSPARAM 256 /* Maximum length of string-parameters like 'Answerback message' */

#define DTCHAR_ASCII	     0 /* 0x20..0x7f charset-independent */
#define DTCHAR_ISO88591      1 /* 0x00..0xff (mainly 0x00..0x9f) ISO-8859-1 */
#define DTCHAR_RAW_BINARY    2 /* 0x00..0xff binary data, don't convert even if UTF8 (e.g.: 6-byte mouse sequence) */
#define DTCHAR_OEM	   255 /* 0x00..0x1f, 0x80..0xff OEM charset, eg grafical-characters */
#define DTCHAR_ANSI	   254 /* 0x00..0xff ANSI charset */
#define DTCHAR_XTERM_GRAPH 253 /* 0x20..0x7f interpreted as xterm-graphics 'l'==ULCORNER==OEM'DA'==U+250C */
#define DTCHAR_UNICODE	   252 /* 0x00..     unicode  == UTF32   */
#define DTCHAR_UTF32	   252 /* 0x00..     UTF-32LE == unicode */
#define DTCHAR_UTF16	   251 /* 0x00..     unicode encoded as UTF-16LE */
#define DTCHAR_UTF8	   250 /* 0x00..     unicode encoded as UTF-8 (not 'CESU-8', 'WTF-8' or 'modified UTF-8') */

typedef struct DtChar DtChar;

typedef struct Line Line;
struct Line {
    int len;			/* length of the line */
    int wrapped;		/* suppress end of line in Paste */
    DtChar *text;		/* 20160921: array => pointer */
    Attrib attr[MaxLineLen];	/* attributes of each character */
    Attrib end_attr;		/* attributes to width of window */
};

#define LINE_WRAP_NEXTLINE 1	/* this line continues in the next line */
#define LINE_WRAP_PREVLINE 2	/* this line is the countinuation of the previous line */

typedef struct LineBuffer {
    Line **list;		/* all lines in buffer */
    int  allocated;		/* 'list' available from 0..allocated */
    int  used;			/* number of lines in buffer */
} LineBuffer;

/* alloc 'list', fill with 'NULL' */
void linesAllocList (LineBuffer *l, int newalloc);
void linesFreeList (LineBuffer *l);

/* initialize a single line */
void linesInit (Line *l);

/* allocate and initialize a single line */
Line* linesAllocInit (Line **into);

/* Return the specified line */
Line* linesGetLine(BufferLineNo num);
/* linesNewLines:
 * Add zero or more new _initialized_ line(s) to the bottom of the line array.
 * It returns (if nDeletedLines!=NULL) the number of deleted lines,
 * so the caller be able to decrement 'term.topVisibleLine' and 'term.select.range'
 */
void linesNewLines (BufferLineNo nNewLines, BufferLineNo *pnDeletedLines);

/* Make sure that lines exist up to the specified line array index. */
void linesCreateTo(BufferLineNo idx);
/* Return the line array index of the specified terminal line. */
#define linesTerminalToLine(posy) linenoTerminalToBuffer(posy)
/* Insert a number of lines at the specified terminal line. */
void linesInsert(int posy, int bottom, int numLines);

/* Insert a number of blank characters in the specified terminal line. */
void linesCharsInsert (TerminalLineNo posy, Column posx, NChars numChars);

/* Delete a number of lines at the specified terminal line. */
void linesDelete(int posy, int bottom, int numLines);

/* Delete a number of characters in the specified terminal line. */
void linesCharsDelete (TerminalLineNo posy, Column posx, NChars numChars);

/* Clear all of the text in the terminal between two locations. */
void linesClearRange(TerminalLineNo y1, Column x1, TerminalLineNo y2, Column x2);

/* For now: single-byte only */
void linesSetChar (int dtCharType, uint32_t c);
/* Fill the screen with 'E' for alignment test */
void linesAlignTest(void);

Attrib linesEndAttr (Attrib from);
/* we don't store ATTR_FLAG_UNDERLINE in end_attr 
   (other attributes aren't supported yet)
 */

/* before saving lines to History we might want to know the number
   of usable lines 
   if it is too few, oldest lines will be lost;
   but before releasing them we should scroll
   the visible window off from that region */
LineNo linesAvailable (void);

/* then we can release the oldest lines
   to make place for the new ones */
void linesDeleteOldestLines (LineNo nDelFromHistory);
void linesDeleteBottomLines (LineNo nDel);

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
   2. Make sure that before calling, allocated >= used+ncnt && 0<=npos<=used
   3. Immediately after this function, you have to deal with the NULL values
      in 'list'
   4. The caller has to adjust term.topVisibleLine
   5. Also adjust or cancel the selection, if neccessary
 */
void linesInsertArea (LineBuffer *buff, BufferLineNo npos, BufferLineNo ncnt);

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
	BufferLineNo nto, BufferLineNo nfrom, BufferLineNo ncnt);

/* 20160926 */
/* DtChar is opaque, so caller (most likely) will convert the return value
   into 'void *' or 'char *' */
DtChar *linesCharPos (const Line *line, Column col /* zero-based */);

/* special function for 'select.c' */
BOOL linesIsSpaceAtIndex (const Line *line, Column col);

/* secure bytes at the end of line
   return pointer to the line end */
DtChar *linesSecureBytes (Line *line, int n);

/* linesGetChar: get some characters either single-bytes or utf16le (wchar_t)
 *
 * one or more calls of this function might form a 'session';
 *
 * before the very first call set into->len to 0
 *
 * after the very last call
 * *nChar=number of elements (char or wchar_t), *cf=DTCHAR_xxx
 */

#define LGC_MODE_CSMASK 3 /* NARROR, NARROW_NOCONW, UTF16 */
#define LGC_MODE_NARROW 0
/* return single bytes; either 'ANSI' or 'OEM'	*/
/* conversion might occur between ANSI and OEM, */
/* resulting character-loss			*/

#define LGC_MODE_NARROW_NOCONV 1
/* return single bytes that are either ANSI or OEM, */
/* according to 'cs' */
/* if CS==ASCII, then it will be determined by the first */
/* non-ASCII (ANSI or OEM) character */

#define LGC_MODE_UTF16  2		/* return UTF16 (wchar_t in Windows) */
/* intobuff: wchat_t * == uint16_t *
 */
#define LGC_ALLOC	4		/* into is a 'DynBuffer'; call BuffSecure if need be */

struct Buffer;

void linesGetChars (int mode, const Line *ln, Column frompos, NColumns fromlen,
		    struct Buffer *into, int *cs);

void linesGetCharsExt (int mode, const Line *ln,
	const ColumnInterval *ciFrom, ColumnInterval *ciDone, ColumnInterval *ciRest,
	struct Buffer *into, int *cs);

/* check if the current line is the continuation of the previous one
   (if there is a previous line), 
   if so return the previous line, otherwise NULL
 */
Line *linesGetPossiblePrevLine (BufferLineNo blnAct);

/* check if the next line is the continuation of the current one
   (if there is a next line), 
   if so return the previous line, otherwise NULL
 */

Line *linesGetPossibleNextLine (BufferLineNo blnAct);

#endif
