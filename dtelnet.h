/* dtelnet.h
 * Copyright (c) 1997 David Cole
 *
 * Mainline for the dtelnet program.
 */
#ifndef __dtelnet_h
#define __dtelnet_h

#include "lines.h"
#include "resource.h"

/* Define a few messages that we use for reporting network events
 */
#define WM_SOCK_EVENT		(WM_USER + 26) /* socket event */
#define WM_SOCK_GETHOSTBYNAME	(WM_USER + 27) /* gethostbyname complete */
#define WM_SOCK_GETSERVBYNAME	(WM_USER + 28) /* getservbyname complete */

/* Destroy the application window to trigger exit */
void telnetExit(void);
/* Report a fatal error then exit */
void telnetFatal(const char* fmt, ...);
/* Return the name of the application */
const char* telnetAppName(void);
/* Return the name of the .INI file */
const char* telnetIniFile(void);

/* Terminal child window just resized to specified width/height */
void telnetTerminalSize(int xs, int ys);
/* Update the application window title */
void telnetSetTitle(void);

/* Return the program's instance */
HINSTANCE telnetGetInstance(void);
/* Return the program's main window */
HWND telnetGetWnd(void);

MSG *telnetLastMsg(void);

/* keymap-strings:
	see termkey.h and dtelnet.c for details
 */
struct TermKeyMapStringNumS;

extern struct TermKeyMapStringNumS *dtelnetKeyMapStrings;

/* call every SaveProfile */
void telnetSaveProfile (void);

#endif
