/* termkeyt.c */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "buffdata.h"
#include "utils.h"
#include "termkey.h"

#if 0
void *xmalloc (int size) { return malloc (size); }
void *xrealloc (void *p, int size) { return realloc (p, size); }
void xfree    (void *p) { free (p); }
#endif

void telnetAppName (void) {}
void telnetExit (void) {}
void termGetWnd (void) {}
void logStop (void) {}

int main (int argc, char **argv)
{
    TermKeyMap mapv = EmptyTermKeyMap, *map;
    TermKeyMapCond cond;
    TermKey tk;
    int rc;
    ConstBuffData bd;
    long l;
static TermKeyMapStringNum strn [] = {
    {"PrintScreen", 1001},
    {"NewWindow",   1002}
};
#define Nstrn (sizeof(strn) / sizeof(strn[0]))
static TermKeyMapStringNumS strs = {
    0, strn, Nstrn
};

    if (argc==1) {
	map= &mapv;
    } else {
	printf ("Testing default map\n");
	map= NULL;	/* Using Default map */
    }

    printf ("\n=== Adding ===\n");

    memset (&cond, 0, sizeof (cond));
    cond.mask.modifiers = TMOD_SHIFT | TMOD_CONTROL;
    cond.test.modifiers = TMOD_SHIFT;
    cond.mask.locks = 0;
    cond.test.locks = 0;
    rc= tkmAddKeyMapS (map, VK_F1, &cond, "F1 Shift=yes, Control=No", 0);
    printf ("Add#1 rc=%d\n", rc);

    memset (&cond, 0, sizeof (cond));
    cond.mask.modifiers = TMOD_SHIFT;
    cond.test.modifiers = TMOD_SHIFT;
    cond.mask.locks = 0;
    cond.test.locks = 0;
    rc= tkmAddKeyMapS (map, VK_F1, &cond, "F1 Shift=yes", 0);
    printf ("Add#2 rc=%d\n", rc);

    memset (&cond, 0, sizeof (cond));
    cond.mask.modifiers = TMOD_SHIFT;
    cond.test.modifiers = 0;
    cond.mask.locks = LOCK_NUM;
    cond.test.locks = LOCK_NUM;
    rc= tkmAddKeyMapS (map, VK_F2, &cond, "F2 Shift=no NumLock=yes", 0);
    printf ("Add#3 (F2 Shift=no, NumLock=yes) rc=%d\n", rc);

    memset (&cond, 0, sizeof (cond));
    cond.mask.modifiers = TMOD_SHIFT;
    cond.test.modifiers = 0;
    cond.mask.locks = 0;
    cond.test.locks = 0;
    rc= tkmAddKeyMapS (map, VK_F4, &cond, NULL, 100);
    printf ("Add#4 (F4 Shift=no) rc=%d\n", rc);

    printf ("\n=== Searching ===\n");

    memset (&tk, 0, sizeof (tk));
    tk.keycode= VK_F2;
    tk.modifiers= 0;
    tk.locks= LOCK_NUM;
    rc= tkmGetKeyMap (map, &tk, &bd, &l);
    printf ("Test(NumLock+F2) returned %d ('%.*s'/%ld)\n", rc, (int)bd.len, bd.ptr, l);

    memset (&tk, 0, sizeof (tk));
    tk.keycode= VK_F2;
    tk.modifiers= TMOD_SHIFT;
    tk.locks= 0;
    rc= tkmGetKeyMap (map, &tk, &bd, &l);
    printf ("Test(Shift+F2) returned %d ('%.*s'/%ld)\n", rc,(int) bd.len, bd.ptr, l);

    memset (&tk, 0, sizeof (tk));
    tk.keycode= VK_F1;
    tk.modifiers= TMOD_SHIFT;
    rc= tkmGetKeyMap (map, &tk, &bd,&l);
    printf ("Test(Shift+F1) returned %d ('%.*s'/%ld)\n", rc, (int)bd.len, bd.ptr, l);

    memset (&tk, 0, sizeof (tk));
    tk.keycode= VK_F1;
    tk.modifiers= TMOD_SHIFT | TMOD_CONTROL;
    rc= tkmGetKeyMap (map, &tk, &bd, &l);
    printf ("Test#3 (Shift+Control+F1) returned %d ('%.*s'/%ld)\n", rc, (int)bd.len, bd.ptr, l);

    memset (&tk, 0, sizeof (tk));
    tk.keycode= VK_F3;
    tk.modifiers= TMOD_SHIFT | TMOD_CONTROL;
    rc= tkmGetKeyMap (map, &tk, &bd, &l);
    printf ("Test#4 (Shift+Control+F3) returned %d ('%.*s'/%ld)\n", rc, (int)bd.len, bd.ptr, l);

    printf ("\n=== Dumping ===\n");

    tkmDumpMap (stdout, map);

    tkmReleaseMap (map);

    printf ("\n=== Loading from file ===\n");

    tkmLoadFromIniFile (map, "\\HOME\\ZSIGA\\DTELNET\\DTELNET.INI", "KEYMAP",
		&strs);

    printf ("\n=== Dumping ===\n");

    tkmDumpMap (stdout, map);

    return 0;
}
