/* debuglog.c */

#include "preset.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/timeb.h>
#include <time.h>
#include <windows.h>

#include "debuglog.h"

/* write entry into dtelnet.dbg */

void dtelnetDebugF (int parindent, const char *fmt, ...)
{
    static int indent= 0;
    static int fopened= 0;
    static FILE *f= NULL;
    va_list ap;
#ifdef WIN32
    SYSTEMTIME tm;
#else
    struct timeb tb;
    struct tm tm;
#endif
    size_t fmtlen;
    const char *arrow_prefix;

    if (fopened<0) return;
    if (fopened==0) {
	f= fopen ("dtelnet.dbg", "wt");
	if (!f) {
	    fopened= -1;
	    return;
	}
	fopened= 1;
    }

/* indent>0: entering a function; increment indenting after printing this
   indent<0: leaving a function;  decrement indenting before printing this
   indent=0: inside a function
*/

    arrow_prefix= "";
    if (parindent>0) {		/* add "-> " if not already present */
	if (memcmp (fmt, "-> ", 3) != 0) arrow_prefix= "-> ";

    } else if (parindent<0) {	/* add "<- " if not already present */
	if (memcmp (fmt, "<- ", 3) != 0) arrow_prefix= "<- ";
    }

    if (parindent<0 && indent>0) --indent;

    if (!fmt) goto print_done;
#ifdef WIN32
    GetLocalTime(&tm);
    fprintf (f, "%04d%02d%02d.%02d%02d%02d.%03d"
	, tm.wYear, tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond
	, tm.wMilliseconds);
#else
    ftime (&tb);
    tm= *localtime (&tb.time);
    fprintf (f, "%04d%02d%02d.%02d%02d%02d.%03d"
	, 1900+tm.tm_year, 1+tm.tm_mon, tm.tm_mday
	, tm.tm_hour, tm.tm_min, tm.tm_sec
	, tb.millitm);
#endif
    fprintf (f, " %*s %s", 3*indent, "", arrow_prefix);

    fmtlen= strlen (fmt);
    if (fmtlen) {
	va_start (ap, fmt);
	vfprintf (f, fmt, ap);
	va_end (ap);
    }

    if (fmtlen==0 || fmt [fmtlen-1] != '\n') {
	fputc ('\n', f);
    }

print_done:
    if (parindent>0) ++indent;
}
