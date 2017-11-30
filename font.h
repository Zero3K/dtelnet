/* font.h
 * Copyright (c) 1997 David Cole
 *
 * Manage font dialog and font related .INI file settings.
 */
#ifndef __font_h
#define __font_h

/* font.c uses internally */
typedef struct CharacterSet {
    char *csName;
    unsigned char csCode;
} CharacterSet;

typedef struct FontDesc {
    char fdName [LF_FACESIZE];
    int  fdHeight;
    int  fdSize;
    int  fdAveWidth;
    int  fdMaxWidth;
    unsigned char fdCharSet;
    int  fBold;
    TEXTMETRIC tm;
    LOGFONT logfont;
    int nCharExtra;	/* GetTextCharacterExtra */
} FontDesc;

/* We will have two FontDesc:
   the "requested" (selected by user)
   and the "real" (selected by Windows)
   2004.10.24: The OEM font is the third 
   2013.05.29: also underlined real/oem */

#define FONTS_USED	  4

#define FONT_MAIN      0 /* not underlined, user-selected charset */
#define FONT_OEM       1 /* not underlined, OEM charset */
#define FONT_UNDERLINE 2 /* underlined, user-selected charset */
#define FONT_OEM_UL    3 /* underlined, OEM charset */

/* should be reorganized/deleted */
#define DT_FONT_REQUESTED 0 /* ezt kéri a felhasználó elsõdleges fontnak */
#define DT_FONT_REQ_OEM   1 /* ezt kéri a felhasználó OEM fontnak */
#define DT_FONT_REAL      2 /* not underlined, user-selected charset */
#define DT_FONT_OEM       3 /* not underlined, OEM charset */
#define DT_FONT_REAL_UL   4 /* underlined, user-selected charset */
#define DT_FONT_OEM_UL    5 /* underlined, OEM charset */
#define DT_FONT_MAX	  5

FontDesc *fontGet (int which); /* In: DT_FONT_***; Out: FontDesc */

/* Create fonts from "Requested" FontDesc,
   return HFONTs, TEXTMETRIC and fill FontDesc REAL and OEM */
void fontCreate (HWND hwnd, TEXTMETRIC *ptm, HFONT hfto[FONTS_USED]);

void fontDestroy (HFONT hfonts[FONTS_USED]);

/* Display the Font dialog */
void showFontDialog(HINSTANCE instance, HWND wnd);
/* Load the font settings from the .INI file */
void fontGetProfile(void);
/* Save the font settings to the .INI file */
void fontSaveProfile(void);

/* From wingdi.h ... */
#define ANSI_CHARSET            0
#define DEFAULT_CHARSET         1
#define SYMBOL_CHARSET          2
#define SHIFTJIS_CHARSET        128
#define HANGEUL_CHARSET         129
#define GB2312_CHARSET          134
#define CHINESEBIG5_CHARSET     136
#define OEM_CHARSET             255
#define JOHAB_CHARSET           130
#define HEBREW_CHARSET          177
#define ARABIC_CHARSET          178
#define GREEK_CHARSET           161
#define TURKISH_CHARSET         162
#define THAI_CHARSET            222
#define EASTEUROPE_CHARSET      238
#define RUSSIAN_CHARSET         204
#define MAC_CHARSET             77
#define BALTIC_CHARSET          186

#endif
