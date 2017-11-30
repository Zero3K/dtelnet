/* debuglog.h */

#ifndef DEBUGLOG_H
#define DEBUGLOG_H

/* write entry into dtelnet.dbg */

void dtelnetDebugF (int parindent, const char *fmt, ...);

/* indent>0: entering a function; increment indenting after printing this
   indent<0: leaving a function;  decrement indenting before printing this
   indent=0: inside a function
*/

#endif
