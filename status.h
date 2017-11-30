/* status.h
 * Copyright (c) 1997 David Cole
 *
 * Control and manage the status bar
 */
#ifndef __status_h
#define __status_h

/* Return the handle of the status bar */
HWND statusGetWnd(void);
/* Format and display a status message */
void statusSetMsg(const char* fmt, ...);
/* Tell the status bar to redisplay the terminal type */
void statusSetTerm(void);

/* something changed that should affect the status line */
void statusInvalidate (void);

/* Register the status bar window class */
BOOL statusInitClass(HINSTANCE instance);
/* Create the status bar window */
BOOL statusCreateWnd(HINSTANCE instance, HWND parent);

#endif
