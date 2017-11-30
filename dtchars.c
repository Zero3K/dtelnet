/* dtchars.c
 * Copyright (c) 2001-2017 David Cole
 *
 * Handling dtelnet's charsets
 *
 */

#include <string.h>
#include <windows.h>

#include "dtelnet.h"
#include "dtchars.h"
#include "utils.h"

#define MAX_CHARSET_NAMES 4096
#define MAX_CHARSET       4096
#define MAX_CODEPAIRS     4096

static char ExtraCharsets[] = "ExtraCharsets";
static char UTF8[] = "UTF8";
static char UTF_8[]= "UTF-8";
static char ANSI[] = "ANSI";
static char OEM[]  = "OEM";

static struct {
    char *buff;
    int count;
    char *next;
    int index;
    HMENU menu;
    UINT  baseid;
    int   lastchecked;
} ECData = {
    NULL,
    0,
    NULL,
    0,
    (HMENU)0,
    0,
    0
};

int dtcEC_LoadNames (void)
{
    int count;
    char *p;
    int len;

    if (ECData.buff==NULL) {
	ECData.buff = xmalloc (MAX_CHARSET_NAMES);
    } else {
	return ECData.count;
    }
    count = 0;
    GetPrivateProfileString (ExtraCharsets, NULL, "",
	ECData.buff, MAX_CHARSET_NAMES, telnetIniFile());
    p = ECData.buff;
    while (*p) {
	len = strlen (p);
	p += len + 1;
	++ count;
    }
    ++p; /* skip terminating '\0' */
    ECData.buff  = xrealloc (ECData.buff, (int)(p - ECData.buff)); /* cut back */
    ECData.count = count;
    ECData.next  = ECData.buff;
    ECData.index = 1;
    return count;
}

char *dtcEC_GetName (int idx)
{
    int next;
    char *p;
    size_t len;

    if      (idx<=0) return NULL;
    else if (idx==1) return UTF8;
    else if (idx==2) return ANSI;
    else if (idx==3) return OEM;
    idx -= DTC_STANDARD_CHARSETS;

    if (ECData.buff == NULL || ECData.count < idx) return NULL;
    if (idx < ECData.index) {
	next = 1;
	p = ECData.buff;
    } else {
	next = ECData.index;
	p = ECData.buff;
    }
    for (; idx > next; ++next) {
	len = strlen (p);
	p += len + 1;
    }
    ECData.buff = p;
    ECData.index = next;
    return p;
}

int dtcEC_GetIndex (const char *name)
{
    char *p;
    int n,idx;

    if      (strcmp (name, UTF8)==0 || strcmp (name, UTF_8)==0) return 1;
    else if (strcmp (name, ANSI)==0)  return 2;
    else if (strcmp (name, OEM)==0)   return 3;
    else {
	if (ECData.buff == NULL) return 0; /* not found */
	p = ECData.buff;
	n = ECData.count;
	for (idx = 1; idx <= n; ++idx) {
	    if (stricmp (p, name) == 0) return idx + DTC_STANDARD_CHARSETS;
	    p += strlen (p) + 1;
	}
    }
    return 0; /* not found */
}

void dtcEC_ReleseNames (void)
{
    if (ECData.buff) {
	xfree (ECData.buff);
	ECData.buff = NULL;
    }
}

int dtcEC_NamesToCombo (HWND combo)
{
    int count, idx;

    count = dtcEC_LoadNames ();
    for (idx = 1; idx <= count+DTC_STANDARD_CHARSETS; ++idx) {
	SendMessage (combo, CB_ADDSTRING, 0,
		     (LPARAM)dtcEC_GetName(idx));
    }
    return count;
}

int dtcEC_InitMenu (HMENU menu, UINT baseid)
{
    int count, idx;

    ECData.menu = menu;
    ECData.baseid = baseid;
    ECData.lastchecked = 0;
    count = dtcEC_LoadNames ();
    for (idx = 1+DTC_STANDARD_CHARSETS; idx <= count + DTC_STANDARD_CHARSETS; ++idx) {
	AppendMenu (menu, MF_STRING | MF_ENABLED,
		    baseid + idx - 1,
		    dtcEC_GetName(idx));
    }
    return count;
}

int dtcEC_CheckMenuItem (char *name)
{
    int idx;

    if (ECData.menu==(HMENU)0) return -1;
    if (ECData.lastchecked != 0) {
	CheckMenuItem (ECData.menu, ECData.baseid + ECData.lastchecked - 1,
		       MF_BYCOMMAND | MF_UNCHECKED);
	ECData.lastchecked = 0;
    }
    idx = dtcEC_GetIndex (name);
    if (idx <= 0) return -1;
    CheckMenuItem (ECData.menu, ECData.baseid + idx - 1,
		   MF_BYCOMMAND | MF_CHECKED);
    ECData.lastchecked = idx;
    return 0;
}

int dtcEC_Load (const char *key,
		CodeTable srv2oem,  CodeTable oem2srv,
                CodeTable ansi2srv, CodeTable srv2ansi)
{
    static const char UNSET [] = "*UNSET";
    char buffer [MAX_CHARSET];
    char codepairs [MAX_CODEPAIRS];
    int rc= 0;

    if (strcmp (key, UTF8)==0 ||
	strcmp (key, UTF_8)==0) {	/* no codetables for UTF8 */
	if (srv2oem)  memset (srv2oem,  0, 256);
	if (oem2srv)  memset (oem2srv,  0, 256);
	if (srv2ansi) memset (srv2ansi, 0, 256);
	if (ansi2srv) memset (ansi2srv, 0, 256);
	return 0;
    }

    ctabIdent (srv2oem);
    ctabIdent (oem2srv);
    ctabIdent (ansi2srv);
    ctabIdent (srv2ansi);

    if (strcmp (key, ANSI)==0) {
ANSI:	AnsiToOemBuff ((LPSTR)srv2oem, (LPSTR)srv2oem, 256);
	OemToAnsiBuff ((LPSTR)oem2srv, (LPSTR)oem2srv, 256);

    } else if (strcmp (key, OEM)==0) {
	AnsiToOemBuff ((LPSTR)ansi2srv, (LPSTR)ansi2srv, 256);
	OemToAnsiBuff ((LPSTR)srv2ansi, (LPSTR)srv2ansi, 256);

    } else {
	GetPrivateProfileString (ExtraCharsets, key, UNSET,
				 buffer, sizeof (buffer), telnetIniFile());
	if (strcmp (buffer, UNSET)==0) {
	    rc= -1;
	    goto ANSI; /* Not found: default= ANSI */
	}
	unescape (buffer, strlen (buffer), codepairs, sizeof (codepairs));

	ctabModif (ansi2srv, codepairs, -1, 1); /* eg: win1250->latin2 */
	ctabModif (srv2ansi, codepairs, -1, 0); /* eg: latin2->win1250 */

	ctabModif (srv2oem, codepairs, -1, 0);
	AnsiToOemBuff ((LPSTR)srv2oem, (LPSTR)srv2oem, 256); /* eg: latin2->cp852 */

	OemToAnsiBuff ((LPSTR)oem2srv, (LPSTR)oem2srv, 256);
	ctabTranslay (ansi2srv, oem2srv, 256);               /* eg: cp852->latin2 */
    }
    return rc;
}

