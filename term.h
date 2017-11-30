/* term.h
 * Copyright (c) 1997 David Cole
 *
 * Control and manage the terminal window
 */
#ifndef __term_h
#define __term_h

#include "utils.h"
#include "lines.h"
#include "termkey.h"
#include "termwin.h"
#include "select.h"
#include "font.h"

/* Constants for windows size limits - Khader Alatrash */
	#define TERM_MIN_X	10
	#define TERM_MAX_X	222

	#define TERM_MIN_Y	2
	#define TERM_MAX_Y	222

	#ifndef MAX_TERM_LINES	/* for debug you might want to predefine */
	#define MAX_TERM_LINES	2000	// must be >= TERM_MAX_Y
	#endif

    /* Why 222 and not 255? Because mouse events are encoded in x+20h,y+20h form
     * and coordinates are 1-based. So 223+32+1 would overflow a byte (256).
     * Besides, the one who wants 200+ columns/rows probably have an extremely big
     * monitor screen and using tiny fonts. (Superior vision?)
     * 2001/01/11 -- Mark Melvin
     */

/* Implement a state machine to decode the terminal escape sequences
 */
typedef enum {
    consNormal,
    consEscape,
    consHash,
    consOpenSquare,	/* ESC [	-- or CSI (0x9B) */
    consCloseSquare,	/* ESC ]	-- or OSC (0x9D) */
    consOpenParen,
    consCloseParen,
    consDCS,
    consDCS1,
    consInStringParam,
    consInStringEnd
} ConsoleState;

typedef struct InputParsingState {
    ConsoleState state;
    BOOL seenQuestion;		/* seen '?' in escape sequence? (as in: ESC[?...   */
    unsigned char chSeq;	/* 0x00 - 0x1F (eg ESC=0x1b)
				   0x80 - 0x9F (eg CSI=0x9b)
				   Note: ESC P is stored as CSI */
    int nImd;			/* number of intermediate' characters		   */
    unsigned char chFirstImd;	/* first 'intermediate' character after ESC, CSI... */
				/* (eg ' ', '?' or '>')	   			   */
				/* not stored if numeric, or not between 0x00-0x7f */
				/* default: zero				   */
    unsigned char chLastImd;	/* last 'intermediate' character, but only if nImd>=2 */
				/* default: zero; example: CSI ?  num  $      p	      */
				/*			       ^first  ^last  ^final  */
    unsigned char chFinal;	/* the terminating character;
				   in can be 0x30-x7f (escape-seq)
				   or 0x40-0x7f (control-seq)
				   in single char sequences (eg BS=0x08, NEL=0x85)
				   it is zero, even if it was two character, eg ESC + P
				 */
    int numParams;		/* number of parameters defined */
    int params[20];		/* escape sequence parameters */
    char sparam[MAXSPARAM];	/* string param */
    int  numSParam;             /* bytes collected in 'sparam' */
    int  stateSParam;		/* 0/1/2 = waiting: normal bytes/hex first half/hex sencond half */
    char dcschar;		/* when in a DCS-sequence, the controlling code */
} InputParsingState;

/* Cursor styles
 */ 
typedef enum {
    cursorBlock,
    cursorUnderLine,
    cursorVertLine
} CursorStyle;

typedef struct TermCharSet {
    int flags;			/* TCSF_*** */
    unsigned char *charset;	/* later, this may be removed from here */
} TermCharSet;

#define TCSF_GRAPH_MASK 1
#define TCSF_GRAPH_NO   0
#define TCSF_GRAPH_YES  1

int setTermCharSet (TermCharSet *to, int n); /* TCSN_*** */

	/* name	  value	   G0-sequence G1-sequence	comment */
#define TCSN_UK	      0	/* ESC ( A	ESC ) A		== US   */
#define TCSN_US	      1	/* ESC ( B	ESC ) B		normal mode */
#define TCSN_GRAPHICS 2	/* ESC ( 0	ESC ) 0		graphics mode */
#define TCSN_ALT      3	/* ESC ( 1	ESC ) 1		== US */
#define TCSN_ALTGRAPH 4	/* ESC ( 2	ESC ) 2		== GRAPHICS */

typedef struct CursorData {
    TerminalPoint pos;
    BOOL fLineFull;		/* the cursor is at the right margin,
				   but it should be after that,
				   so before the next character,
				   we have to go to the next line,
				   or scroll, or something */
} CursorData;

typedef struct SavedCursorData {/* data affected by DECSC (ESC 7) and DECRC (ESC 8) */
    TerminalPoint pos;
    BOOL fLineFull;		/* the emulators I've checked don't save this flag... but we do;) */
    AttrData attr;		/* saved attributes */
    int charSet;		/* saved character set */
    TermCharSet G0;		/* saved G0 character set */
    TermCharSet G1;		/* saved G1 character set */
} SavedCursorData;

typedef struct ScrollData {
    TerminalInterval lines;
    BOOL fFullScreen;		/* TRUE: the full window is the scrolling region */
    BOOL fRestrict;		/* TRUE: the cursor is locked into the scrolling region,*/
				/* the server sends scroll-region-relative coordinates */
				/*       (DECOM = ESC [ ? 6 h)			     */
} ScrollData;

typedef struct TermMouseRep {
    int  mouseReporting;	/* TERM_MOUSE_REP_* default: 0=no */
    int  mouseCoord;		/* TERM_MOUSE_COORD_* default: 0=old_comatible */
    BOOL fMoveReported;		/* the last report was mouse move */
    TerminalPoint ptLastReported; /* last reported position */
} TermMouseRep;

typedef struct AltBuffData {
    LineBuffer linebuff;
} AltBuffData;

void termSetMouseRep   (int mode);  /* TERM_MOUSE_REP_* */
void termSetMouseCoord (int coord); /* TERM_MOUSE_COORD_* */

typedef struct Terminal {
    HFONT fonts[FONTS_USED];	/* terminal fonts: normal / OEM / underlined / underlined_OEM */
    BYTE  fontCharSet;          /* character set of terminal font */
    PixelSize charSize;		/* size of the character cell */
    CharSize winSize;		/* current terminal size (in characters) */
    PixelSize winInitPos;	/* initial window position (in pixels) */
    BOOL useInitPos;		/* override initial placement? */
    PixelRect cursorRect;	/* special usage, see in term.c: makeCaretBitmap and winCaretPos */

    HBITMAP caretBitmap;	/* the caret bitmap */

    LineBuffer linebuff;
    BufferLineNo topVisibleLine;/* top visible line on window (line index) */
				/* used when converting to/from WindowLineNo */
    BufferLineNo maxLines;	/* maximum number of lines to remember */

    SelectData select;		/* data of the selection */

    InputParsingState state;	/* the former state and seenQuestion variables */
    CursorData cursor;		/* current cursor position (terminal) */
    BOOL cursorVisible;		/* is cursor visible? */
    AttrDefaultData defattr;	/* before 20160506 it was part of 'AttrData' -- bad design */
    AttrData attr;		/* clear screen character attribute */
    BOOL lineWrap;		/* should we wrap lines */
    BOOL insertMode;		/* should we insert characters */
    BOOL newLineMode;		/* CR key returns '\r\n'(T) or '\n'(F) */
    BOOL ansiCursorKeys;	/* ANSI cursor key sequences? */
    BOOL autoRepeat;		/* should keys auto repeat? */
    BOOL flowStartAny;		/* does any key restart flow? */
    BOOL echoKeystrokes;	/* should we echo keystrokes? */
    unsigned char tabStops[MaxLineLen];	/* 1 == tabstop */
    ScrollData scroll;

    int charSet;		/* current character set 0/1 = G0/G1 */

    TermCharSet G0;		/* current G0 character set */
    TermCharSet G1;		/* current G1 character set */

    SavedCursorData saved[2];	/* #0 is used when altbuff==0, #1, otherwise */

    TermMouseRep mouseRep;

    struct {
	char title[128+1];	/* application set title (always zero-terminated) */
	unsigned short len;	/* 0..sizeof title -1 (number of bytes) */
	short dtcc;		/* DTCHAR_xxx: singlebyte or UTF8; no UCS2/UTF16/UTF32 */
    } title;

    BOOL attachedPrinter;	/* enables xterm-style attached printer */
    BOOL enablePrintScreen;	/* enables screen dump operation */

    unsigned char keypadMode[2];/* #0: top 4 four keys: NumLock '/' '*' '-' */ 
				/* #1: the other keys of numeric block */

    uint32_t miscFlags;		/* other boolean settings; their default value is zero;
				   so reset will do set miscFlags to zero */

/* 2010.12.17.LZS: Alternate buffer:
   when changing between buffs (ESC[?47h/ESC[?47h),
   restore from here / save to here
 */
    int altbuff;		/* 0/1 = normal(default,ESC[?47l)/alt(ESC[?47h]) */
    AltBuffData alt;
} Terminal;

/* miscFlags */
#define TERMFLAG_8BIT		1 /* send 8-bit control-characters (default=7bit) */
#define TERMFLAG_NCSM		2 /* No Clear on Column Change (DECNCSM) -- sets: CSI ? 95 l/h; uses: CSI ? 3 l/h */

#define KPM_PRESET_FLAG	     0x04 /* if set:   application/numeric mode is preset (=forced) according to the next bit */
				  /* if clear: application/numeric mode can be set by the remote side */
#define KPM_PRESET_APPLI     0x02 /* 0/1: numeric/application mode is preset */
#define KPM_SET_APPLI	     0x01 /* 0/1: numeric/application mode set by the remote site */

#define KPM_TOP4KEYS		0
#define KPM_OTHERKEYS		1

/* termSetKeyPadMode:

    sets bit#01 according to fset;
    returns the actual status (that depends on the other bits, too!)
    and sets *fStatusChange to 1, if the status-line should be changed
 */
void termSetKeyPadMode (unsigned char into[2], int fAppl, int *fStatusChange);

int termKeyApplicationMode(const unsigned char from[2], int index);
#define KPM_CHK_FUNCKEY(termp)  termKeyApplicationMode ((termp)->keypadMode, 0)
#define KPM_CHK_OTHERKEY(termp) termKeyApplicationMode ((termp)->keypadMode, 1)

int termKeypadModeStr(const unsigned char from[2], char reply[3]);
/* one or two characters, 
    from==NULL means the actual 'term'

   return:
    "A" = every key is preset to 'application'
    "n" = set to 'numeric'
    "Nn" = top four keys are preset to 'numeric', the rest are set to 'numeric'
 */
int termKeypadModeStatus (const unsigned char from[2], char reply[]);
/* similar to the previous, but longer: "Appl" or "Num" or "num/Appl" */

#define TERM_MOUSE_REP_OFF		0 /* report nothing */
#define TERM_MOUSE_REP_X10		1 /* ESC[9h mode    */
#define TERM_MOUSE_REP_X11		2 /* ESC[1000h mode */
#define TERM_MOUSE_REP_X11_HIGHLIGHT	3 /* ESC[1001h mode -- we don't actually do this! */
			    		  /* treat is as it were the previous one      */
#define TERM_MOUSE_REP_X11_BTN_EVENT	4 /* ESC[1002h mode */
#define TERM_MOUSE_REP_X11_ANY_EVENT	5 /* ESC[1003h mode -- we don't actually do this! */
			    		  /* treat is as it were the previous one      */

#define TERM_MOUSE_COORD_STD		0 /* default:   6-byte binary seq    ESC [ M Cb Cx Cy	range: 1..223 */
#define TERM_MOUSE_COORD_UTF8		1 /* CSI ? 1005 6+ -byte binary seq  ESC [ M Cb Cx(Cx) Cy(Cy) range: 1..2015 */
#define TERM_MOUSE_COORD_SGR		2 /* CSI ? 1006 text-seq             ESC [ < text-Cb ; text-Cx ; text-Cy {m|M} */
#define TERM_MOUSE_COORD_URXVT		3 /* CSI ? 1015 text-seq	     ESC [ text-Cb ; text-Cx ; text-Cy M */
/* Some of these uses 32+ offset in Cb and/or Cx/Cy:

    STD:   Cb +32 Cx/Cy +32 (x/y range: 1..223  -> 33..255  -> 21..FF)
    UTF8:  Cb +32 Cx/Cy +32 (x/y range: 1..2015 -> 33..2047 -> 21..7F,C280..DFBF)
    SGR:   Cb  +0 Cx/Cy  +0 (X/y range: no explicit limit)
    URXVT: Cb +32 Cx/Cy  +0 (X/y range: no explicit limit)
 */

extern Terminal term;		/* the one and only terminal */

void termGetProfile(void);
/* get only [Terminal].Attrib=color/color-on-color/color */
void termGetDefAttr(void);

/* Copy the selection onto the clipboard */
void termSelectCopy(void);
/* Paste the current clipboard contents into the terminal */
void termSelectPaste(void);
/* Return whether or not there currently is a selection */
BOOL termHaveSelection(void);
/* Return whether or not the user is currently performing a selection */
BOOL termSelectInProgress(void);
/* Return the state of auto-copy on selection */
BOOL termAutoCopy(void);
/* Toggle the state of auto-copy on selection */
void termToggleAutoCopy(void);
/* Return the state of mouse event reporting */
BOOL termMouseReporting(void);
/* Toggle the state of mouse event reporting */
void termToggleMouseReporting(void);

void termCancelSelect (int mode);
/* cancel the selection, depending on mode
   mode==0: unconditionally
   mode==1: only if overlaps with the visible part
   mode==2: cancel only the visible part (unimplemented yet)
   mode==other: (reserved)
 */

/* Return the state of xterm-style attached printer */
BOOL termAttachedPrinter(void);
/* Toggle the state of xterm-style attached printer */
void termToggleAttachedPrinter(void);
/* Return the state of printscreen */
BOOL termPrintScreenEnabled(void);
/* Toggle the state of printscreen */
void termTogglePrintScreen(void);
/* Return the current cursor style */
CursorStyle termCursorStyle(void);
/* Set the cursor style */
void termSetCursorStyle(CursorStyle style);

/* Return handle of the terminal window */
HWND termGetWnd(void);
/* Set the font used to draw the terminal */
void termSetFont(void);

/* termCheckSizeLimits:
 * Checks if windows size is according to defined limits - Khader Alatrash
 * returns TRUE, if it has changed any field of 'size'
 */
BOOL termCheckSizeLimits(CharSize *size);

/* Set the geometry of the terminal window (in characters) */
void termSetGeometry(char* str, OptSource source);
/* Set the attribute to be used for blank characters */
void termSetBlankAttr(const AttrDefaultData *attr, OptSource source);
/* Get the attribute to be used for blank characters */
/* Attrib termGetBlankAttr(void); -- unused */
/* Set the application window title */
void termSetTitle(const char* title, int dtcc);
/* Return the application window title */
char* termGetTitle(unsigned *len, int *dtcc);
/* Return whether or not there is a application window title set */
BOOL termHasTitle(void);

/* Scroll the terminal to the bottom of the history */
void termScrollToBottom(void);
/* Toggle whether or not terminal output forces a scroll to bottom */
void termToggleBottomOnOutput(void);
/* Return whether or not terminal output forces a scroll to bottom */
BOOL termBottomOnOutput(void);

/* WM_KEYDOWN: User pressed a key - auto-repeat detection,
   returns TRUE, if the next WM_CHAR should be ignored */
BOOL termKeyDown(TermKey *tk, const ConstBuffData *pmap);

/* WM_SYSKEYDOWN: User pressed a system key (Alt+Key)
   returns TRUE, if the next WM_CHAR should be ignored */
BOOL termSysKeyDown(TermKey *tk, const ConstBuffData *pmap);

/* WM_CHAR: User pressed a normal character key */
void termWmChar(WPARAM key, LPARAM lparam, int ccode);

/* status of NumLock an CapsLock for status line */
void termKeyLockStatus (char *buff);

/* Hide the caret in the terminal window */
void termHideCaret(void);
/* Show the caret in the terminal window */
void termShowCaret(void);
/* Set the caret visibility in the terminal window */
void termSetCursorMode(BOOL visible);
/* Application has just gained focus */
void termSetFocus(void);
/* Application has just lost focus */
void termKillFocus(void);

/* The application window has been resized - set the terminal window size */
void termSetWindowSize(int cx, int cy);
/* Windows is going to change application window size. */
void termWindowPosChanging(SIZE* winSize);
/* Format the terminal window geometry */
void termFormatSize(char* str);
/* The same for the status windows: reserve order (rows*columns), to be 'stty size'  and 'resize' compatible 
   return string-length */
int termFormatSizeStatus(char* str);

/* Create the terminal window */
BOOL termCreateWnd(HINSTANCE instance, HWND parent);
/* Register the terminal window class */
BOOL termInitClass(HINSTANCE instance);

/* termGetChar: fetch characters from a DtChar array
 * Before the very first call set
 * *nChar=0, *cf= DTC_ASCII/DTC_ANSI/DTC_OEM 
 * After the very last call
 * *nChar=number of characters, *cf=DTC_ASCII/DTC_ANSI/DTC_OEM
 */
void termGetChars (char *text, const DtChar *ch, int len, int *nChar, int *cs);

void termSetAltBuff (int sel /* 0/1 = std/alt */);
int  termGetAltBuff (void)   /* 0/1 = std/alt */;
void termClearAltBuff (void);

/*  Scroll the terminal window vertically
 *
 * Args:
 * code  - SB_****: the type of scroll performed
 * param - depends on 'code': amount of lines / amount of pages / position
 */
void termVerticalScroll (UINT code, UINT param);

/* termWheelAction -- handle WM_MOUSEWHEEL messages
   also processes messages WM_CREATE and WM_SETTOMGCHANGE
 */
void termWheelAction (UINT msg, WPARAM wparam, LPARAM lparam);


/* termMouseAction -- handle WM_***mouse*** messages
   also processes messages WM_CREATE and WM_SETTOMGCHANGE
 */

LRESULT termMouseAction (UINT message, WPARAM wparam, LPARAM lparam, const MSG *msg);

typedef struct TermPosition {
    struct {		/* the pixel coordinates of the point: */
	PixelPoint p;	/* - within the window;		eg: y=81, x=75 */
	PixelPoint mod;	/* - within the character cell: eg: y= 1, x= 5 */
    } pixel;

    struct {		/* window coordinates: */
	WindowPoint p;	/* - unclipped (overflow and underflow possible);
							eg: y= 8; x= 7  */
	WindowPoint clip;     /* - clipped between 0 and winSize.cx/cy	*/
	WindowPoint stclip;   /* - clipped between 0 and winSize.cx-1/cy-1	*/
	int   out;	/* col<0 || col>=term.winSize.cx ||
		     	   row<0 || row>=term.winSize.cy see MP_OUT_*** */
    } win;

/* the following fields are valid only if 0<=row<term.winSize.cx */
    struct {		/* logical coordinates */
			/* 	logical.column = window.column		*/
			/* 	logical.row    = window.row + term.topVisible */
	BufferPoint p;	/* - unclipped (overflow and underflow possible);
							eg: y= 1008; x= 7  */
	BufferPoint sel;	/* select-friendly variant: */
			/*    when over the Line-len (or no Line-data at all), */
			/*    x= term.winSize.cx  */
	Line *line;	/* the Line-data, or NULL */
	int linelen;	/* linelen, or 0 */
	BOOL out;	/* p.y < 0 || p.y >= term.linebuff.used ||
			   p.x < 0 || p.x log_line_len see MP_OUT_*** */
    } log;
} TermPosition;

			/* physical		  logical			  */
			/* -------		  -------			  */	
#define MP_OUT_LEFT   1 /* col<0 		  col<0				  */
#define MP_OUT_RIGHT  2 /* col>=term.winSize.cx   col>=log_linelen		  */
#define MP_OUT_X      3
#define MP_OUT_TOP    4 /* row<0		  row<0				  */
#define MP_OUT_BOTTOM 8 /* row>=term.winSize.cy	  log_lineno>=term.linebuff.used  */
#define MP_OUT_Y     12

void termFillPos (int xpos, int ypos, TermPosition *into);

/* is this line the last of the scrolling region? */
BOOL termIsScrollBottom (TerminalLineNo tline);

/* is this line the last of the screen? */
BOOL termIsScreenBottom (TerminalLineNo tline);

/* is this line in the scrolling region? */
BOOL termIsInScrollRegion (TerminalLineNo tline);

/* conversion between pixel-size and character-size:
   (if charSize==NULL, use term.winSize)
 */
void termCalculatePixelSize (const CharSize *charSize, PixelSize *pixelSize, BOOL fWithScrollbar);
void termCalculateCharSize  (const PixelSize *pixelSize, CharSize *charSize, BOOL fWithScrollbar);

/* The 'Paste' button become configurable: middle/right:
 1/2 = middle/right */
int termGetPasteButton (void);
int termSetPasteButton (int n);

/* Invalidate an area specified by character coordinates (window-based coordinates)
 */
void winInvalidateRect (const WindowRect *rect);

/* Invalidate the whole window */
void winInvalidateAll (void);

/* Invalidate an area specified by (row, col) coordinates
 *
 * Args:
 * (y1,x1) -- upper left corner (included)
 * (y2,x2) -- lower right corner (not included)
 */
#define winInvalidate(y1,x1,y2,x2) \
{ \
    WindowRect r;		\
    r.left= x1;			\
    r.top= y1;			\
    r.right= x2;		\
    r.bottom= y2;		\
    winInvalidateRect (&r);	\
}

/* save [Terminal] settings */
void termSaveProfile (void);

/* save only [Terminal].Attrib=color/color-on-color/color */
/* parameter can be NULL -- meaning 'term.defattr' */
void termSaveDefAttr (const AttrDefaultData *from);

#endif
