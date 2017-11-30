/* ctab.h
 * Copyright (c) 2001 dtelnet-team
 *
 * Handling codetables
 *
 */

#ifndef __ctab_h
#define __ctab_h

typedef unsigned char CodeTable [256];

#define EmptyCodeTable ""

/* Create identical codetable */
void ctabIdent (CodeTable into);

/* Create constant codetable */
void ctabConst (CodeTable into, int value);

/* Modify an already filled codetable
 * Params:
 *  ct:    the codetable to modify
 *  pairs: pairs of characters to modify the table (from,to or to,from)
 *  count: is the number of pairs (or zero terminated, if negative)
 *  dir:   the order in pairs: dir=0/1 means from,to / to,from
 */
void ctabModif (CodeTable ct, const void *pairs, int count, int dir);

/* Translay a string with a codetable
 * len<0 means zero-terminated
 */
void ctabTranslay (const CodeTable ct, void *str, int len);

#endif
