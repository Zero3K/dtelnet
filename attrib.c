/* attrib.c */

#include "preset.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <commdlg.h>

#include "platform.h"
#include "utils.h"

#include "attrib.h"

COLORREF colors[16] = {
    RGB(0,0,0),			/* Black */
    RGB(200,0,0),		/* Red */
    RGB(0,200,0),		/* Green */
    RGB(200,200,0),		/* Yellow */
    RGB(0,0,200),		/* Blue */
    RGB(200,0,200),		/* Magenta */
    RGB(0,200,200),		/* Cyan */
    RGB(200,200,200),		/* White */

    RGB(100,100,100),		/* Bold Black */
    RGB(255,0,0),		/* Bold Red */
    RGB(0,255,0),		/* Bold Green */
    RGB(255,255,0),		/* Bold Yellow */
    RGB(30,144,255),		/* Bold Blue */
    RGB(255,0,255),		/* Bold Magenta */
    RGB(0,255,255),		/* Bold Cyan */
    RGB(255,255,255)		/* Bold White */
};

#define MAXCOLORVAL 255

/* from: 0..215; r,g,b: 0..5 */
static void decode216color (uint8_t from, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (from>215) from= 215;
    *b= (uint8_t)(from%6);
    *g= (uint8_t)((from/6)%6);
    *r= (uint8_t)((from/36)%6);
}

/* r,g,b: 0..5; to: 0..215 */
static void code216color (uint8_t *to, uint8_t r, uint8_t g, uint8_t b)
{
    if (r>5) r= 5;
    if (g>5) g= 5;
    if (b>5) b= 5;
    *to= (uint8_t)(36*r + 6*g + b);
}

COLORREF termColor (DtColor color)
{
    COLORREF rgb;

    if (color<16) {
/* 'palette' has been commented out for now... or for good, most likely */
	rgb= colors[color];

    } else if (color<232) {
	unsigned tmp;
	unsigned b6, g6, r6;
	unsigned b8, g8, r8;

	tmp = color - 16;
	b6= tmp%6;
	g6= (tmp/6)%6;
	r6= (tmp/36)%6;

	b8= b6*MAXCOLORVAL/5;
	g8= g6*MAXCOLORVAL/5;
	r8= r6*MAXCOLORVAL/5;

	rgb= RGB (r8, g8, b8);

    } else {
	unsigned tmp, b8, g8, r8;

	tmp= color-232;
	b8= g8= r8= tmp*MAXCOLORVAL/23;

	rgb= RGB (r8, g8, b8);
    }

    return rgb;
}
#undef MAXCOLORVAL

/* the last 'retc' parameter is temporal, it might be removed anytime */
COLORREF termXColor (XDtColor color, const AttrDefaultData *pda, DtColor *retc)
{
    COLORREF rgb;
    DtColor tmp;

    if (color&XDtColorDefaultCheckBit) {
	const AnyColor *pac= NULL;

	switch (color) {
	case XDtColorDefNormForeGround: pac= &pda->norm.fg; break;
	case XDtColorDefNormBackGround: pac= &pda->norm.bg; break;
	case XDtColorDefEmphForeGround: pac= &pda->emph.fg; break;
	case XDtColorDefEmphBackGround: pac= &pda->emph.bg; break;
	}
	if (pac && pac->usel==ANYCOLOR_USEL_DTCOLOR) {
	    tmp= pac->u.dtcolor;
	    rgb= termColor (pac->u.dtcolor);

	} else if (pac && pac->usel==ANYCOLOR_USEL_COLORREF) {
	    tmp= 0;	/* <FIXME> see? Here is the trouble! */
	    rgb= pac->u.colorref;

	} else { /* Huh?! */
	    tmp= 0;
	    rgb= (COLORREF)0;
	}

    } else {
	tmp= (DtColor)color;
	rgb= termColor ((DtColor)color);
    }

    if (retc) *retc= tmp;
    return rgb;
}

/* called before TextOut, so it has to return the actual colors */
Attrib getAttrToDisplay (ColorRefPair *into,
	const Attrib in, const AttrData *pattr, const AttrDefaultData *defattr, BOOL selected)
{
    Attrib retval;
    XDtColor xfgc, xbgc;
    DtColor  fgc,  bgc;	/* temporal! */
    int inv_cnt;
    ColorRefPair my_into;

    if (!into) into= &my_into;	/* be careful;) */

/* colors can be swapped for three different reasons: */
    inv_cnt=
	(selected!=0) +				/* 1. selection */
	(pattr->inverseVideo!=0) +		/* 2. ESC[7m */
	((in&ATTR_FLAG_NEGATIVE)!=0);		/* 3. ESC[?5h */

/* foreground */
    if (in&ATTR_FLAG_DEF_FG) xfgc= XDtColorDefNormForeGround;
    else 		     xfgc= GET_FG (in);
/* background */
    if (in&ATTR_FLAG_DEF_BG) xbgc= XDtColorDefNormBackGround;
    else 		     xbgc= GET_BG (in);

/* The order of the following two steps could be swapped.
   Or could be configurable. Or we could implement 'bold' and and 'blink'.
   I hate the idea of blinking, though:
   wasting CPU on annoying the user is a bad idea in my book.
 */

/* swap the colors, if inv_cnt is an odd number */
    if (inv_cnt%2) {	/* odd: do invert */
	XDtColor tmp= xbgc;
	xbgc= xfgc;
	xfgc= tmp;
    }

/* Do the 'bold/blink substitution' hack.
   Note: this hack only affects colors 0..15 and XDefaultXXXX colors
 */
    if (in&ATTR_FLAG_BOLD) /* change forground intensity */
	if (xfgc<=15 || (xfgc&XDtColorDefaultCheckBit)) xfgc ^= INTENSE;
    if (in&ATTR_FLAG_BLINK) /* change background intensity */
	if (xbgc<=15 || (xbgc&XDtColorDefaultCheckBit)) xbgc ^= INTENSE;

#if 0
    retval= SET_FG (retval, xfgc);
    SET_BG (retval, xbgc);
#endif

    into->fg= termXColor (xfgc, defattr, &fgc);
    into->bg= termXColor (xbgc, defattr, &bgc);

    retval= (in&ATTR_FLAG_UNDERLINE) | (bgc<<8) | fgc;

    return retval;
}

static char* color_names[] = {
	"Black", "Red", "Green", "Yellow",
	"Blue", "Magenta", "Cyan", "White"
};

static BOOL isSixHexDigits (const char *str)
{
    size_t i, len;

    len= strlen (str);

    if (len!=6) return FALSE;
    for (i=0; i<len; ++i) {
	if (!isxdigit (str[i])) return FALSE;
    }
    return TRUE;
}

unsigned getXX (const char from[2])
{
static const char digits [] = "0123456789abcdef";
    const char *p1, *p2;

    p1= strchr (digits, tolower (from[0]));
    p2= strchr (digits, tolower (from[1]));
    if (!p1 || !p2) return 0;	/* Grrr */

    return (unsigned char)(((p1-digits)<<4)+(p2-digits));
}

/* Convert a color name to ANSI color index
 *
 * Args:
 * str - string containing color name
 *
 * Returns ANSI color number 0 .. 15, or -1 if no color found.
 * 20131216: also accept numbers between 0 and 255
 */
static BOOL parseColor (const char* str, AnyColor *into)
{
    int idx;			/* iterate over possible colors */
    int found;

    for (found= -1, idx = 0; found==-1 && idx < (int)numElem(color_names); idx++)
	if (stricmp(str, color_names[idx]) == 0)
	    found= idx;

    if (found!=-1) {
	if (isupper (str[0])) found += 8;
	into->usel= ANYCOLOR_USEL_DTCOLOR;
	into->u.dtcolor= (DtColor)found;

	found= TRUE;
	goto RETURN;
    }

    if (isSixHexDigits (str)) {	/* RGB */
	into->usel= ANYCOLOR_USEL_COLORREF;
	into->u.colorref= RGB (getXX (str), getXX (str+2), getXX (str+4));

	found= TRUE;
	goto RETURN;
    }

    { /* numbers between 0 and 255 */
	long n;
	char *endptr;

	n= strtol (str, &endptr, 0);
	if (n>=0 && n<=255 && *endptr=='\0') {
	    into->usel= ANYCOLOR_USEL_DTCOLOR;
	    into->u.dtcolor= (DtColor)n;
	    found= TRUE;
	}
    }

RETURN:
    return found;
}

/* return value:
 *   -1: error
 * 0..2: the colors parsed
 */
static int parseTwoColors (char* str, AnyColor retc[2])
{
    char* fields[3];
    int numFields;
    int iFld;
    int rc, nfound= 0;
    int retval;

    memset (retc, 0, sizeof *retc);

    numFields = split (str, '/', fields, numElem (fields));
    iFld= 0;

    for (rc= 0; rc>=0 && nfound<2 && iFld<numFields; ) {
	rc= parseColor (fields[iFld++], &retc[nfound]);

	if (rc<0) {	/* error */
	    retval= -1;
	    goto RETURN;

	} else {	/* ok */
	    ++nfound;
	}
    }
    retval= nfound;

RETURN:
    return retval;
}

static BOOL hackEmphasizedColor (const AnyColor *old, AnyColor *new_,
	const AnyColor *except)
{
    AnyColor tmp= *old;
    BOOL found;

    if (tmp.usel==ANYCOLOR_USEL_DTCOLOR) {
	if (old->u.dtcolor<0x10) {
	    tmp.u.dtcolor ^= INTENSE;
	
	} else if (old->u.dtcolor<0xe8) {
	    uint8_t r, g, b, tmpcol;

	    tmpcol= (uint8_t)(old->u.dtcolor-0x10);
	    decode216color (tmpcol, &r, &g, &b);
	    code216color (&tmpcol, (uint8_t)(r^1), (uint8_t)(g^1), (uint8_t)(b^1));
	    tmp.u.dtcolor = (int8_t)(0x10 + tmpcol);

	} else {
	    uint8_t tmpcol;

	    tmpcol= (uint8_t)(old->u.dtcolor-0xe8);
	    tmpcol ^= 2;	/* don't ask */
	    tmp.u.dtcolor= (uint8_t)(0xe8 + tmpcol);
	}

	if (except->usel==ANYCOLOR_USEL_DTCOLOR &&
	    except->u.dtcolor==tmp.u.dtcolor) found= FALSE;
	else				      found= TRUE;

    } else if (tmp.usel==ANYCOLOR_USEL_COLORREF) {
	uint8_t r, g, b;
	r= GetRValue (tmp.u.colorref);
	g= GetGValue (tmp.u.colorref);
	b= GetBValue (tmp.u.colorref);
	tmp.u.colorref= RGB (r^16, g^16, b^16);	/* don't ask */

	if (except->usel==ANYCOLOR_USEL_COLORREF &&
	    except->u.colorref==tmp.u.colorref) found= FALSE;
	else					found= TRUE;

    } else {
	found= FALSE;
    }

    if (found) *new_ = tmp;
    else       *new_ = *old;

    return found;
}

/* Convert color description from string to byte value for use in
 * terminal emulation.
 *
 * Args:
 * desc - color description in form FG[-BG[-bold]]
 *
 * Return: 0=ok
 */
int parseAttrib (char* desc, AttrDefaultData *attr)
{
    char* fields[4];		/* split parts of color description */
    int numFields;		/* number of parts in color description */
    int iFld;
    AnyColor fgColors[2], bgColors[2];
    int nfgcol, nbgcol;
    int rc= 0;			/* meaning: okay */

    memset (attr, 0, sizeof (*attr));

    /* Split the colour description fields by delimiter '-'
     */
    numFields = split(desc, '-', fields, numElem(fields));
    iFld= 0;

    /* Foreground colors */
    if (iFld>=numFields) goto RETURN;
    nfgcol= parseTwoColors (fields[iFld++], fgColors);
    if (nfgcol<1) {
	rc= -1;
	goto RETURN;
    }

    /* -on- between the colors */
    if (iFld<numFields && stricmp(fields[iFld], "on")==0) ++iFld;

    /* Background colors */
    if (iFld>=numFields) goto RETURN;
    nbgcol= parseTwoColors (fields[iFld++], bgColors);
    if (nbgcol<1) {
	rc= -1;
	goto RETURN;
    }

    if (nfgcol==1) {
	hackEmphasizedColor (&fgColors[0], &fgColors[1], &bgColors[0]);
    }
    if (nbgcol==1) {
	hackEmphasizedColor (&bgColors[0], &bgColors[1], &fgColors[0]);
    }

    /* We don't do '-bold' anymore -- you can specify 2*2 colors from 16,
     * or from 256 (or from 2^24), that should be enough
     */
#if 0
    /* Intense (bright) attribute dubbed as bold*/
    if (iFld<numFields && stricmp(fields[iFld], "bold") == 0) {
	fg |= INTENSE;
    }
#endif

RETURN:
    attr->norm.fg= fgColors[0];
    attr->norm.bg= bgColors[0];
    attr->emph.fg= fgColors[1];
    attr->emph.bg= bgColors[1];

    return rc;
}

static size_t tostrColor(int opt, const AnyColor *color, char* to)
{
    size_t plen;

    if (color && color->usel==ANYCOLOR_USEL_DTCOLOR) {
	DtColor dtcolor= color->u.dtcolor;
	if (dtcolor <= 15) {
	    const char *pc;

	    pc= color_names [dtcolor&7];
	    plen= strlen (pc);
	    memcpy (to, pc, plen+1); /* copy with terminating zero */
	    if (dtcolor&8) *to= (char)toupper ((unsigned char)*to); /* light color: upper case */
	    else	   *to= (char)tolower ((unsigned char)*to); /* dark color:  lower case */

	} else {
	    plen= sprintf (to, "0x%02x", (int)dtcolor);
	}

    } else if (color && color->usel==ANYCOLOR_USEL_COLORREF) {
	plen= sprintf (to, "%02x%02x%02x", 
		(int)GetRValue (color->u.colorref),
		(int)GetGValue (color->u.colorref),
		(int)GetBValue (color->u.colorref));

    } else {	/* huh?! */
	plen= sprintf (to, "black");
    }
    return plen;
}

static size_t tostrTwoColors (int opt, const AnyColor color2[2], char* to)
{
    char *p= to;

    p += tostrColor (opt, &color2[0], p);
    *p++= '/';
    p += tostrColor (opt, &color2[1], p);
    return (size_t)(p-to);
}

/* Convert attribute to string
 */
int tostrAttrib (int opt, const AttrDefaultData *defval, char *to)
{
    char *p= to;
    AnyColor color2[2];

    color2[0]= defval->norm.fg;
    color2[1]= defval->emph.fg;
    p += tostrTwoColors (opt, color2, p);

    p += sprintf (p, "-on-");

    color2[0]= defval->norm.bg;
    color2[1]= defval->emph.bg;
    tostrTwoColors (opt, color2, p);
    return 0;
}

void defaultBlankAttr (AttrDefaultData *into)
{
    into->norm.fg.usel= ANYCOLOR_USEL_DTCOLOR;
    into->norm.fg.u.dtcolor= White;
    into->norm.bg.usel= ANYCOLOR_USEL_DTCOLOR;
    into->norm.bg.u.dtcolor= Black;

    into->emph.fg.usel= ANYCOLOR_USEL_DTCOLOR;
    into->emph.fg.u.dtcolor= Light_White;
    into->emph.bg.usel= ANYCOLOR_USEL_DTCOLOR;
    into->emph.bg.u.dtcolor= Light_Black;
}

int GetStdColor (AttrDefaultData *defattr, int clrid, HWND parent)
{
    int retval;
    COLORREF rgb;
    AnyColor *cp;

    switch (clrid) {
    case STD_CLRID_NORM_FG: cp= &defattr->norm.fg; break;
    case STD_CLRID_NORM_BG: cp= &defattr->norm.bg; break;
    case STD_CLRID_EMPH_FG: cp= &defattr->norm.fg; break;
    case STD_CLRID_EMPH_BG: cp= &defattr->norm.bg; break;
    default: return -1;
    }

    switch (cp->usel) {
    case ANYCOLOR_USEL_DTCOLOR:	/* 0-255 */
	rgb= termColor (cp->u.dtcolor);
	break;

    case ANYCOLOR_USEL_COLORREF: /* RGB */
	rgb= cp->u.colorref;
	break;

    default:
	rgb= 0x000000;		/* no better idea */
	break;
    }

    retval= GetColor (&rgb, parent);
    if (retval==0) {		/* success */
	cp->usel= ANYCOLOR_USEL_COLORREF;
	cp->u.colorref= rgb;
    }
    return retval;
}

/* invoke the color selector and set the color */
/* input none                                  */
/* output RGB color                            */

#ifndef CC_ANYCOLOR
#define CC_ANYCOLOR	0x100
#endif
#ifndef CC_SOLIDCOLOR
#define CC_SOLIDCOLOR	0x80
#endif

int GetColor (COLORREF *value, HWND parent)
{
    CHOOSECOLOR cc;
    COLORREF mycolors[16];
    BOOL success;
    int rc;

    memcpy (mycolors, colors, sizeof mycolors);

    // Initialize CHOOSECOLOR
    memset (&cc, 0, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = parent;
    cc.lpCustColors = mycolors;
    cc.rgbResult = *value;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT | CC_SOLIDCOLOR | CC_ANYCOLOR;
    success= ChooseColor(&cc);

    if (success) {
	*value= cc.rgbResult;
	rc= 0;
    } else {
	rc= -1;
    }

    return rc;
}
