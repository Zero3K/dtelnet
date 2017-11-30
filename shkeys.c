/* shkeys.c */

/* compile me: cc -o shkeys shkeys.c */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef struct ConstBuffData {
    const char *ptr;
    size_t len;
} ConstBuffData;

#define StaticConstBuffData(Name,Sname,Value) \
    static const char Sname [] = Value; \
    static const ConstBuffData Name = { (char *)Sname, sizeof (Sname)-1}

#define StaticConstBD(Name,Value) StaticConstBuffData(Name,N##Name,Value)

static int BuffDataBeg (const ConstBuffData *data, const ConstBuffData *beg)
{
    if (data->len < beg->len) return 0;
    return memcmp (data->ptr, beg->ptr, beg->len) == 0;
}

static struct {
    int ckm;	/* 0=don't touch
		   1=at start: ESC [ ? 1 h	DECCKM-set
		     at stop:  ESC [ ? 1 l	DECCKM-reset
		 */
    int kpam;	/* 0=don't touch
		   1=at start: ESC =		DECKPAM
		     at stop:  ESC >		DECKPNM
		 */
    int mouse;	/* 9 for X10, 1000 for X11 */
} opt = {
    0,
    0,
    0
};

static void ParseArgs (int *pargc, char ***pargv);

#define ESC "\x1b"

static const struct {
    const char *KPAM;
    const char *KPNM;
    const char *CKM_ON;
    const char *CKM_OFF;
} seq = {
    ESC "=",
    ESC ">",
    ESC "[?1h",
    ESC "[?1l"
};

static void PrintBlock (ConstBuffData *bd);
static void PrintSome (ConstBuffData *bd);

int main (int argc, char **argv)
{
    struct termios oldtio, newtio;
    int rc, rdlen, leave;
    char buf [12];
    ConstBuffData rest;

    ParseArgs (&argc, &argv);

    rc= tcgetattr (0, &oldtio);
    if (rc) {
        perror ("tcgetattr (0, &oldtio)");
	return 4;
    }
    memcpy (&newtio, &oldtio, sizeof (newtio));
    newtio.c_lflag &= ~(ISIG | ICANON | ECHO);
    newtio.c_iflag &= ~(INLCR | ICRNL | IXON | IXOFF);
    newtio.c_cflag &= ~(PARENB);
    newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS8;
    newtio.c_cc[VMIN] = 1;
    rc= tcsetattr (0, TCSAFLUSH, &newtio);
    if (rc) {
	perror ("tcsetattr (0, TCSAFLUSH, &newtio)");
	return 12;
    } 

    if (opt.kpam)  write (1, seq.KPAM, strlen (seq.KPAM));
    if (opt.ckm)   write (1, seq.CKM_ON, strlen (seq.CKM_ON));
    if (opt.mouse) {
	char mbuff [64];
	int mlen;

	mlen= sprintf (mbuff, "\x1b[?%dh", opt.mouse);
	write (1, mbuff, (size_t)mlen);
    }

    printf ("Press any key to see codes, ctrl-D to terminate\n\n");

    for (leave= 0; ! leave; ) {
        rdlen= read (0, buf, sizeof (buf));
	if (rdlen<0) {
	    perror ("read(0)");
	    leave= 1; /* error */
	    continue;
	} else if (rdlen==0) {
	    leave= 1; /* EOF */
	    continue;
	}

	rest.len= rdlen;
	rest.ptr= buf;

	PrintBlock (&rest);

	if (buf[0]==4) leave= 1; /* ctrl-D */
    }

    if (opt.mouse) {
	char mbuff [64];
	int mlen;

	mlen= sprintf (mbuff, "\x1b[?%dl", opt.mouse);
	write (1, mbuff, (size_t)mlen);
    }

    if (opt.kpam)  write (1, seq.KPNM, strlen (seq.KPNM));
    if (opt.ckm)   write (1, seq.CKM_OFF, strlen (seq.CKM_OFF));

    rc= tcsetattr (0, TCSAFLUSH, &oldtio);
    if (rc) {
	perror ("tcsetattr (0, TCSAFLUSH, &oldtio)");
	return 12;
    }
    return 0;
}

static void PrintMouse (const char *from)
{
    int code, codeh, codel;
    int row, col;

    code= (unsigned char)from[3];
    col = (unsigned char)from[4] - 32;
    row = (unsigned char)from[5] - 32;

    codeh= code&0xe0;
    codel= code&0x03;

    printf ("Mouse event %02x:", code);

    if ((codeh==0x20 && codel==0x03) ||
        (codeh==0x40 && codel==0x03)) {
	printf (" button release / move after release");

    } else if (codeh==0x20) {
	printf (" button #%d press", 1+codel);

    } else if (codeh==0x60) {
	printf (" button #%d press or wheel #%d %s"
		, 4+codel, 1+codel/2, codel%2==0? "up": "down");

    } else if (codeh==0x40) {
	printf (" press-and-move button #%d is down", 1+codel);

    } else if (codeh==0x80) {
	printf (" press-and-move button #%d is down or wheel #%d %s"
	    , 4+codel, 1+codel/2, codel%2==0? "up": "down");
    }

    if (code&0x04) printf (" shift pressed");
    if (code&0x08) printf (" alt pressed");
    if (code&0x10) printf (" ctrl pressed");
    printf (" at row=%d col=%d", row, col);
    printf ("\n");
}

StaticConstBD (BMousePref, "\x1b[M");

static void PrintBlock (ConstBuffData *bd)
{
    ConstBuffData mybd;

    while (bd->len >= 6 && BuffDataBeg (bd, &BMousePref)) {
	mybd.ptr= bd->ptr;
	mybd.len= 6;
	bd->ptr += 6;
	bd->len -= 6;

	PrintMouse (mybd.ptr);
	PrintSome (&mybd);
    }
    if (bd->len) PrintSome (bd);
}

static void PrintSome (ConstBuffData *bd)
{
    size_t i;
    int c;
    char prbuff [128], *p;

    p= prbuff;
    for (i=0; i<bd->len; ++i) {
	c= (unsigned char)bd->ptr[i];
	if (c < ' ') {
	    p += sprintf (p, "^%c",  c+64);
	} else if (c==0x7f) {
	    p += sprintf (p, "^?");
	} else {
	    p += sprintf (p, "%c", c);
	}
    }
    for (i=0; i<bd->len; ++i) {
	c= (unsigned char)bd->ptr[i];
	printf ("%-16s %3d 0x%02x 0%03o\n", prbuff, c, c, c);
	prbuff[0] = '\0';
    }
}

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

    while (--argc && **++argv=='-' && parse_arg) {
	switch (argv[0][1]) {
	case 'c': case 'C':
	     if (strcasecmp (argv[0], "-ckm")==0) {
		opt.ckm= 1;
		break;
	     }
	     goto UNKOPT;

	case 'k': case 'K':
	     if (strcasecmp (argv[0], "-kpam")==0) {
		opt.kpam= 1;
		break;
	     }
	     goto UNKOPT;

	case 'm': case 'M':
	     if (strcasecmp (argv[0], "-mouse")==0 ||
		 strcasecmp (argv[0], "-mouse-x11")==0) {
		opt.mouse= 1000;
		break;

	     } else if (strcasecmp (argv[0], "-mouse-x10")==0) {
		opt.mouse= 9;
		break;

	     } else if (strcasecmp (argv[0], "-mouse-btn-event")==0 ||
			strcasecmp (argv[0], "-mouse-event-btn")==0) {
		opt.mouse= 1002;
		break;

	     } else if (strcasecmp (argv[0], "-mouse-any-event")==0 ||
			strcasecmp (argv[0], "-mouse-event-any")==0) {
		opt.mouse= 1003;
		break;
	     }
	     goto UNKOPT;

	case 0:parse_arg = 0; break;
	case '-':
	    if (argv[0][2]=='\0') {
		parse_arg = 0;
		break;
	    }
	    goto UNKOPT;

	default: UNKOPT:
	    fprintf (stderr, "usage: %s [-kpam] [{-mouse-x10|-mouse-x11}]'\n"
		, progname);
	    exit (4);
	}
    }
    ++argc;
    *--argv = progname;
    *pargc = argc;
    *pargv = argv;
}
