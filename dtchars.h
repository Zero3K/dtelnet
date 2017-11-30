/* dtchars.h
 * Copyright (c) 2001-2017 David Cole
 *
 * Handling dtelnet's charsets
 *
 */

#ifndef __dtchars_h
#define __dtchars_h

#include "ctab.h"

#define DTC_STANDARD_CHARSETS 3 /* UTF8, ANSI and OEM */
#define DTC_UTF8	      1
#define DTC_ANSI	      2
#define DTC_OEM		      3
/* extra charsets (if any) are 4+ */

/* Routines to handle section [ExtraCharsets]
 */
int  dtcEC_LoadNames   (void);
void dtcEC_ReleseNames (void);

char *dtcEC_GetName     (int idx);
int   dtcEC_GetIndex    (const char *name);

int dtcEC_NamesToCombo (HWND combo);

int dtcEC_InitMenu (HMENU menu, UINT baseid);
int dtcEC_CheckMenuItem (char *name);

/* dtcEC_load:

   In:
				    Example1	     Example2	    Example3
	key: server charset	    ANSI	     OEM	    latin2-win1250
   Out:
	four translay tables (some of them are identical):
				    Example1	     Example2	    Example3
	   srv2oem:  server -> oem  win1250->cp852   cp852->cp852   latin2->cp852
	   oem2srv:  oem -> server  cp852->win1250   cp852->cp852   cp852->latin2
	   srv2ansi: server -> ansi win1250->win1250 cp852->win1250 latin2->win1250
	   ansi2srv: ansi -> server win1250->win1250 win1250->cp852 win1250->latin2

   (Note: latin2 and win1250 are similar, but not identical)

   Return:
	0= success, otherwise "ANSI" defaulted
 */
int dtcEC_Load (const char *key,
		CodeTable srv2oem,  CodeTable oem2srv,
		 CodeTable ansi2srv, CodeTable srv2ansi);

#endif
