/* shchars.c */

/* compile me: cc -o shchars -lcurses shchars.c */

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>

static unsigned char stoplist [256];

static void ruler (void);

#define Valid(p) ((p)!=NULL && (p)!=(char *)-1 && *(p))

static struct {
    int enacs;
} opt = {
    0		/* -1/0/1 = don't use / default / use */
};

static void ParseArgs (int *pargc, char ***pargv);

int main (int argc, char **argv)
{
    const char *term;
    const char *smacs, *rmacs;
    const char *enacs;
    const char *acsc;
    int err, rc, i, j, c, len;

    ParseArgs (&argc, &argv);

    term= getenv ("TERM");
    if (Valid (term)) {
        fprintf (stderr, "TERM=%s\n", term);
    } else {
        fprintf (stderr, "TERM variable not set, exiting\n");
	return 4;
    }
    rc= setupterm ((char *)term, 1, &err);
    if (rc!=OK) {
        fprintf (stderr, "setupterm failed err=%d\n", err);
	return 4;
    }
    smacs= tigetstr ("smacs");
    rmacs= tigetstr ("rmacs");
    acsc = tigetstr ("acsc");
    enacs= tigetstr ("enacs");

    if (!Valid (smacs) || !Valid (rmacs) || !Valid (acsc)) {
        fprintf (stderr, "could not get smacs, rmacs or acsc\n");
	return 8;
    }
    if (Valid (enacs) && opt.enacs>=0) {
	fprintf (stderr, "Using enacs.\n");
	printf ("%s", enacs);

    } else if (!Valid(enacs) && opt.enacs>0) {
	fprintf (stderr, "Terminal '%s' has no enacs.\n", term);
    }
/*  fprintf (stderr, "Using smacs/rmacs.\n"); */

    /* enable graphical characters */
    for (len= strlen (acsc), i=0; i<len-1; i+=2) {
        stoplist [(unsigned char)acsc[i+1]] &= ~2;
    }

    ruler ();
    for (i=0x00; i<=0xf0; i+=0x10) {
        printf ("%02x  ", i);
        for (j=0x0; j<=0xf; j++) {
	    c= i+j;
	    if (stoplist[c] & 1) printf (". ");
	    else                 printf ("%c ", c);
	}
	printf (" %02x  %s", i, smacs);
        for (j=0x0; j<=0xf; j++) {
	    c= i+j;
	    if (stoplist[c] & 2) printf (". ");
	    else                 printf ("%c ", c);
	}
	printf ("%s %02x\n", rmacs, i);
    }
    ruler ();
    return 0;
}

static void ruler (void)
{
    int j;
    
    printf ("    ");
    for (j=0; j<=0xf; ++j) {
        printf ("%x ", j);
    }
    printf ("     ");
    for (j=0; j<=0xf; ++j) {
        printf ("%x ", j);
    }
    printf ("\n");
}

/*  If the output of this program is broken,
 *  you may increase values of this table:
 *  1 means: dont show it as regular character,
 *  2 means: dont show it as graphic character,
 *  3 means: do not show it at all
 */
static unsigned char stoplist [256] = {
/*       0 1 2 3 4 5 6 7 8 9 a b c d e f */
/* 00 */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/* 10 */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/* 20 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* 30 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* 40 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* 50 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* 60 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* 70 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,

/*       0 1 2 3 4 5 6 7 8 9 a b c d e f */
/* 80 */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/* 90 */ 3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
/* a0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* b0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* c0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* d0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* e0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
/* f0 */ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static void ParseArgs (int *pargc, char ***pargv)
{
    int argc;
    char **argv;
    int parse_arg;
    char *progname;

    argc = *pargc;
    argv = *pargv;
    parse_arg = 1;
    progname = argv[0];
    opt.enacs= 0;

    while (--argc && **++argv=='-' && parse_arg) {
	switch (argv[0][1]) {
	case 'e':case 'E':
	    if (strcasecmp (argv[0], "-enacs")==0) {
	        opt.enacs= 1;
		break;

	    } else if (strcasecmp (argv[0], "-enacs-")==0) {
	        opt.enacs= -1;
		break;

	    } else goto UNKOPT;

	case 'n':case 'N':
		if (strcasecmp (argv[0], "-no-enacs")==0) {
		    opt.enacs= -1;
		    break;
		} else goto UNKOPT;

	case 0: case '-': parse_arg = 0; break;
	default:
UNKOPT:     fprintf (stderr, "Unknown option '%s'\n", *argv);
	    exit (4);
	}
    }
    ++argc;
    *--argv = progname;
    *pargc = argc;
    *pargv = argv;
}
