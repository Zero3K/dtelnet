/* argv.h
 * Copyright (C) 1998 David Cole
 *
 * Implement getopt() like argv/argc parsing
 */
#ifndef __argv_h
#define __argv_h

typedef struct OptRet {
    int optval;		/* the return value of getopt */
    int optind;		/* index of current argument */
    char *optarg; 	/* point to data associated with argument */
} OptRet;

int getoptInit(char* str, char** argv, int argvSize);
int getoptStart(int argc, const char **argv, const char *opts);
int getopt(OptRet *optret);

/* opts syntax:
	'letter'   option is a single letter with no value eg '/B'
	'letter:'  option is a single letter with value eg '/H host'
	'(ident)'  long option without value eg '/CloseOnExit'
	'(ident):' long option with value eg '/AnswerBack VT320-DaveCool'

   return value:
	for short options: the letter itself eg 'B'
	for long options: GETOPT_LONGOPT + position in "opts" - see example
	on error: '?'

   example:
	opts="B:CD(ANSWERBACK):A:"
	"/Bvalue"           returns 'B'
	"/AnswerBack VT520" returns LONGOPT+4 (check why 4!)
	"/Ared-white"       returns 'A'
*/

#define GETOPT_LONGOPT 1000

#endif
