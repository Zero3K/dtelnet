/* harap.c */

#include <stdio.h>

#include "buffdata.h"
#include "harap.h"

/* Figyelem: Mivel a 'sep' lehet szokoz is, a vizsgalatok sorrendjet
   nem szabad megvaltoztatni:
   'A' allapot: elobb SPACE azutan SEP	 vizsgalata
   'B' allapot: elobb SEP   azutan SPACE vizsgalata
*/

int xharap (const ConstBuffData *text, BuffData *word,
	    BuffData *rest, int sep, int dir)
{
    int state;
    const char *p, *plimit;
    const char *plast, *pbegin;   /* a megtalalt szo elso es utolso byteja */
    const char *rbegin;
    int  pstep;
    int  rc, opt;

    opt = 0;
    if (dir>=XHARAP_RETMOREINFO-1) {
	opt |= XHARAP_RETMOREINFO;
	dir -= XHARAP_RETMOREINFO;
    }
    state = 'A';
    if (dir>=0) {		    /* normal mukodes */
	p = text->ptr;
	plimit = p + text->len;
	pstep = 1;
    } else {			    /* forditott mukodes */
	plimit = text->ptr - 1;
	p = plimit + text->len;
	pstep = -1;
    }
    pbegin = plast = NULL;
    rbegin = NULL;

    for (; p != plimit && state != 'D'; p += pstep) {
	switch (state) {
	case 'A':
	    if (*p==' ' || *p=='\t');	     /* Sorrend fontos #1 */
	    else if (*p==sep) state = 'C';   /* Sorrend fontos #2 */
	    else { pbegin = plast = p; state = 'B'; }
	    break;

	case 'B':
	    if (*p==sep ||
		(sep==' ' && *p=='\t'))
		state = 'C';		     /* Sorrend fontos #1 */
	    else if (*p==' ' || *p=='\t');   /* Sorrend fontos #2 */
	    else plast = p;
	    break;
	case 'C':
	    if (*p!=' ' && *p!='\t') { rbegin = p; state = 'D'; }
	    break;
	}
    }
    if (rest) {
	if (rbegin==NULL) {
	    rest->ptr = NULL;
	    rest->len = 0;
	} else {
	    if (dir >= 0) { /* normal mukodes */
		rest->ptr = (char *)rbegin;
		rest->len = (size_t)(plimit - rbegin);
	    } else {
		rest->ptr = (char *)text->ptr;
		rest->len = (size_t)(rbegin - plimit);
	    }
	}
    }
    if (word) {
	if (pbegin == NULL) {
	    word->ptr = (char *)pbegin;
	    word->len = 0;
	} else {
	    if (dir >= 0) { /* normal mukodes */
		word->ptr = (char *)pbegin;
		word->len = (size_t)(plast - pbegin + 1);
	    } else {
		word->ptr = (char *)plast;
		word->len = (size_t)(pbegin - plast + 1);
	    }
	}
    }
/*
 *   if (state >= 'C' || pbegin != NULL) rc = 0;
 *   else				 rc = EOF;
 */
    if (state=='A') rc = EOF;
    else if (state=='B' && (opt&XHARAP_RETMOREINFO)) rc = 1;
    else rc = 0;

    return rc;
}
