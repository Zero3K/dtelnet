/* emul.h
 * Copyright (c) 1997 David Cole
 *
 * Terminal emulation
 */
#ifndef __emul_h
#define __emul_h

#include "ctab.h"
#include "buffdata.h"
#include "lines.h"
#include "termkey.h"

/* Define key sequences for a terminal emulation
 */
#define MAX_F_KEY	   26  /* some keyboard have 16 F-keys, and Shift adds 10 */

#define ETF_ESC_PARO		3   /* how to handle "\E(" sequences */
#define ETF_ESC_PARO_LINUX	1     /* like LINUX */
#define ETF_ESC_PARO_VT320	2     /* like VT320+ */

#define ETF_MOUSE_EVENT		4  /*  does the terminal support mouse ev */

#define ETF_SHCTRL_MASK	     0x38  /* what to do when Shift+Up or Ctrl+PgDn is pressed */
#define ETF_SHCTRL_RXVT	     0x08     /* do what rxvt does eg: ESC[a ESCOa ESC[3$ ESC[11^ */
#define ETF_SHCTRL_XTERM_NEW 0x10     /* do what xterm-new does eg: ESC[1;2A ESC[1;5P */
#define ETF_SHCTRL_KONSOLE   0x18     /* do what konsole does: *almost* xterm-new, except for F1..F4 */
				      /* eg Shift+F1=ESCO2P, Shift+Ctrl+F3=ESCO6R */
#define ETF_SHCTRL_SHIFT_F   0x10     /* if clear: Shift adds 10 to the Fkey-number: F3->F13 (rxvt) */
				      /* if set:   Shift modifies the escape-sequence of the Fkey (xterm, konsole) */

#define TERM_NAME_LEN  20
#define KEYCODE_LEN  8

typedef char KEYCODE[KEYCODE_LEN];

typedef struct Emul {
    char  name[TERM_NAME_LEN];  /* name of terminal type */
    char  servname[TERM_NAME_LEN]; /* what we tell the server (TERM= env-var) */
    int   flags;                /* ETF_... */

    KEYCODE keyPageUp;		/* string to send for PageUp key */
    KEYCODE keyPageDown;
    KEYCODE keyHome;
    KEYCODE keyEnd;
    KEYCODE keyInsert;
    KEYCODE keyDelete;
    KEYCODE keyBacktab;
    KEYCODE keyBackSpace;          /* Standard BackSpace */
    KEYCODE keyAltBackSpace;       /* Alternate BackSpace */
    KEYCODE keyNum5;		   /* 20140115.LZS */

    int   nMaxFkey;                /* Should be less or equal to MAX_F_KEY */
    Buffer  keyFn[MAX_F_KEY];	   /* keyF[0]=F1.. keyF[19]=F20 */

    CodeTable ctGraphConv;         /* map graphics characters to vt100 std */
    CodeTable ctServerToOem;       /* map characters coming from server */
    CodeTable ctOemToServer;       /* map characters going to server */
    CodeTable ctServerToAnsi;      /* map characters coming from server */
    CodeTable ctAnsiToServer;      /* map ANSI codes to server charset */

    int fAnsi;                     /* 0/1/2+ = OEM (any variant) / ANSI (any variant) / UTF-8 */
} Emul;

void emulRelBuff (Emul *ptr);	   /* release buffers */

typedef struct {
    unsigned int num;
    char** names;
} EmulNames;

/* Functions to access terminal definitions */
void telnetEnumTermProfiles(EmulNames* emuls);
void telnetGetTermProfile(Emul* emul, const char* name);
void telnetSaveTermProfile(const Emul* emul, char* name);

/* Return pointer to terminal currently being emulated. */
Emul* emulGetTerm(void);
/* Set the terminal type to emulate by name */
void emulSetTerm(const char* name);

/* Set the from/to server charsets specified by name */
/* emul==NULL means currEmul */
int emulSetCharset (Emul *emul, const char *name);

/* Return current character attributes */
Attrib getAttrColors(void);	/* only the colors from term.currAttr.attr, nothing more (no underline,bold,blink,italic,inverse etc) */
Attrib getAttrToStore(void);	/* depends on term.currAttr.attr and term.currAttr.flags&ATTRF_NEGATIVE */

/* Process a stream of characters through terminal emulator */
void emulAddText  (const char *text, int len, BOOL bKeyboard);
void emulAddText2 (const void *text, int len, int dtcc, BOOL bKeyboard);

/* data received from server, dtcc is determined from currEmul.fAnsi */
void emulAddTextServer (const char *text, int len);

/* Handle a normal key press */
void emulKeyPress (uint32_t ch, int dtcc);

/* Handle a function key press -- different versions */
/* encoding: the string is either single-byte or utf-8 (see 'dtcc')
 */
void emulFuncKeyPress2 (int localaction, const char *str, int dtcc);
void emulFuncKeyPressB (int localaction, const ConstBuffData *b, int dtcc);

/* how to do local echoing */
#define EFK_LOCAL_SKIP      0 /* do nothing */
#define EFK_LOCAL_SAME      1 /* echo the same str */
#define EFK_LOCAL_BACKSPACE 2 /* delete charcter left to the cursor */
#define EFK_LOCAL_DELETE    3 /* delete charcter under the cursor */
#define EFK_LOCAL_INSERT    4 /* switch insert mode */
#define EFK_LOCAL_LEFT      5 /* cursor left */
#define EFK_LOCAL_RIGHT     6 /* cursor right */
#define EFK_LOCAL_HOME      7 /* cursor to begging of line */
#define EFK_LOCAL_END       8 /* cursor to end of line */
#define EFK_LOCAL_SEND      9 /* send in the current line */
#define EFK_LOCAL_UP       10 /* cursor up */
#define EFK_LOCAL_DOWN     11 /* cursor down */

/* Set terminal local echo mode on or off */
void emulSetEcho(BOOL echo);
/* Set terminal local flow control "resume with any character" on or off */
void emulLocalFlowAny(BOOL any);

/* Reset terminal to initial state
 * note: RIS=hard reset; DECSTR=soft reset
 * in the future, they might mean different things
 */
void emulResetTerminal (BOOL fHard);

/* Define AnswerBack message */
void emulSetAnswerBack (int len, const char *from);

/* Query/set emulExecuteFlag */
BOOL emulGetExecuteFlag (void);
BOOL emulSetExecuteFlag (BOOL newval);

/* Set term.keypadMode */
void emulSetKeypadMode (const unsigned char newval[2]);

/* character sets */
extern unsigned char *usCharSet;
extern unsigned char *graphicsCharSet;
extern unsigned char *ukCharSet;
extern unsigned char *altCharSet;
extern unsigned char *altGraphicsCharSet;

#define TMOD_ALT         0x0001
#define TMOD_CONTROL     0x0002
#define TMOD_SHIFT       0x0004
#define TMOD_WIN         0x0008

const char *emulGetKeySeq (const TermKey *tk, BOOL fAnsiCrsrMode);
/* currently supported keycodes (tk->keycode)
    VK_PRIOR  (keyPageUp)
    VK_NEXT   (keyPageDown)
    VK_HOME   (keyHome)
    VK_END    (keyEnd)
    VK_INSERT (keyInsert)
    VK_DELETE (keyDelete)
    VK_BACK   (keyBackSpace)
    VK_UP     (ESC [ A)
    VK_DOWN   (ESC [ B)
    VK_RIGHT  (ESC [ C)
    VK_LEFT   (ESC [ D)
 */

 /* execute <Shift>/<Ctrl> transformation on an ESC-sequence
  * eg: rxvt:  Ctrl+Up:  ESC[A -> ESCOa
  *     xterm: Shift+F1: ESCOP -> ESC[1;2P
  */
char *emulShCtrlMagic (const TermKey *tk, const char *from, char *to, int emulflags);

struct CursorData;

void emulCursorReset (struct CursorData *cp);

#endif

