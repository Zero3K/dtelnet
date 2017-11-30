/* argv.c
 * Copyright (C) 1998-2005 David Cole
 *
 * Implement getopt() like argv/argc parsing
 */
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "argv.h"

/* Split a string into argc/argv
 *
 * Args:
 * str -      the argument string. Will be modified in place
 * argv	-     array that will be filled	with pointers to individual arguments
 * argvSize - number of elements in argv
 *
 * Returns number of arguments found (argc)
 *
 * Example of sh-like argument parsing:
 *
 *    in:  \a\ \c
 *    out: a c
 *    in:  "\a\ \c"
 *    out: \a\ \c
 *    in:  '\a\ \c'
 *    out: \a\ \c
 *    in:  \r\ Hallo" "\rLajos'\'"\""
 *    out: r Hallo \rLajos\"
 */
int getoptInit (char* str, char** argv, int argvSize)
{
    int argc, state, strsep;
    char *to;

    for (argc= 0, to= str, state= 0; argc <= argvSize && *str; ++str) {
	if (state==0) {
	    if (isspace(*str)); /* do nothing */
	    else if (*str=='"' || *str=='\'') {
		argv[argc]= to; strsep= *str; state= 3;
	    } else if (*str=='\\') state= 2;
	    else {
		argv[argc]= to; *to++ = *str; state= 1;
	    }

	} else if (state==1) {
	    if (isspace(*str)) {
		++argc; *to++ = '\0'; state= 0;
	    } else if (*str=='"' || *str=='\'') { strsep= *str; state= 3; }
	    else if (*str=='\\') state= 2;
	    else { *to++ = *str; }

	} else if (state==2) {
	    *to++ = *str;
	    state = 1;

	} else if (state==3) {
	    if (*str==strsep) state= 1;
	    else if (strsep=='"' && *str=='\\') state= 4;
	    else { *to++ = *str; }

	} else if (state==4) {
	    if (*str!='"' && *str!='\\') *to++ = '\\';
	    *to++ = *str;
	    state = 3;
	}
    }
    if (state!=0) { ++argc; *to= '\0'; };

    return argc;
}

static struct opdata {
    int argc;			/* iterate over	argc */
    const char **argv;		/* iterate over	argv */
    const char  *opts;
    int optind;
} optdata= {
    0,
    NULL,
    "",
    -1
};

int getoptStart(int argc, const char **argv, const char *opts)
{
    optdata.argc= argc;
    optdata.argv= argv;
    optdata.opts= opts;
    optdata.optind= -1;

    return 0;
}

/* Iterates	over argc/argv,	returning arguments	in the order that they
 * appear in argv.	The	first time that	the	function is	called,	it
 * stores the passed argc/argv,	these are then used	for	each
 * subsequent call.
 *
 * Args:
 * argcInit	- number of	arguments in argv
 * argvInit	- array	of string arguments
 * opts	-	  describes	argument structure;
 *				<c>	 - <c> is a	switch (no associated data)
 *				<c>: - <c> is an argument with associated data
 *
 * Returns the next	valid argument found in	argv.  If argument found
 * does	not appear in opts, '?' is returned. At end of argv or
 * non-argument	encountered, EOF is returned.
 */
int getopt (OptRet *ret)
{
	int c;					/* the next command line argument */
	const char *str, *optbeg, *optend;	/* scan	the option string */
	const char *arg;			/* scan	the argument string */
	int arglen, optlen, found, optno;

	memset (ret, 0, sizeof (*ret));
	/* Skip	to the next	argument, return EOF if	there are no more.
	 */
	optdata.optind++;
	if (optdata.optind >= optdata.argc)	{
		optdata.argv = NULL;
		ret->optval= EOF;
		ret->optind= optdata.argc;
		goto RETURN;
	}
	ret->optind= optdata.optind;
	/* Terminate argument processing if the	next argument does not
	 * start with either a '-' or a	'/'.
	 */
	if (*optdata.argv[optdata.optind] != '/' &&
	    *optdata.argv[optdata.optind] != '-') {
		optdata.argv = NULL;
		ret->optval= EOF;
		goto RETURN;
	}

	/* Get the argument, if	it is '\0', then we have an error.
	 */
	arg = optdata.argv[optdata.optind]+1;
	c = toupper (arg[0]);
	arglen = strlen (arg);
	if (c == 0)	{
		ret->optval= '?';
		goto RETURN;
	}
	/* Find	the argument in	the option string, if not found, we have
	 * an error.
	 */
	for (found= 0, str= optdata.opts, optno= 1; ! found && *str != '\0'; ) {
	    if (*str=='(') {
		optbeg = str+1;
		optend = strchr (optbeg, ')');
		if (optend) {
		    str = optend + 1;
		} else {             /* This should not be happen */
		    optend = str + strlen (str);
		    str = optend;
		}
		optlen = (int)(optend - optbeg);
		found= arglen>=optlen &&
		       (arg[optlen]=='\x0' || arg[optlen]=='=') &&
		       strnicmp (optbeg, arg, optlen)==0;
	    } else {
		found= toupper(*str)==c;
		if (found) {
		    optbeg = str;
		    optend = str+1;
		    optlen = (int)(optend-optbeg);
		}
		++str;
	    }
	    if (! found) {
		++optno;
		if (*str==':') ++str;
	    }
	}
	if (! found) {
		ret->optval= '?';
		goto RETURN;
	}

	if (*str == ':') {			    /* option with value */
	    if (optlen>1 && arg[optlen]=='=') {     /* /LongOpt=Value    */
		ret->optarg = (char *)(arg + optlen + 1);

	    } else if (optlen==1 && arg[1]!='\0') { /* /XValue */
		ret->optarg = (char *)(arg + optlen);

	    } else {                                /* /Option Value */
		if (optdata.optind+1 < optdata.argc) {
		    ret->optarg = (char *)optdata.argv[++optdata.optind];
		} else {
		    /* error if the arguments are ended */
		    ret->optval= '?';
		    goto RETURN;
		}
	    }
	}

	/* Tell	caller which argument we found
	 */
	if (optlen==1) ret->optval= c;
	else           ret->optval= GETOPT_LONGOPT + optno;
RETURN:
	return ret->optval;
}

