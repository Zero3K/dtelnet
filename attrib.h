/* attrib.h */

#ifndef __attrib_h
#define __attrib_h

/* include these before 'attrib.h':
#include "preset.h"
#include <windows.h>
#include "platform.h"
*/

/* Define the colors in the order they appear in the palette
 * with component-bits:
 *  1 Red
 *  2 Green
 *  4 Blue
 *  8 Light
 */
enum AttrColor {
/* 0-15 (0x00-0x0f) */
    Black, 	 Red,		Green,		Yellow,
    Blue, 	 Magenta,	Cyan,		White,
    Light_Black, Light_Red,	Light_Green,	Light_Yellow,
    Light_Blue,  Light_Magenta, Light_Cyan,	Light_White
/* 16-231  (0x10-0xe7): 6*6*6 colours (RGB) */
/* 232-255 (0xe8-0xff): grey-scale of 24 steps */
};

typedef unsigned char  DtColor;
typedef unsigned short DtColorPair; /* see GET_FG, GET_BG, SET_FG, SET_BG */
typedef uint32_t Attrib;

#define INTENSE 0x08
#define GET_INTENSE(a) ((a) & INTENSE)

typedef unsigned short XDtColor; /* actually, it is 9 bits only: */
/* 0-0xff:  ordinal colors
     0x100: default foreground	  -- this can be selected via ESC[39m (or ESC[0m)
     0x101: default background	  -- this can be selected via ESC[49m (or ESC[0m)
   The following two cannot be select vie ESC-sequence;
   they are used internally, instead of bold/blink
     0x108: emphasized foreground -- we use this internally instead of 'bold'
     0x109: emphasized background -- we use this internally instead of 'blink'
     other: unused (unpredictable results)
 */
#define XDtColorDefaultCheckBit	  (0x100)
#define XDtColorDefNormForeGround (0x100)
#define XDtColorDefNormBackGround (0x101)
#define XDtColorDefEmphForeGround (XDtColorDefNormForeGround | INTENSE)
#define XDtColorDefEmphBackGround (XDtColorDefNormBackGround | INTENSE)

/* in /A option the user may specify 
 * - a palette-color by name (0..15)
 * - a palette-color by index (0..255,0..0xff)
 * - a RGB-color (six heaxdecimal digits, eg F84CE1)
 * to store these, we have 'AnyColor' datatype
 */
typedef struct AnyColor {
    unsigned char usel; /* interpretation on union, 0=empty */
    union {
	DtColor dtcolor;	/* usel==1 */
	COLORREF colorref;	/* usel==2 */
    } u;
} AnyColor;

#define ANYCOLOR_USEL_DTCOLOR	1
#define ANYCOLOR_USEL_COLORREF	2

typedef struct AnyColorPair {
    AnyColor fg;
    AnyColor bg;
} AnyColorPair;

#define ATTR_FG_MASK ((Attrib)0x0ff)
#define FG_MASK  ((Attrib)0x0ff)
#define FG(c)    ((unsigned char)((c)&0xff))
#define GET_FG(a)  ((unsigned char)((a) & 0xff))
#define SET_FG(a,v)  {(a) &= ~FG_MASK;  (a) |= (v)&FG_MASK; }

/* background(paper) */
#define ATTR_BG_MASK ((Attrib)0x0ff00)
#define BG_MASK  ((Attrib)0xff00)
#define BG(c)	((unsigned short)(((c)&0xff) << 8))
#define GET_BG(a)  ((unsigned char)(((a) & BG_MASK)  >> 8))
#define SET_BG(a,c)  {(a) &= ~BG_MASK;  (a) |= BG(c) & BG_MASK; }

#define ATTR_FLAG_MASK      0xff0000L
#define ATTR_FLAG_NONCLR    0x0f0000L /* these attributes don't effect blank characters/lines */
				      /* created by 'clear'-operations (ESC[J, ESC[K) */
#define ATTR_FLAG_UNDERLINE 0x010000L /* sort of implemented */
#define ATTR_FLAG_BOLD      0x020000L /* I don't plan to implement it: see emphasized foreground */
#define ATTR_FLAG_BLINK     0x040000L /* I don't plan to implement it: see emphasized background */
#define ATTR_FLAG_NEGATIVE  0x080000L /* sort of implemented */

#define ATTR_FLAG_CLR       0xf00000L /* these attributes do effect blank characters/lines */
				      /* created by 'clear'-operations (ESC[J, ESC[K) */
#define ATTR_FLAG_DEF_FG    0x100000L /* default foreground color (eg: ESC[m or ESC[39m) */
#define ATTR_FLAG_DEF_BG    0x200000L /* default background color (eg: ESC[m or ESC[49m) */

#define ATTR_GET_FLAGS(a)   ((Attrib)((a) & ATTR_FLAG_MASK))
#define ATTR_SET_FLAGS(a,b) {(a) &= ~ATTR_FLAG_MASK;  (a) |= (b) & ATTR_FLAG_MASK; }

/** #define REVERSE(a) ((Attrib)(ATTR_GET_FLAGS(a) | BG(GET_FG(a)) | FG(GET_BG(a)))) **/

typedef struct ColorRefPair {
    COLORREF fg;	/* foreground */
    COLORREF bg;	/* background */
} ColorRefPair;

#define PaletteSize 16

extern COLORREF colors[PaletteSize];

COLORREF termColor (DtColor color);

/* Default colors cen be set via command line or INI-file
    eg: DTELNET.EXE ... /A white/White-on-black/Black */
typedef struct AttrDefaultData {
    AnyColorPair norm;	/* normal foreground and background colours */
    AnyColorPair emph;	/* emphasized colours (bold->emphasized foreground; blink->emphasized background) */
} AttrDefaultData;

void defaultBlankAttr (AttrDefaultData *into);	/* white/White-on-black/Black */

typedef struct AttrData {
    Attrib current;
/*  AttrDefaultData *defval;	*//* from command-line/ini-file -- it's just a pointer since 20160506 */
    BOOL inverseVideo;		/* ESC[?5h -- DECSCNM */
} AttrData;

/* the last 'retc' parameter is temporal, it might be removed anytime */
COLORREF termXColor (XDtColor color, const AttrDefaultData *pda, DtColor *retc);

#define ATTRIB_UNSET ((Attrib)-1) /* it can't be 0, as it is a legal combination */

#define DefaultAttrData {\
    ATTR_FLAG_DEF_FG | ATTR_FLAG_DEF_BG | FG(White) | BG(Black),\
    {FG(White) | BG(Black), FG(Light_White) | BG(Light_Black)},\
    FALSE\
}

/* called before TextOut, so it has to return the actual colors */
Attrib getAttrToDisplay (ColorRefPair *into,
	const Attrib in, const AttrData *pattr, const AttrDefaultData *defattr, BOOL selected);

/* 20140814.LZS: came from dtelnet.h
 * 20120214.LZS: parseAttrib became public
 * because term.termGetprofile uses it
 */
int parseAttrib (char* desc, AttrDefaultData *into);

/* 20140814.LZS: came from dtelnet.h
 * Convert attribute to string
 */
int tostrAttrib (int opt, const AttrDefaultData *defval, char *to);

/* invoke commdlg!ChooseColor;

   GetColor: 'universal'-version
   GetStdColor: dtelnet-specific; if the first par is NULL, it uses &term.defattr

   return value: 0=ok, otherwise=errorcode */

#define STD_CLRID_NORM_FG 0	/* normal foreground */
#define STD_CLRID_NORM_BG 1	/* normal foreground */
#define STD_CLRID_EMPH_FG 2	/* emphasized foreground */
#define STD_CLRID_EMPH_BG 3	/* emphasized background */

int GetColor    (COLORREF *value, HWND parent);
int GetStdColor (AttrDefaultData *defattr, int std_clrid, HWND parent);
#define GetTermStdColor(clrid,parent) GetStdColor(&term.defattr, (clrid), (parent))

#endif
