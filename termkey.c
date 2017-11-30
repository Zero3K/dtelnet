/* termkey.c */

#include "preset.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "buffdata.h"
#include "utils.h"
#include "harap.h"
#include "termkey.h"

unsigned short termKeyClass (TermKey *tk)
{
    unsigned short keycode= tk->keycode;
    unsigned short retval;

    if      (keycode >= VK_F1 && keycode <= VK_F4)  retval= TKC_F_14;
    else if (keycode >= VK_F5 && keycode <= VK_F24) retval= TKC_F_OTHER;
    else if ((keycode >= VK_NUMPAD0 && keycode <= VK_NUMPAD9) ||
	      keycode == VK_DECIMAL)		    retval= TKC_NUMPAD_NUM;
    else if (keycode == VK_NUMLOCK ||
	     (keycode >= VK_MULTIPLY && keycode <= VK_SUBTRACT) ||
	     (keycode == VK_RETURN && (tk->flags&KF_EXTENDED))) retval= TKC_NUMPAD_OTHER;
    else retval= TKC_OTHER;

    tk->classe= retval;
    return retval;
}

void termFillTermKey (WPARAM wparam, LPARAM lparam, TermKey *tk)
{
    memset (tk, 0, sizeof (*tk));

    tk->keycode  = (unsigned short)(wparam&0xff);
    tk->scancode = (unsigned short)((lparam>>16)&0xff);
    tk->flags    = (unsigned short)((lparam>>16)&0xff00);

    GetKeyboardState (tk->keyvect);
    termFillModLock (tk);
    termNumpadMagic (tk);  /* the order is important: call me first, because I change the keycode */
    termKeyClass (tk);	   /* the order is important: call this when the keycode is fixed */
}

void termFillModLock (TermKey *tk)
{
    tk->modifiers= 0;
    tk->locks    = 0;

    if (tk->keyvect[VK_LSHIFT]  &0x80) tk->modifiers |= TMOD_LSHIFT;
    if (tk->keyvect[VK_LCONTROL]&0x80) tk->modifiers |= TMOD_LCONTROL;
    if (tk->keyvect[VK_LMENU]   &0x80) tk->modifiers |= TMOD_LALT;
    if (tk->keyvect[VK_LWIN]    &0x80) tk->modifiers |= TMOD_LWIN;

    if (tk->keyvect[VK_RSHIFT]  &0x80) tk->modifiers |= TMOD_RSHIFT;
    if (tk->keyvect[VK_RCONTROL]&0x80) tk->modifiers |= TMOD_RCONTROL;
    if (tk->keyvect[VK_RMENU]   &0x80) tk->modifiers |= TMOD_RALT;
    if (tk->keyvect[VK_RWIN]    &0x80) tk->modifiers |= TMOD_RWIN;

    if (tk->modifiers&(TMOD_LSHIFT  |TMOD_RSHIFT)   || tk->keyvect[VK_SHIFT]  &0x80) tk->modifiers |= TMOD_SHIFT;
    if (tk->modifiers&(TMOD_LCONTROL|TMOD_RCONTROL) || tk->keyvect[VK_CONTROL]&0x80) tk->modifiers |= TMOD_CONTROL;
    if (tk->modifiers&(TMOD_LALT    |TMOD_RALT)     || tk->keyvect[VK_MENU]   &0x80) tk->modifiers |= TMOD_ALT;
    if (tk->modifiers&(TMOD_LWIN    |TMOD_RWIN))				     tk->modifiers |= TMOD_WIN;

    if (tk->keyvect[VK_CAPITAL]&0x01) tk->locks |= LOCK_CAPS;
    if (tk->keyvect[VK_NUMLOCK]&0x01) tk->locks |= LOCK_NUM;
    if (tk->keyvect[VK_SCROLL] &0x01) tk->locks |= LOCK_SCROLL;
}

void termKeyHackEmulate (TermKey *tk, unsigned short keycode, int action)
{
    int fLock= keycode==VK_CAPITAL || keycode==VK_NUMLOCK || keycode==VK_SCROLL;

    switch (action) {
    case TKHACK_DOWN:
	tk->keyvect[keycode] |= 0x80;
	if (fLock) tk->keyvect[keycode] ^= 0x01;
	break;
    case TKHACK_UP:
	tk->keyvect[keycode] &= (unsigned char)~0x80;
	break;
    case TKHACK_BOTH:
	tk->keyvect[keycode] &= (unsigned char)~0x80;
	if (fLock) tk->keyvect[keycode] ^= 0x01;
	break;
    default:; /* no more actions */ 
    }

    termFillModLock (tk);
}

void termToAscii (TermKey *tk)
{
#ifdef WIN32
    WORD xlat[2];
#else
    DWORD xlat[2];
#endif
    int xlatResult;

    memset (xlat, 0, sizeof (xlat));
    xlatResult= ToAscii (tk->keycode, tk->scancode, tk->keyvect, xlat, 0);

    if (xlatResult>=1)
	tk->ascii [tk->nascii++]= (WORD)xlat[0];
    if (xlatResult>=2 && xlat[0]!=xlat[1])
	tk->ascii [tk->nascii++]= (WORD)xlat[1];
}

static void termNumpadMagicOne (TermKey *tk, unsigned short numkeycode, unsigned short extkeycode)
{
    if ((tk->flags)&KF_EXTENDED) { /* actual extended key, do nothing */
	return;
    }
    if ((tk->keycode==extkeycode) && (tk->locks&LOCK_NUM)) { /* NumLock is On and Shift pressed */
	tk->modifiers |= TMOD_SHIFT;
    }
    if (tk->keycode==extkeycode) { /* NumLock is Off or NumLock is On and Shift pressed */
	tk->keycode= numkeycode;	 /* The key the user actually pressed */
	tk->keyvect[numkeycode] |= 0x80; /* The numeric key was actually pressed */
	tk->keyvect[extkeycode] &= 0x7f; /* The extended key wasn't actually pressed */
    }
}

void termNumpadMagic (TermKey *tk)
{
    switch (tk->keycode) {
    case VK_PRIOR:
    case VK_NUMPAD9:
	termNumpadMagicOne (tk, VK_NUMPAD9, VK_PRIOR);
	break;

    case VK_NEXT:
    case VK_NUMPAD3:
	termNumpadMagicOne (tk, VK_NUMPAD3, VK_NEXT);
	break;

    case VK_UP:
    case VK_NUMPAD8:
	termNumpadMagicOne (tk, VK_NUMPAD8, VK_UP);
	break;

    case VK_DOWN:
    case VK_NUMPAD2:
	termNumpadMagicOne (tk, VK_NUMPAD2, VK_DOWN);
	break;

    case VK_RIGHT:
    case VK_NUMPAD6:
	termNumpadMagicOne (tk, VK_NUMPAD6, VK_RIGHT);
	break;

    case VK_LEFT:
    case VK_NUMPAD4:
	termNumpadMagicOne (tk, VK_NUMPAD4, VK_LEFT);
	break;

    case VK_HOME:
    case VK_NUMPAD7:
	termNumpadMagicOne (tk, VK_NUMPAD7, VK_HOME);
	break;

    case VK_END:
    case VK_NUMPAD1:	
	termNumpadMagicOne (tk, VK_NUMPAD1, VK_END);
	break;

    case VK_INSERT:
    case VK_NUMPAD0:
	termNumpadMagicOne (tk, VK_NUMPAD0, VK_INSERT);
	break;

    case VK_CLEAR:
    case VK_NUMPAD5:
	termNumpadMagicOne (tk, VK_NUMPAD5, VK_CLEAR);
	break;

    case VK_DELETE:
    case VK_DECIMAL:
	termNumpadMagicOne (tk, VK_DECIMAL, VK_DELETE);
	break;
    }
}

static TermKeyMap defmap= EmptyTermKeyMap;

int tkmAddKeyMap (TermKeyMap *map,
		  unsigned short keycode, const TermKeyMapCond *cond,
		  const ConstBuffData *mapto,
		  long lmapto)
{
    TermKeyMapEntry *p;
    TermKeyMapElement *q;
    unsigned short i, j, found;

    if (!map) map= &defmap;

    if (keycode>=TERMKEY_MAXVK) return -1;
    if (map->entries==NULL) {
	map->entries= xmalloc (TERMKEY_MAXVK * sizeof (TermKeyMapEntry));
	for (i=0; i<TERMKEY_MAXVK; ++i) {
	    p= &map->entries[i];
	    p->keycode= i;
	    p->elements= NULL;
	    p->nelement= 0;
	    p->nmax= 0;
	}
    }
    p= &map->entries[keycode];

    for (j=0, found=(unsigned short)-1; found==(unsigned short)-1 && j<p->nelement; ++j) {
	q= &p->elements[j];
    	if (TermKeyMapCondEqual (cond, &q->cond)) found= j;
    }
    if (found!=(unsigned short)-1) { /* remove old definition */
	q= &p->elements[found];
	if (q->mapto.len) xfree (q->mapto.ptr);
	if (found!=p->nelement-1) {
	    memmove (&p->elements[found], &p->elements[found+1],
		     sizeof (TermKeyMapEntry)*(p->nelement-1-found));
	}
	--p->nelement;
    }
    if (!mapto && !lmapto) return 0;	/* remove */

    if (p->nelement == p->nmax) {
	p->nmax += (unsigned short)8;
	p->elements = xrealloc (p->elements, p->nmax * sizeof (TermKeyMapElement));
    }
    q= &p->elements[p->nelement];
    q->cond.mask.modifiers= cond->mask.modifiers;
    q->cond.mask.locks=     cond->mask.locks;
    q->cond.mask.flags=     cond->mask.flags;
    q->cond.test.modifiers= cond->mask.modifiers & cond->test.modifiers;
    q->cond.test.locks=     cond->mask.locks     & cond->test.locks;
    q->cond.test.flags=     cond->mask.flags     & cond->test.flags;

    if (mapto && mapto->len) {
	q->mapto.ptr= xmalloc (mapto->len+1);
	memcpy (q->mapto.ptr, mapto->ptr, mapto->len);
	q->mapto.ptr[mapto->len]= '\0';
	q->mapto.len = mapto->len;
    } else {
	q->mapto.ptr= "";
	q->mapto.len= 0;
    }
    q->lmapto= lmapto;

    ++p->nelement;

    return 0;
}

int tkmAddKeyMapS (TermKeyMap *map,
		   unsigned short keycode, const TermKeyMapCond *cond,
		   const char *mapto, long lmapto)
{
    ConstBuffData bd, *pbd;

    if (mapto) {
	bd.ptr= mapto;
	bd.len= strlen (bd.ptr);
	pbd= &bd;
    } else {
	pbd= NULL;
    }
    return tkmAddKeyMap (map, keycode, cond, pbd, lmapto);
}

int tkmGetKeyMap (const TermKeyMap *map,
		  const TermKey *tk,
		  ConstBuffData *output,
		  long *lmapto)
{
    const TermKeyMapEntry *p;
    const TermKeyMapElement *q;
    unsigned short j, found;

    if (output) {
	output->ptr= NULL;
	output->len= 0;
    }
    if (lmapto) {
	*lmapto= 0;
    }

    if (!map) map= &defmap;

    if (map->entries==NULL || tk->keycode>TERMKEY_MAXVK) return -1;
    p= &map->entries[tk->keycode];
    if (!p->elements) return -1;

    for (j=0, found=(unsigned short)-1; found==(unsigned short)-1 && j<p->nelement; ++j) {
	q= &p->elements[j];
    	if (TermKeyMapCondMatch (tk, &q->cond)) found= j;
    }

    if (found != (unsigned short)-1) {
	if (output) {
	    q= &p->elements[found];
	    output->ptr= q->mapto.ptr;
	    output->len= q->mapto.len;
	}
	if (lmapto) *lmapto= q->lmapto;
	return 0;
    }
    return -1;
}

void tkmReleaseMap (TermKeyMap *map)
{
    TermKeyMapEntry *p;
    TermKeyMapElement *q;
    unsigned short i, j;

    if (!map) map= &defmap;

    if (!map->entries) return;
    for (i=0; i<TERMKEY_MAXVK; ++i) {
	p= &map->entries[i];
	for (j=0; j<p->nelement; ++j) {
	    q= &p->elements[j];
	    if (q->mapto.len) xfree (q->mapto.ptr);
	}
    }
    xfree (map->entries);
    map->entries= NULL;
}

void tkmDumpMap (FILE *to, const TermKeyMap *map)
{
    const TermKeyMapEntry *p;
    const TermKeyMapElement *q;
    unsigned short i, j;
    unsigned short nmaps= 0, nkeys= 0;

    if (!map) map= &defmap;

    if (!map->entries) {
	fprintf (to, "KeyMap is empty (unallocated)\n");
	return;
    }
    for (i=0; i<TERMKEY_MAXVK; ++i) {
	p= &map->entries[i];
	if (p->nelement) {
	    ++nkeys;
	    nmaps += p->nelement;
	    for (j=0; j<p->nelement; ++j) {
		q= &p->elements[j];
	    	fprintf (to, "Map key=%02x modifiers=%02x/%02x locks=%02x/%02x to '",
		    p->keycode,
		    q->cond.mask.modifiers, q->cond.test.modifiers,
		    q->cond.mask.locks,     q->cond.test.locks);
		if (q->mapto.len) {
		    Buffer tmp;
		    char stmp [256];

		    tmp.ptr= stmp;
		    tmp.maxlen= sizeof (stmp);
		    tmp.len= 0;
	
		    bescape (0, (ConstBuffData *)&q->mapto, &tmp, NULL);
		    fprintf (to, "%.*s", (int)tmp.len, tmp.ptr);
		}
	    	fputc ('\'', to);
		if (q->lmapto) fprintf (to, " l=%ld", q->lmapto);
	    	fputc ('\n', to);
	    }
	}
    }
    if (nmaps) {
	fprintf (to, "Totally %u maps for %u keys\n", nmaps, nkeys);
    } else {
	fprintf (to, "KeyMap is empty\n");
    }
}

StaticConstBD (BAlt,      "Alt");
StaticConstBD (BAltGr,    "AltGr");
StaticConstBD (BCaps,     "Caps");
StaticConstBD (BCapsLock, "CapsLock");
StaticConstBD (BControl,  "Control");
StaticConstBD (BCtrl,     "Ctrl");
StaticConstBD (BExt,      "Ext");
StaticConstBD (BExtended, "Extended");
StaticConstBD (BLfAlt,    "LeftAlt");
StaticConstBD (BLfControl,"LeftControl");
StaticConstBD (BLfCtrl,   "LeftCtrl");
StaticConstBD (BLfShift,  "LeftShift");
StaticConstBD (BLfWin,	  "LeftWin");
StaticConstBD (BLAlt,     "LAlt");
StaticConstBD (BLControl, "LControl");
StaticConstBD (BLCtrl,    "LCtrl");
StaticConstBD (BLShift,   "LShift");
StaticConstBD (BLWin,	  "LWin");
StaticConstBD (BNum,      "Num");
StaticConstBD (BNumLock,  "NumLock");
StaticConstBD (BRgAlt,    "RightAlt");
StaticConstBD (BRgControl,"RightControl");
StaticConstBD (BRgCtrl,   "RightCtrl");
StaticConstBD (BRgShift,  "RightShift");
StaticConstBD (BRgWin,	  "RightWin");
StaticConstBD (BRAlt,     "RAlt");
StaticConstBD (BRControl, "RControl");
StaticConstBD (BRCtrl,    "RCtrl");
StaticConstBD (BRShift,   "RShift");
StaticConstBD (BRWin,	  "RWin");
StaticConstBD (BScl,      "Scroll");
StaticConstBD (BSclLock,  "ScrollLock");
StaticConstBD (BShift,    "Shift");
StaticConstBD (BWin,	  "Win");

typedef struct ModTabElem {
const ConstBuffData *name;
    TermKeyMapBits bits;    
} ModTabElem;

static const ModTabElem ModTab [] = { /* ABC-sorted for bsearch */
{&BAlt,      {TMOD_ALT, 0, 0}},				/* Alt */
{&BAltGr,    {TMOD_ALT | TMOD_CONTROL, 0, 0}},		/* AltGr */
{&BCaps,     {0, LOCK_CAPS, 0}},               		/* Caps */
{&BCapsLock, {0, LOCK_CAPS, 0}},               		/* CapsLock */
{&BControl,  {TMOD_CONTROL, 0, 0}},            		/* Control */
{&BCtrl,     {TMOD_CONTROL, 0, 0}},            		/* Ctrl */
{&BExt,      {0, 0, KF_EXTENDED}},			/* Ext */
{&BExtended, {0, 0, KF_EXTENDED}},			/* Extended */
{&BLAlt,     {TMOD_LALT, 0, 0}},			/* LAlt */
{&BLControl, {TMOD_LCONTROL, 0, 0}},			/* LControl */
{&BLCtrl,    {TMOD_LCONTROL, 0, 0}},			/* LCtrl */
{&BLfAlt,    {TMOD_LALT, 0, 0}},			/* LeftAlt */
{&BLfControl,{TMOD_LCONTROL, 0, 0}},			/* LeftControl */
{&BLfCtrl,   {TMOD_LCONTROL, 0, 0}},			/* LeftCtrl */
{&BLfShift,  {TMOD_LSHIFT, 0, 0}},			/* LeftShift */
{&BLfWin,    {TMOD_LWIN, 0, 0}},			/* LeftWin */
{&BLShift,   {TMOD_LSHIFT, 0, 0}},			/* LShift */
{&BLWin,     {TMOD_LWIN, 0, 0}},			/* LWin */
{&BNum,      {0, LOCK_NUM, 0}},               		/* Num */
{&BNumLock,  {0, LOCK_NUM, 0}},               		/* NumLock */
{&BRAlt,     {TMOD_RALT, 0, 0}},			/* RAlt */
{&BRControl, {TMOD_RCONTROL, 0, 0}},			/* RCtrl */
{&BRCtrl,    {TMOD_RCONTROL, 0, 0}},			/* RControl */
{&BRgAlt,    {TMOD_RALT, 0, 0}},			/* RightAlt */
{&BRgControl,{TMOD_RCONTROL, 0, 0}},			/* RightControl */
{&BRgCtrl,   {TMOD_RCONTROL, 0, 0}},			/* RightCtrl */
{&BRgShift,  {TMOD_RSHIFT, 0, 0}},			/* RightShift */
{&BRgWin,    {TMOD_RWIN, 0, 0}},			/* RightWin */
{&BRShift,   {TMOD_RSHIFT, 0, 0}},			/* RShift */
{&BRWin,     {TMOD_RWIN, 0, 0}},			/* RWin */
{&BScl,      {0, LOCK_SCROLL, 0}},              	/* Scroll */
{&BSclLock,  {0, LOCK_SCROLL, 0}},              	/* ScrollLock */
{&BShift,    {TMOD_SHIFT, 0, 0}},               	/* Shift */
{&BWin,      {TMOD_WIN, 0, 0}}               		/* Win */
};
#define NModTab (sizeof(ModTab)/sizeof(ModTab[0]))

static int ModCompare (const void *pkey, const void *pelem)
{
    const ConstBuffData *key= pkey;
    const ModTabElem   *elem= pelem;
    int cmp= BuffDataCmpI (key, elem->name);
    return cmp;
}

static int Value (const ConstBuffData *b)
{
    char buff [64];
    long lval;
    char *endptr;

    if (b->len==0 || b->len > sizeof (buff)-1) return -1;
    memcpy (buff, b->ptr, b->len);
    buff [b->len]= '\0';
    lval= strtol (buff, &endptr, 0);
    if (*endptr) return -1;
    return (int)lval;
}

static int tkmpsnFind (const void *pkey, const void *pelem)
{
    const ConstBuffData *key= (const ConstBuffData *)pkey;
    const TermKeyMapStringNum *elem= (const TermKeyMapStringNum *)pelem;
    ConstBuffData belem;

    belem.ptr= elem->str;
    belem.len= strlen (belem.ptr);

    return BuffDataCmp (key, &belem);
}

static int tkmLoadFromIniStr (TermKeyMap *into, const char *str,
	TermKeyMapStringNumS *strs)
{
    int rc;
    ConstBuffData b, b1, b2, be, btmp, bkey;
    int keycode;
    const ModTabElem *me;
    TermKeyMapCond cond = EmptyTermKeyMapCond;
    int act;
    DynBuffer bunesc= EmptyBuffer;
    const ConstBuffData *pmapto;
    long lmapto;

    if (into==NULL) into= &defmap;

    b.ptr= str;
    b.len= strlen (str);
    rc= harapr (&b, (BuffData *)&b1, (BuffData *)&b2, '=');
    if (rc) {
	rc= -1;
	goto RETURN;		/* No '=' found */
    }

/*  printf ("\"%.*s\" <> \"%.*s\"\n",
 *	(int)b1.len, b1.ptr, (int)b2.len, b2.ptr);
 */

    btmp= b1;
    rc= jharapr (&btmp, (BuffData *)&bkey, (BuffData *)&btmp, ',');
    if (rc==-1) goto RETURN;
    keycode= Value (&bkey);
    if (keycode<0 ||keycode>TERMKEY_MAXVK) {
	rc= -2;
	goto RETURN;
    }

    while ((rc= harapr (&btmp, (BuffData *)&be, (BuffData *)&btmp, ','))>=0) {
/*	printf ("\t\"%.*s\"\n",
 *		(int)be.len, be.ptr);
 */
	if (be.len==0) continue;

	act= 1; /* default: + */

	if (be.ptr[be.len-1]=='+') {
	    act= 1;
	    --be.len;

        } else if (be.ptr[be.len-1]=='-') {
	    act= 2;
	    --be.len;

	} else if (be.ptr[be.len-1]=='*') {
	    act= 0;
	    --be.len;
	}
	if (be.len==0) continue;
	me= bsearch (&be, ModTab, NModTab, sizeof (ModTab[0]), ModCompare);
        if (!me) continue;
/*      printf ("\t\"%.*s\" found %02x %02x act=%d\n"
 * 	    , (int)me->name->len, me->name->ptr
 *	    , me->bits.modifiers, me->bits.locks
 *	    , act);
 */
	if (act==0) continue;	/* 0: don't care */
	cond.mask.modifiers |= me->bits.modifiers;
	cond.mask.locks     |= me->bits.locks;
	cond.mask.flags     |= me->bits.flags;
	if (act==1) {		/* 1: must be set */
	    cond.test.modifiers |= me->bits.modifiers;
	    cond.test.locks     |= me->bits.locks;
	    cond.test.flags     |= me->bits.flags;
	} else {		/* 2: must be unset */
	    cond.test.modifiers &= (unsigned short)~me->bits.modifiers;
	    cond.test.locks     &= (unsigned short)~me->bits.locks;
	    cond.test.flags     &= (unsigned short)~me->bits.flags;
	}
    }
/*  printf ("\tkey=%d mod=%02x/%02x lock=%02x/%02x\n",
 *	keycode,
 *	cond.mask.modifiers, cond.test.modifiers,
 *	cond.mask.locks,     cond.test.locks);
 */

    if (b2.len==0) {	/* delete the current mapping */
	pmapto= NULL;
	lmapto= 0;
	goto MAP;
    }
    if (b2.len<2 || b2.ptr[0]!='"' || b2.ptr[b2.len-1]!='"') { /* it might be a special literal */
	const TermKeyMapStringNum *np;

	if (!strs || !strs->n || !strs->p) {
	    rc= -1;
	    goto RETURN;
	}
	np = bsearch (&b2, strs->p, strs->n, sizeof (strs->p[0]), tkmpsnFind);
	if (!np) {
	    rc= -1;
	    goto RETURN;
	}
	pmapto= NULL;
	lmapto= np->num;
	goto MAP;
    }
    b2.ptr += 1; /* strip "quotes" */
    b2.len -= 2;

    bunesc.len= 0;
    BuffSecure (&bunesc, b2.len);
    bunescape (2|4, &b2, &bunesc, NULL);	/* 2: stop at unescaped "double" quote; 
						   4: 'bunesc' in DynBuffer */
    pmapto= (ConstBuffData *)&bunesc;
    lmapto= 0;

MAP:
    rc= tkmAddKeyMap (into, (unsigned short)keycode, &cond, pmapto, lmapto);

RETURN:
    BuffCutback (&bunesc, 0);
    return rc;
}

static int tkmpsnCompar (const void *p, const void *q)
{
    const TermKeyMapStringNum *l= (const TermKeyMapStringNum *)p;
    const TermKeyMapStringNum *r= (const TermKeyMapStringNum *)q;

    return strcmp (l->str, r->str);
}

int tkmLoadFromIniFile (TermKeyMap *into, const char *filename, const char *sectionname,
	TermKeyMapStringNumS *strs)
{
    char *buff;
    UINT sectmaxsize= 32767, size;
    const char *p, *plim, *q;
    int rc;

    if (!into) into= &defmap;
    if (!sectionname) sectionname= "Keymap";

    if (strs && !strs->fsorted) {
	if (strs->n >= 2) {
	    qsort (strs->p, strs->n, sizeof (strs->p[0]), tkmpsnCompar);
	}
	strs->fsorted= 1;
    }

    buff= xmalloc (sectmaxsize);
    memset (buff, '#', sectmaxsize);

    size= GetPrivateProfileSection (sectionname, buff, sectmaxsize, filename);
    if (size==0) { /* No 'Keymap' section */
	rc= 0;
	goto RETURN;
    }
    if (size==sectmaxsize-2) {
	fprintf (stderr, "Error loading section '%s' from file '%s' (maxsize=%ld)\n",
		sectionname, filename, (long)sectmaxsize);
	rc= -1;
	goto RETURN;
    }
    p= buff;
    plim= buff + size;
    while (p<plim) {
	q= p + strlen(p);
/*      printf ("%s\n", p); */
	tkmLoadFromIniStr (into, p, strs);
	p= q+1;
    }
    rc= 0;

RETURN:
    if (buff) free (buff);
    return rc;
}
