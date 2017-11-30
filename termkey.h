/* termkey.h */

/* separated from term.c */

#ifndef TERMKEY_H
#define TERMKEY_H

#include <windows.h>
#include <stdio.h>

#include "buffdata.h"

#ifndef VK_LSHIFT
#define VK_LSHIFT         0xA0
#define VK_RSHIFT         0xA1
#define VK_LCONTROL       0xA2
#define VK_RCONTROL       0xA3
#define VK_LMENU          0xA4
#define VK_RMENU          0xA5
#endif

#ifndef VK_LWIN
#define VK_LWIN           0x5B
#define VK_RWIN           0x5C
#define VK_APPS           0x5D
#endif

/* a classification of keys (experimental) */
#define TKC_F_14	  1	/* function key F1..F4 */
#define TKC_F_OTHER	  2	/* function key F5..F24 */
#define TKC_NUMPAD_NUM   10	/* numpad keys that are affected by NumLock: 0123456789. */
#define TKC_NUMPAD_OTHER 11	/* other numpad keys: NumLock / * - + Enter */
#define TKC_OTHER	  0	/* everything else */

typedef struct TermKey {
/* fields filled by termFillTermKey */
    unsigned short keycode;	/* wparam&0xff		-- eg VK_ESC=0x1b, VK_RETURN=0x0d, VK_SHIFT=0x10 */
    unsigned short scancode;	/* (lparam>>16)&0xff	-- eg ESC=1, Enter=28, LeftShift=42, RightShift=54 */
    unsigned short flags;	/* (lparam>>16)&0xff00	-- eg KF_EXTENDED=0x100, KF_UP=0x8000, KF_ALTDOWN=0x2000 */
    unsigned short modifiers;	/* GetKeyboardState -> VK_SHIFT/VK_CONTROL/VK_MENU bit 0x80 */
    unsigned short locks;	/* GetKeyboardState -> VK_CAPITAL/VK_NUMLOCK/VK_SCROLL bit 0x01 */
    unsigned short classe;	/* TKC_*** */
    unsigned char keyvect [256];	/* GetKeyboardState output */

/* fields filled by termToAscii */
    unsigned short nascii;	/* ToAscii output number 0..2*/
    unsigned short ascii[2];	/* ToAscii output */
} TermKey;

#define TMOD_ALT         0x0001
#define TMOD_CONTROL     0x0002
#define TMOD_SHIFT       0x0004
#define TMOD_WIN         0x0008
#define TMOD_FIRST4      0x000f /* ALT|CONTROL|SHIFT|WIN */

#define TMOD_LALT        0x0010
#define TMOD_LCONTROL    0x0020
#define TMOD_LSHIFT      0x0040
#define TMOD_LWIN        0x0080

#define TMOD_RALT        0x0100
#define TMOD_RCONTROL    0x0200
#define TMOD_RSHIFT      0x0400
#define TMOD_RWIN        0x0800

#define TMOD_FIRST12     0x0fff /* all of the above */

#define LOCK_CAPS   1
#define LOCK_NUM    2
#define LOCK_SCROLL 4

/* fill struct TermKey from WM_KEYDOWN.wparam, .lparam and GetKeyboardState */
void termFillTermKey (WPARAM wparam, LPARAM lparam, TermKey *tk);

/* fill fields 'modifiers' and 'lock'
   generally you don't call this from outside
 */
void termFillModLock (TermKey *tk);

/* fill 'class' field of TermKey -- called by termFillTermKey */
unsigned short termKeyClass (TermKey *tk);

/* fill the last fields of TermKey using ToAscii */
void termToAscii (TermKey *tk);

/* in some contexts different keys generate the same keycode
   or a single key generates different keycodes.

   Example:
   NumLock=off Key=Up:         keycode= 38(VK_UP)      extended=yes
   NumLock=off Key=num8:       keycode= 38(VK_UP)      extended=no
   NumLock=on  Key=Up:         keycode= 38(VK_UP)      extended=yes
   NumLock=on  Key=num8:       keycode=104(VK_NUMPAD8) extended=no
   NumLock=on, Key=Shift+num8: keycode= 38(VK_UP)      extended=no

   What's more, when NumLock=on and you press Shift+num8,
   your Windows automagically 'releases' Shift,
   generates WM_KEYDOWN and WM_KEYUP
   then 'presses' Shift... so we have to undo this feature

   So what we do:
   1. do nothing for actual extended keys (ie when bit KF_EXTENDED is set:
      arrows, Ins, Del, Home, End, PgUp, PgDn)
   2. logically press shift if keycode suggest extended key (eg VK_UP) and NumLock=on
   3. change extended keycode (VK_UP) to numeric keycode (VK_NUMPAD8)

   Change: this function is now called automagically by termFillTermKey
 */
void termNumpadMagic (TermKey *tk);

void termKeyHackEmulate (TermKey *tk, unsigned short keycode, int action);
/* change 'tk' as if key represented by 'keycode' were pressed/released/both
   if 'fmodifier' is TRUE then bit_0x01 is set as well, otherwise only bit_0x80
   Part of the 'NumLock should act as PF1' problem
   calls 'termFillModLock'
 */
#define TKHACK_DOWN 1
#define TKHACK_UP   2
#define TKHACK_BOTH 3

typedef struct TermKeyMapBits {
unsigned short modifiers;
unsigned short locks;
unsigned short flags;
} TermKeyMapBits; 

#define EmptyTermKeyMapBits {0, 0, 0}

typedef struct TermKeyMapCond {
    TermKeyMapBits mask;	/* if ((actual&mask)==test) remapping; */
    TermKeyMapBits test;	
} TermKeyMapCond;

#define EmptyTermKeyMapCond {EmptyTermKeyMapBits, EmptyTermKeyMapBits}

#define tkmpcShiftOnly {{TMOD_FIRST4, 0, 0}, {TMOD_SHIFT, 0, 0}}
#define tkmpcCtrlOnly  {{TMOD_FIRST4, 0, 0}, {TMOD_CONTROL, 0, 0}}
#define tkmpcAltOnly   {{TMOD_FIRST4, 0, 0}, {TMOD_ALT, 0, 0}}
#define tkmpcWinOnly   {{TMOD_FIRST4, 0, 0}, {TMOD_WIN, 0, 0}}

#define tkmpcLeftAltOnly  {{TMOD_FIRST12, 0, 0}, {TMOD_ALT | TMOD_LALT, 0, 0}}

#define TermKeyMapCondMatch(tkp,cp) \
	(((tkp)->modifiers&(cp)->mask.modifiers) == ((cp)->mask.modifiers&(cp)->test.modifiers) && \
	 ((tkp)->locks    &(cp)->mask.locks)     == ((cp)->mask.locks    &(cp)->test.locks)	&& \
	 ((tkp)->flags    &(cp)->mask.flags)     == ((cp)->mask.flags    &(cp)->test.flags))

#define TermKeyMapCondEqual(cp,dp) \
	((cp)->mask.modifiers == (dp)->mask.modifiers && \
	 (cp)->mask.locks     == (dp)->mask.locks     && \
	 (cp)->mask.flags     == (dp)->mask.flags     && \
	 ((cp)->mask.modifiers&(cp)->test.modifiers) == ((dp)->mask.modifiers&(dp)->test.modifiers) && \
	 ((cp)->mask.locks    &(cp)->test.locks)     == ((dp)->mask.locks    &(dp)->test.locks) && \
	 ((cp)->mask.flags    &(cp)->test.flags)     == ((dp)->mask.flags    &(dp)->test.flags))

typedef struct TermKeyMapElement {
    TermKeyMapCond cond;	/* if ((actual&mask)==test) remapping; */
    BuffData mapto;		/* 'mapto' and 'lmapto' shouldn't be both filled: */
    long lmapto;		/* mapto="string" and lmapto=0; or mapto="" and lmapto=<nonzero> */
} TermKeyMapElement;

typedef struct TermKeyMapEntry {
    unsigned short keycode;		/* The key we want to map to something else */
    TermKeyMapElement *elements;	/* The TermKeyMapElements referring to this key */
    unsigned short nelement;
    unsigned short nmax;
} TermKeyMapEntry;

typedef struct TermKeyMap {
    TermKeyMapEntry *entries;		/* either NULL or pointer to TERMKEY_MAXVK elements */
} TermKeyMap;

#define EmptyTermKeyMap { NULL}

/* tkmAddKeyMap:
	first, it deletes the old mapping for the key-combination,
	then stores the new one, but only if mapto!=NULL or lmpapto!=0
 */

int tkmAddKeyMap (TermKeyMap *map,
		  unsigned short keycode, const TermKeyMapCond *cond,
		  const ConstBuffData *mapto, long lmapto);

int tkmAddKeyMapS (TermKeyMap *map,
		   unsigned short keycode, const TermKeyMapCond *cond,
		   const char *mapto, long lmapto);

#define TERMKEY_MAXVK 256 /* VK_values 0..255 so far */

int tkmGetKeyMap (const TermKeyMap *map,
		  const TermKey *tk,
		  ConstBuffData *output,
		  long *lmapto);

void tkmReleaseMap (TermKeyMap *map);

void tkmDumpMap (FILE *to, const TermKeyMap *map);

#define TKM_DEFAULT_MAP NULL

typedef struct TermKeyMapStringNum {
    char *str;
    long num;
} TermKeyMapStringNum;

typedef struct TermKeyMapStringNumS {
    int fsorted;				/* set to zero */
    TermKeyMapStringNum *p;
    size_t n;
} TermKeyMapStringNumS;

int tkmLoadFromIniFile (TermKeyMap *into, const char *filename, const char *sectionname,
	TermKeyMapStringNumS *strs);

#define TKM_DEFAULT_SECTION NULL

#endif

