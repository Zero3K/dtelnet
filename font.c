/* font.c
 * Copyright (c) 1997 David Cole
 *
 * Manage font dialog and font related .INI file settings.
 */

#include "preset.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "resource.h"
#include "utils.h"
#include "font.h"
#include "term.h"
#include "dtelnet.h"
#include "dialog.h"

static BOOL haveDialog;		/* have we created the Font dialog? */

typedef struct FontData {
    BOOL fMore;                 /* Display more charsets (not only DEFAULT/ANSI/OEM) */
} FontData;

static FontData fontData = {
    FALSE			/* fMore */
};

static FontDesc fd[6]; /* initialised with zeroes */
/* elements:
	0/1:	DT_FONT_REQUESTED/DT_FONT_REQ_OEM
	2/3:	DT_FONT_REAL/DT_FONT_OEM
	4/5:	DT_FONT_REAL_UL/DT_FONT_OEM_UL
 */

/* Enumerate all of the font sizes we will offer
 */
static int validSizes[] = {
    8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28
};

/* Return a FontDesc */
FontDesc *fontGet (int which)
{
    if (which<DT_FONT_REQUESTED || which>DT_FONT_MAX) return NULL;
    else return &fd[which];
}

static void fontGetDesc (HDC dc, HFONT hfont, FontDesc *into, const LOGFONT *logfont)
{
    HFONT hfontOld;
    TEXTMETRIC tm, *ptm= &tm;

    hfontOld = (HFONT)SelectObject(dc, hfont);
    GetTextFace(dc, sizeof(into->fdName), into->fdName);
    GetTextMetrics(dc, ptm);
    into->tm= *ptm;
    into->fdCharSet = ptm->tmCharSet;
    into->fdHeight  = ptm->tmHeight;
    into->fdSize = ptm->tmHeight - ptm->tmExternalLeading
				 - ptm->tmInternalLeading;
    into->fdAveWidth = ptm->tmAveCharWidth;
    into->fdMaxWidth = ptm->tmMaxCharWidth;
    into->fBold   = ptm->tmWeight > FW_NORMAL;
    into->nCharExtra = GetTextCharacterExtra (dc);

    into->logfont= *logfont;

    SelectObject(dc, hfontOld);
}

static int fdCmp (const FontDesc *fd1, const FontDesc *fd2)
{
    int err;

    err= 2*abs(fd1->fdAveWidth - fd2->fdAveWidth) +
	 1*abs(fd1->fdHeight   - fd2->fdHeight);
    return err;
}

typedef struct SearchStruct {
    BOOL filled;
    HFONT hfont;
    FontDesc fd;
    LOGFONT logfont;
    int diff;
} SearchStruct;

/* params:
      fAcceptEqual: if this try is just as bad as the previous, which one we prefer:
	TRUE:  the current one
	FALSE: the previous one
   return:
	1: good/better
	0: wrong/worse
 */
static int fontSearchForOemFontTry (SearchStruct *best, HDC hdc, const FontDesc *reqoem, const FontDesc *from,
	int size, int weight, BOOL fAcceptEqual)
{
    HFONT hfont;
    int diff;
    FontDesc fdTmp;
    int rc;
    LOGFONT logfontv, *logfont= &logfontv;

    memset (logfont, 0, sizeof (*logfont));
    logfont->lfHeight= -size;
    logfont->lfWidth= from->fdAveWidth;
    logfont->lfEscapement= 0;
    logfont->lfOrientation= 0;
    logfont->lfWeight= weight;
    logfont->lfItalic= FALSE;
    logfont->lfUnderline= FALSE;
    logfont->lfStrikeOut= FALSE;
    logfont->lfCharSet= OEM_CHARSET;
    logfont->lfOutPrecision= OUT_DEFAULT_PRECIS;
    logfont->lfClipPrecision= CLIP_DEFAULT_PRECIS;
    logfont->lfQuality= DRAFT_QUALITY;
    logfont->lfPitchAndFamily= FIXED_PITCH;
    logfont->lfFaceName[0]= '\0';

    if (reqoem && reqoem->fdName[0]) {
	strncat (logfont->lfFaceName, reqoem->fdName, LF_FACESIZE-1);
    } else {
	strncat (logfont->lfFaceName, from->fdName, LF_FACESIZE-1);
    }

    hfont = CreateFontIndirect (logfont);
    fontGetDesc (hdc, hfont, &fdTmp, logfont);
    diff= fdCmp (&fdTmp, from);

    if (! best->filled) {
	best->filled= TRUE;
	best->hfont= hfont;
	best->fd= fdTmp;
	best->logfont= *logfont;
	best->diff= diff;
        rc= 1;

    } else if ((diff < best->diff) ||
	       (diff == best->diff && fAcceptEqual)) {
	DeleteObject (best->hfont);
	best->hfont= hfont;
	best->fd= fdTmp;
	best->logfont= *logfont;
	best->diff= diff;
	rc= 1;

    } else {
	DeleteObject (hfont);
	rc= 0;
    }
    return rc;
}

static HFONT fontSearchForOemFont (HDC hdc, const FontDesc *reqoem, const FontDesc *from, FontDesc *into)
{
    struct SearchStruct best;
    int weight, size;
    int result;

    best.filled= FALSE;

    if (from->fBold) weight= FW_BOLD;
    else	     weight= FW_NORMAL;

    size= from->fdSize;

    fontSearchForOemFontTry (&best, hdc, reqoem, from, size, weight, FALSE);
    if (best.diff==0) goto FOUND;

#if 1
/* increase size */
    do {
	size= size+1;
	result= fontSearchForOemFontTry (&best, hdc, reqoem, from, size, weight, TRUE);
	if (best.diff==0) goto FOUND;
    } while (result>0);

/* reduce requirements */
    result= fontSearchForOemFontTry (&best, hdc, reqoem, from, size, FW_DONTCARE, FALSE);
    if (best.diff==0) goto FOUND;
#endif

FOUND:
    *into= best.fd;
    return best.hfont;
}

/* Create fonts from "Requested" FontDesc,
   return HFONTs, TEXTMETRIC and fill FontDesc REAL and OEM */
void fontCreate (HWND hwnd, TEXTMETRIC *ptm, HFONT hfto[FONTS_USED])
{
    HFONT hfont;
    HDC   dc;
    int weight;
    LOGFONT logfontv, *logfont= &logfontv;

    if (fd[DT_FONT_REQUESTED].fBold) weight= FW_BOLD;
    else			     weight= FW_DONTCARE;

    dc = GetDC(hwnd);

    memset (logfont, 0, sizeof (*logfont));
    logfont->lfHeight= -fd[DT_FONT_REQUESTED].fdSize;
    logfont->lfWidth= 0;
    logfont->lfEscapement= 0;
    logfont->lfOrientation= 0;
    logfont->lfWeight= weight;
    logfont->lfItalic= FALSE;
    logfont->lfUnderline= FALSE;
    logfont->lfStrikeOut= FALSE;
    logfont->lfCharSet= fd[DT_FONT_REQUESTED].fdCharSet;
    logfont->lfOutPrecision= OUT_DEFAULT_PRECIS;
    logfont->lfClipPrecision= CLIP_DEFAULT_PRECIS;
    logfont->lfQuality= DEFAULT_QUALITY;
    logfont->lfPitchAndFamily= FIXED_PITCH;
    logfont->lfFaceName[0]= '\0';
    strncat (logfont->lfFaceName, fd[DT_FONT_REQUESTED].fdName, LF_FACESIZE-1);
    hfont= CreateFontIndirect (logfont);

    if (hfont!=NULL) {
	fontGetDesc (dc, hfont, &fd[DT_FONT_REAL], logfont);
	*ptm= fd[DT_FONT_REAL].tm;
    }
    hfto[DT_FONT_REAL - DT_FONT_REAL] = hfont;

    logfont->lfUnderline= TRUE;
    hfont = CreateFontIndirect (logfont);
    if (hfont!=NULL) {
	fontGetDesc (dc, hfont, &fd[DT_FONT_REAL_UL], logfont);
    }
    hfto[DT_FONT_REAL_UL - DT_FONT_REAL] = hfont;

    hfont= fontSearchForOemFont (dc, &fd[DT_FONT_REQ_OEM], &fd[DT_FONT_REAL], &fd[DT_FONT_OEM]);
    hfto[DT_FONT_OEM - DT_FONT_REAL] = hfont;

    *logfont= fd[DT_FONT_OEM].logfont;
    logfont->lfUnderline= TRUE;
    hfont = CreateFontIndirect (logfont);
    if (hfont!=NULL) {
	fontGetDesc (dc, hfont, &fd[DT_FONT_OEM_UL], logfont);
    }
    hfto[DT_FONT_OEM_UL - DT_FONT_REAL] = hfont;

    ReleaseDC(hwnd, dc);
}

void fontDestroy (HFONT hfonts[FONTS_USED])
{
    int i;

    for (i=0; i<FONTS_USED; ++i) {
	if (hfonts[i] != 0) {
	    DeleteObject (hfonts[i]);
	    hfonts[i]= 0;
	}
    }
}

/* Return the name of the font being used in the terminal window
 */
static void fontGetDefault(void)
{
    HDC dc;			/* get display context of screen */
    TEXTMETRIC tm;		/* get size of OEM Fixed Font */

    /* We have not selected a font yet, scan the font list and look
     * for the best font.  On win31, this will be oemXXXX, on win95,
     * it will be Terminal.
     */
    dc = CreateDC("DISPLAY", NULL, NULL, NULL);
    if (dc == NULL) {
	telnetFatal("Cannot get DISPLAY dc");
	return;
    }
    SelectObject(dc, GetStockObject(OEM_FIXED_FONT));
    GetTextMetrics(dc, &tm);
    fd[DT_FONT_REQUESTED].fdSize = tm.tmHeight;
    fd[DT_FONT_REQUESTED].fdCharSet = tm.tmCharSet;
    GetTextFace(dc, sizeof(fd[DT_FONT_REQUESTED].fdName),
	       fd[DT_FONT_REQUESTED].fdName);
    DeleteDC(dc);
}

typedef struct EnumFontProcUserPar {
    HWND hwndList;
    BOOL filterCharSet;
    unsigned char selCharSet;
    BOOL allowList;
} EnumFontProcUserPar;

/* 20131110.LZS: EnumFontFamiliesEx doesn't seem to help:
   it returns many founts that aren't suitable for secondary charset.
 */
#if defined(_WIN32) && defined(USE_ENUMFONTFAMILIESEX)
/* callback for EnumFontFamiliesEx
 */
static int CALLBACK enumFontExProc(const LOGFONT* lf, const TEXTMETRIC* tm,
			  EnumFontTypeType fontType, LPARAM uspar)
{
    const EnumFontProcUserPar *up= (EnumFontProcUserPar *)uspar;

    if ((lf->lfPitchAndFamily & 0x3) == FIXED_PITCH) {
	if (SendMessage(up->hwndList, LB_FINDSTRING, -1, (LPARAM)lf->lfFaceName)==LB_ERR) {
	    SendMessage(up->hwndList, LB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
	}
    }
    return 1;
}
#else
static const char *FixFonts [] = {
    "Consolas",
    "Courier",
    "Courier New",
    "Fixedsys",
    "Lucida Console"
};

#define NFixFonts (sizeof (FixFonts) / sizeof (FixFonts[0]))

static BOOL IsFixFont (const char *name)
{
    int i, found;

    found= -1;
    for (i= 0; i<(int)NFixFonts && found==-1; ++i) {
	if (strcmp (name, FixFonts[i])==0) found= i;
    }
    return found != -1;
}

/* Called once for every font on the system.  Add each Fixed Pitch
 * font found to the font name listbox.
 */
int CALLBACK enumFontProc(const LOGFONT* lf, const TEXTMETRIC* tm,
			  EnumFontTypeType fontType, LPARAM uspar)
{
    const EnumFontProcUserPar *up= (EnumFontProcUserPar *)uspar;

    if (((lf->lfPitchAndFamily & 0x3) == FIXED_PITCH &&
	((!up->filterCharSet || up->selCharSet==lf->lfCharSet))) ||
	 (up->allowList && IsFixFont (lf->lfFaceName))) {

	if (lf->lfFaceName[0]=='@') return 1;	/* vertical font -- we don't need it */

	if (SendMessage(up->hwndList, LB_FINDSTRING, -1, (LPARAM)lf->lfFaceName)==LB_ERR) {
	    SendMessage(up->hwndList, LB_ADDSTRING, 0, (LPARAM)lf->lfFaceName);
	}
    }
    return 1;
}
#endif

/* Add all Fixed Pitch fonts to the font name listbox.
 *
 * fontsel:
 *	1: main font (any charset)
 *	2: secondary font (OEM charset)
 */
static void fontListNames (HWND hwndList, int fontsel)
{
    HDC dc;			/* display context for screen */
    EnumFontProcUserPar up;
    const char *fontname;
    LRESULT idx;		/* find index of font currently being used */

    /* Load up the listbox with all of the font names
     */
    SendMessage(hwndList, LB_RESETCONTENT, 0, 0);
    dc = CreateDC("DISPLAY", NULL, NULL, NULL);
    if (dc == NULL) {
	telnetFatal("Cannot get DISPLAY dc");
	return;
    }
    memset (&up, 0, sizeof (up));
    up.hwndList= hwndList;
    if (fontsel==2){	/* only lfCharSet==OEM */
	up.filterCharSet= TRUE;
	up.selCharSet= OEM_CHARSET;
	up.allowList= TRUE;
    } else {
	up.filterCharSet= FALSE;
    }
#if defined(_WIN32) && defined(USE_ENUMFONTFAMILIESEX)
    {
	LOGFONT lfSelect;

	memset (&lfSelect, 0, sizeof (lfSelect));
	if (fontsel==2) lfSelect.lfCharSet= OEM_CHARSET;
	else		lfSelect.lfCharSet= DEFAULT_CHARSET;
	EnumFontFamiliesEx(dc, &lfSelect, (FONTENUMPROC)enumFontExProc, (LPARAM)&up, 0);
    }
#else
    EnumFontFamilies(dc, NULL, (FONTENUMPROC)enumFontProc, (LPARAM)&up);
#endif
    DeleteDC(dc);

    /* Select the font that is currently being used
     */
    if (fontsel==2 && fd[DT_FONT_REQ_OEM].fdName[0]!='\0') fontname= fd[DT_FONT_REQ_OEM].fdName;
    else						   fontname= fd[DT_FONT_REQUESTED].fdName;

    idx = SendMessage(hwndList, LB_FINDSTRINGEXACT, 0, (LPARAM)fontname);
    if (idx == LB_ERR)
	idx = 0;

    SendMessage(hwndList, LB_SETCURSEL, (WPARAM)idx, 0);
}

/* Select the current font size
 */
static void fontSizeSelect (HWND wnd, int fs)
{
    char str[20];
    LRESULT idx;

    sprintf(str, "%2d", fs);
    idx = SendMessage(wnd, CB_FINDSTRINGEXACT, 0, (LPARAM)str);
    if (idx != LB_ERR) {
	SendMessage(wnd, CB_SETCURSEL, (WPARAM)idx, 0);
    } else {
	SetWindowText (wnd, str);
    }
}

/* Initialise the list of font sizes that we allow
 */
static void fontListSizes(HWND wnd)
{
    unsigned i;
    char str [20];

    /* Load up the listbox with all of the font names
     */
    SendMessage(wnd, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < numElem(validSizes); i++) {
	sprintf(str, "%2d", validSizes[i]);
	SendMessage(wnd, CB_ADDSTRING, 0, (LPARAM)str);
    }

    fontSizeSelect (wnd, fd[DT_FONT_REQUESTED].fdSize);
}

static struct CharacterSet fontCharacterSets [] = {
    {"default", DEFAULT_CHARSET},
    {"ANSI",    ANSI_CHARSET},
    {"OEM",     OEM_CHARSET}
};

#define NfontCharacterSets (sizeof(fontCharacterSets)\
			   /sizeof(fontCharacterSets[0]))

static struct CharacterSet fontMoreCharacterSets [] = {
    {"default",     DEFAULT_CHARSET},
    {"ANSI",        ANSI_CHARSET},
    {"OEM",         OEM_CHARSET},
    {"SYMBOL",      SYMBOL_CHARSET},
    {"SHIFTJIS",    SHIFTJIS_CHARSET},
    {"HANGEUL",     HANGEUL_CHARSET},
    {"GB2312",      GB2312_CHARSET},
    {"CHINESEBIG5", CHINESEBIG5_CHARSET},
    {"JOHAB",       JOHAB_CHARSET},
    {"HEBREW",      HEBREW_CHARSET},
    {"ARABIC",      ARABIC_CHARSET},
    {"GREEK",       GREEK_CHARSET},
    {"TURKISH",     TURKISH_CHARSET},
    {"THAI",        THAI_CHARSET},
    {"EASTEUROPE",  EASTEUROPE_CHARSET},
    {"RUSSIAN",     RUSSIAN_CHARSET},
    {"MAC",         MAC_CHARSET},
    {"BALTIC",      BALTIC_CHARSET}
};

#define NfontMoreCharacterSets (sizeof(fontMoreCharacterSets)\
			       /sizeof(fontMoreCharacterSets[0]))

static void fontListCharSets(HWND hwnd, BOOL fMore)
{
    int i, ncs;
    CharacterSet *csp;

    if (fMore) {
	csp = fontMoreCharacterSets;
	ncs = NfontMoreCharacterSets;
    } else {
	csp = fontCharacterSets;
	ncs = NfontCharacterSets;
    }

    SendMessage(hwnd, LB_RESETCONTENT, 0, 0);
    for (i=0; i<ncs; ++i) {
	SendMessage(hwnd, LB_ADDSTRING, 0,
		    (LPARAM)csp[i].csName);
	if (csp[i].csCode==fd[DT_FONT_REQUESTED].fdCharSet) {
	    SendMessage(hwnd, LB_SETCURSEL, (WPARAM)i, 0L);
	}
    }
}

/* Save the currently selected font name to <fontName>
 */
static void fontGetNameList(HWND wnd, int fwhich)
{
    LRESULT idx = SendMessage(wnd, LB_GETCURSEL, 0, 0L);
    if (idx == LB_ERR)
	return;
    SendMessage(wnd, LB_GETTEXT, (WPARAM)idx, (LPARAM)(LPCSTR)fd[fwhich].fdName);
}

/* Save the currently selected font size to <fontSize>
 * return -1/0/1 = error/OK/not changed
 */
static int fontGetSizeList(HWND wnd, int *into)
{
    LRESULT idx;
    int fs;

    *into= 0;
    idx = SendMessage(wnd, CB_GETCURSEL, 0, 0L);
    if (idx == LB_ERR) return EOF;
    fs = validSizes[(int)idx];
    *into= fs;
    return 0;
}

/* Save the currently entered font size to <fontSize>
 * return -1/0/1 = error/OK/not changed
 */
static int fontGetSizeEdit (HWND wnd, int *into)
{
    char buff [16];
    long fs;
    char *endptr;

    GetWindowText (wnd, buff, sizeof (buff));
    fs = strtol (buff, &endptr, 10);
    if (fs < 4 || fs > 100) return EOF;
    *into= (int)fs;
    return 0;
}

/* Save the currently selected character set to <fontCharSet>
 */
static void fontGetCharSetList(HWND wnd)
{
    LRESULT idx = SendMessage(wnd, LB_GETCURSEL, 0, 0L);
    if (idx < 0)
	return;
    fd[DT_FONT_REQUESTED].fdCharSet = fontMoreCharacterSets[(int)idx].csCode;
}

static struct CharacterSet *fontFindCharset (int csCode)
{
    size_t i;
    CharacterSet *found;

    for (i=0, found=NULL; !found && i<NfontMoreCharacterSets; ++i) {
	if (csCode==fontMoreCharacterSets[i].csCode) found = &fontMoreCharacterSets[i];
    }
    return found;
}

static void fontSetDlgFontName (HWND hdlg, int id, int csCode)
{
    struct CharacterSet *p;
    char tmp [32], *s;

    p = fontFindCharset (csCode);
    if (p) {
	s = p->csName;
    } else {
	s = tmp;
	sprintf (s, "charset #%d", fd[DT_FONT_REAL].fdCharSet);
    }
    SetDlgItemText (hdlg, id, s);
}

static void fontFormatDebugSize (char *tmp, const FontDesc *fd)
{
    const TEXTMETRIC *ptm= &fd->tm;

    sprintf (tmp, "%d,%d+%d+%d+%d;%d/%d%s"
	, (int)ptm->tmHeight
	, (int)ptm->tmExternalLeading
	, (int)ptm->tmInternalLeading
	, (int)(ptm->tmAscent - ptm->tmInternalLeading)
	, (int)ptm->tmDescent
	, (int)ptm->tmAveCharWidth
	, (int)ptm->tmMaxCharWidth
	, fd->fBold? " B": ""
	);
}

static void fontSetStaticFields (HWND hdlg)
{
    char tmp[256];
    int n;

    SetDlgItemText (hdlg, IDC_R_FONT_NAME, fd[DT_FONT_REAL].fdName);
    sprintf (tmp, "%d x %d", fd[DT_FONT_REAL].fdSize, fd[DT_FONT_REAL].fdAveWidth);
    SetDlgItemText (hdlg, IDC_R_FONT_SIZE, tmp);
    fontSetDlgFontName (hdlg, IDC_R_FONT_CHARSET, fd[DT_FONT_REAL].fdCharSet);
    fontFormatDebugSize (tmp, &fd[DT_FONT_REAL]);
    SetDlgItemText (hdlg, IDC_R_FONT_SIZEDBG, tmp);

    SetDlgItemText (hdlg, IDC_O_FONT_NAME, fd[DT_FONT_OEM].fdName);
    sprintf (tmp, "%d x %d", fd[DT_FONT_OEM].fdSize, fd[DT_FONT_OEM].fdAveWidth);
    SetDlgItemText (hdlg, IDC_O_FONT_SIZE, tmp);
    fontSetDlgFontName (hdlg, IDC_O_FONT_CHARSET, fd[DT_FONT_OEM].fdCharSet);
    fontFormatDebugSize (tmp, &fd[DT_FONT_OEM]);
    SetDlgItemText (hdlg, IDC_O_FONT_SIZEDBG, tmp);

{
#ifdef WIN32
static const char sAnsi [] = "System's default ANSI charset is";
static const char sOem []  = "System's OEM charset is";

    n = GetACP ();
    sprintf (tmp, n ? "%s win-%d" : "%s not known", sAnsi, n);
    SetDlgItemText (hdlg, IDC_FONT_ACP, tmp);
    n = GetOEMCP ();
    sprintf (tmp, n ? "%s CP%d" : "%s not known", sOem, n);
    SetDlgItemText (hdlg, IDC_FONT_OEMCP, tmp);
#else
static const char sKB []   = "Keyboard codepage is";

    n = GetKBCodePage ();
    sprintf (tmp, n ? "%s CP%d" : "%s not known", sKB, n);
    SetDlgItemText (hdlg, IDC_FONT_ACP, tmp);
    SetDlgItemText (hdlg, IDC_FONT_OEMCP, "");
#endif
}
}

/* Dialog procedure for the Font dialog
 */
DIALOGRET CALLBACK fontDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    int nChecked;
    int rc;
#ifdef _WIN32
    int idCtrl= LOWORD (wparam);
    HWND hwndCtrl= (HWND)lparam;
    int notification= HIWORD (wparam);
#else
    int idCtrl= wparam;
    HWND hwndCtrl= (HWND)LOWORD(lparam);
    int notification= HIWORD (lparam);
#endif
    static struct FontDlgVars {
	int fontsize;
	int fontsize_edited; /* sets:   CBN_EDITCHANGE
				clears: CBN_SELCHANGE, CBN_KILLFOCUS
				reads:  CBN_KILLFOCUS
			      */
	int font_selected;	/* 0/1/2: None/Primary/Secondary */
	int listbox_content;	/* 0/1/2: Empty/PrimaryFont/SecondaryFont */
    } var;
    struct FontDlgVars *pvar;

    pvar= (void *)GetWindowLongPtr (dlg, DWLP_USER);

    switch (message) {
    case WM_INITDIALOG:
	/* Flag that we have created the dialog, and register the
	 * dialog window handle with the dialog handling code.
	 */
	haveDialog = TRUE;
	dialogRegister(dlg);

	pvar= &var;		/* could be xmalloc? */
	memset (pvar, 0, sizeof (*pvar));
	pvar->fontsize= fd[DT_FONT_REQUESTED].fdSize;
	SetWindowLongPtr (dlg, DWLP_USER, (LPARAM)pvar);

	/* Initialise the font name and font size listboxes.
	 */
	fontListNames(GetDlgItem(dlg, IDC_FONT_NAME), 1); /* Primary */
	fontListSizes(GetDlgItem(dlg, IDC_FONT_SIZE));
	fontListCharSets(GetDlgItem(dlg, IDC_FONT_CHARSET),
			 fontData.fMore);
	CheckDlgButton(dlg, IDC_FONT_MORECS, fontData.fMore);
	CheckDlgButton(dlg, IDC_FONT_BOLD, fd[DT_FONT_REQUESTED].fBold);

	CheckRadioButton(dlg, IDC_FONT_PRIMARY, IDC_FONT_SECONDARY, IDC_FONT_PRIMARY);

	pvar->listbox_content= 1;	/* Primary */
	pvar->font_selected= 1;		/* Primary */

	fontSetStaticFields(dlg);
	return TRUE;

    case WM_COMMAND:
	switch (idCtrl) {
	case IDC_FONT_NAME:
	    /* Every time a new font name is selected, redisplay the
	     * terminal using that font.
	     */
	    if (notification==LBN_SELCHANGE) {
		int fsel;

		if (pvar->listbox_content==1) fsel= DT_FONT_REQUESTED;
		else			      fsel= DT_FONT_REQ_OEM;
		fontGetNameList(GetDlgItem(dlg, IDC_FONT_NAME), fsel);
		termSetFont();
		fontSetStaticFields(dlg);
	    }
	    return TRUE;

	case IDC_FONT_SIZE:
	    /* Every time a new font size is selected, redisplay the
	     * terminal using the new size.
	     */
	    if (notification==CBN_SELCHANGE) {
		int fontsize;

		pvar->fontsize_edited= 0;
		rc = fontGetSizeList (hwndCtrl, &fontsize);
		if (rc || fontsize==pvar->fontsize) return TRUE;
		pvar->fontsize= fontsize;

	    } else if (notification==CBN_KILLFOCUS) {
		int fontsize;

		if (!pvar->fontsize_edited) return TRUE;
		rc = fontGetSizeEdit (hwndCtrl, &fontsize);
		if (rc || fontsize==pvar->fontsize) return TRUE;
		pvar->fontsize= fontsize;
		pvar->fontsize_edited= 0;

	    } else if (notification==CBN_EDITCHANGE) {
		pvar->fontsize_edited= 1;
		return TRUE;

	    } else {
		return TRUE;
	    }
	    if (rc==0) { /* Note: there is no separate font-size for the secondary font */
		fd[DT_FONT_REQUESTED].fdSize= pvar->fontsize;
		termSetFont();
		fontSetStaticFields(dlg);
	    }
	    return TRUE;

	case IDC_FONT_CHARSET:
	    if (notification==LBN_SELCHANGE) {
		fontGetCharSetList (hwndCtrl);
		termSetFont();
		fontSetStaticFields(dlg);
	    }
	    return TRUE;

	case IDC_FONT_MORECS:
	    nChecked = IsDlgButtonChecked(dlg, IDC_FONT_MORECS);
	    fontData.fMore = (BOOL)nChecked;
	    fontListCharSets(GetDlgItem(dlg, IDC_FONT_CHARSET),
			     fontData.fMore);
	    return TRUE;

	case IDC_FONT_BOLD:
	    nChecked = IsDlgButtonChecked(dlg, IDC_FONT_BOLD);
	    fd[DT_FONT_REQUESTED].fBold = (BOOL)nChecked;
	    termSetFont();
	    fontSetStaticFields(dlg);
	    return TRUE;

	case IDC_FONT_PRIMARY:
	case IDC_FONT_SECONDARY:
	    {
		int nfont= (idCtrl==IDC_FONT_PRIMARY)? 1: 2;

		if (pvar->font_selected==nfont) return TRUE;

		pvar->font_selected= nfont;
		fontListNames (GetDlgItem(dlg, IDC_FONT_NAME), nfont); /* Primary/Secondary */
		pvar->listbox_content= nfont;

		return TRUE; 
	    }

	case IDOK:
	case IDCANCEL:
	    /* Destroy the dialog and unregister the dialog handle
	     * with the dialog handling code.
	     */
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    haveDialog = FALSE;
	    return TRUE;
	}
	break;
    }
    return FALSE;
}

/* Called when the user invokes the Font dialog
 */
void showFontDialog(HINSTANCE instance, HWND wnd)
{
    if (!haveDialog) {
	CreateDialog (instance, MAKEINTRESOURCE(IDD_FONT_DIALOG),
		      wnd, fontDlgProc);
    }
}

/* Load the font settings from the .INI file.
 */
void fontGetProfile()
{
    LPCSTR fname;
    char tmp [256];

    fontGetDefault();
    fname = telnetIniFile();
    GetPrivateProfileString("Font", "Name", "",
	fd[DT_FONT_REQUESTED].fdName, sizeof(fd[DT_FONT_REQUESTED].fdName), fname);
    fd[DT_FONT_REQUESTED].fdSize = GetPrivateProfileInt("Font", "Size", fd[DT_FONT_REQUESTED].fdSize, fname);
    fd[DT_FONT_REQUESTED].fdCharSet = (unsigned char)GetPrivateProfileInt("Font", "CharSet",
	fd[DT_FONT_REQUESTED].fdCharSet, fname);
    fontData.fMore = (BOOL)GetPrivateProfileInt("Font", "MoreCharSet", 0, fname);
    fd[DT_FONT_REQUESTED].fBold = (BOOL)GetPrivateProfileInt("Font", "Bold", 0, fname);

    tmp[0]= '\0';
    GetPrivateProfileString("Font", "Secondary", "", tmp, sizeof(tmp), fname);
    if (tmp[0] != '\0') {
	fd[DT_FONT_REQ_OEM].fdName[0]= '\0';
	strncat (fd[DT_FONT_REQ_OEM].fdName, tmp, sizeof (fd[DT_FONT_REQ_OEM].fdName)-1);
    }
}

/* Save the font settings to the .INI file.
 */
void fontSaveProfile()
{
    LPCSTR fname;
    char str[20];

    fname = telnetIniFile();
    WritePrivateProfileString("Font", "Name", fd[DT_FONT_REQUESTED].fdName, fname);
    sprintf(str, "%d", fd[DT_FONT_REQUESTED].fdSize);
    WritePrivateProfileString("Font", "Size", str, fname);
    sprintf(str, "%d", fd[DT_FONT_REQUESTED].fdCharSet);
    WritePrivateProfileString("Font", "CharSet", str, fname);
    sprintf(str, "%d", (int)fontData.fMore);
    WritePrivateProfileString("Font", "MoreCharSet", str, fname);

    sprintf(str, "%d", (int)fd[DT_FONT_REQUESTED].fBold);
    WritePrivateProfileString("Font", "Bold", str, fname);

    if (fd[DT_FONT_REQ_OEM].fdName[0]!='\0') {
	WritePrivateProfileString("Font", "Secondary", fd[DT_FONT_REQ_OEM].fdName, fname);
    }
}
