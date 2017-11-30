/* ctab.c
 * Copyright (c) 2001 dtelnet-team
 *
 * Handling codetables
 *
 */

#include <string.h>

#include "ctab.h"

/* Create identical codetable */
void ctabIdent (CodeTable into)
{
    int i;

    for (i=0; i<256; ++i) {
	into[i] = (char)i;
    }
}

/* Create constant codetable */
void ctabConst (CodeTable into, int value)
{
    memset (into, value, 256);
}

/* Modify an already filled codetable
 * Params:
 *  ct:    the codetable to modify
 *  pairs: pairs of characters to modify the table (from,to or to,from)
 *  count: is the number of pairs (or zero terminated, if negative)
 *  dir:   the order in pairs: dir=0/1 means from,to / to,from
 */
void ctabModif (CodeTable ct, const void *pairs, int count, int dir)
{
    int leave;
    int fromcode, tocode;
    unsigned char *p;

    p = (unsigned char *)pairs;
    do {
	if (count < 0) leave = (p[0]=='\0' || p[1]=='\0');
	else           leave = (count-- == 0);
	if (! leave) {
	    if (dir==0) {
		fromcode = p[0];
		tocode   = p[1];
	    } else {
		fromcode = p[1];
		tocode   = p[0];
	    }
	    ct [fromcode] = (char)tocode;
	    p += 2;
	}
    } while (! leave);
}

/* Translay a string with a codetable
 * len<0 means zero-terminated
 */
void ctabTranslay (const CodeTable ct, void *str, int len)
{
    int leave;
    unsigned char *p;

    p = (unsigned char *)str;
    do {
	if (len<0) leave = (*p=='\0');
	else leave = (len-- == 0);
	if (! leave) {
	    *p = ct [*(unsigned char *)p];
	    ++p;
	}
    } while (! leave);
}

