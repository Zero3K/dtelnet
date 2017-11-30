/* log.c
 * Copyright (c) 1997 David Cole
 *
 * Log telnet session and / or protocol
 */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "term.h"
#include "dtelnet.h"
#include "log.h"
#include "emul.h"

static BOOL logging;		/* are we logging right now? */
static BOOL loggingSession = TRUE; /* if logging, should log session? */
static BOOL loggingProtocol;	/* if logging, should we log protocol? */

static char logFileName[_MAX_PATH];
static char logFileTitle[_MAX_PATH]; /* add to application window title */

static HFILE logFh;		/* handle of open log file */
static char* writingStr = "Error writing to log file";

/* Return the string to add to the application window title
 */
char* logGetTitle()
{
    if (logFileTitle[0] == '\0')
	return NULL;
    return logFileTitle;
}

/* Report an error during the log process
 */
static void logError(char* str)
{
    MessageBox(termGetWnd(), str, telnetAppName(), MB_OK | MB_ICONHAND);
}

/* Initiate logging by querying the user for a file to log messages
 * into.
 */
void logStart()
{
    OPENFILENAME ofn;
    OFSTRUCT of;

    /* If currently logging, close file
     */
    if (logging)
	logStop();

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = termGetWnd();
    ofn.lpstrFilter = "Text Files (*.LOG)\0*.LOG\0All Files (*.*)\0*.*\0";
    ofn.lpstrCustomFilter = NULL;
    ofn.nMaxCustFilter = 0;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = logFileName;
    ofn.nMaxFile = _MAX_PATH;
    ofn.lpstrInitialDir = NULL;
    ofn.lpstrFileTitle = logFileTitle;
    ofn.nMaxFileTitle = _MAX_PATH;
    ofn.lpstrTitle = NULL;
    ofn.lpstrDefExt = "LOG";
    ofn.Flags = 0;

    /* Ask user for file to log messages into
     */
    if (!GetOpenFileName(&ofn))
	return;

    /* Open the new log file
     */
    logFh = OpenFile(logFileName, &of, OF_WRITE|OF_CREATE);
    if (logFh < 0) {
	logError("Cannot create log file");
	return;
    }

    /* Tell application window to update its title.  This will then
     * call logGetTitle to determine the log filename portion of the
     * title.
     */
    telnetSetTitle();

    /* Flag that we are currently logging to a file
     */
    logging = TRUE;
}

/* Stop protocol / session logging
 */
void logStop()
{
    if (!logging)
	return;

    _lclose(logFh);

    logFileTitle[0] = '\0';

    /* Tell application window to update its title.
     */
    telnetSetTitle();

    /* FLag that we are no longer logging
     */
    logging = FALSE;
}

/* Toggle whether or not to log the session (user visible output from
 * remote end).
 */
void logToggleSession()
{
    loggingSession = !loggingSession;
}

/* Toggle whether or not to log the protocol
 */
void logToggleProtocol()
{
    loggingProtocol = !loggingProtocol;
}

/* Log some session text
 */
void logSession(const char* data, int len)
{
    /* Only log text if we are instructed to
     */
    if (!logging || !loggingSession)
	return;

    if (_lwrite(logFh, data, len) != (UINT)len) {
	/* On error, terminate logging
	 */
	logError(writingStr);
	logStop();
    }
}

/* Log some protocol using printf style formatting
 *
 * Args:
 * fmt - printf format string
 * ... - arguments to format string
 */
void logProtocol(char* fmt, ...)
{
    va_list ap;
    char message[512];		/* format message in here */
    int len;

    /* Only log protocol if we are instructed to
     */
    if (!logging || !loggingProtocol)
	return;

    /* Format the message
     */
    va_start(ap, fmt);
    len = vsprintf(message, fmt, ap);
    va_end(ap);

/* hack a CR before LF */
    if (len>0 && message[len-1]=='\n') {
	if (len<2 || message[len-2]!='\r') {
	    message[len-1]= '\r';
	    message[len++]= '\n';
	}
    }
    if (_lwrite(logFh, message, len) != (UINT)len) {
	/* On error, stop logging
	 */
	logError(writingStr);
	logStop();
    }
}

/* Open a previously saved log file and replay it through the terminal
 * emulator.
 *
 * Args:
 * fileName - the name of the log file to replay
 */
void logReplay(const char* fileName)
{
    int numRead;
    OFSTRUCT of;
    unsigned char buff[256];
    /* Open the old log file
     */
    HFILE fh = OpenFile(fileName, &of, OF_READ);
    if (fh < 0)
	return;
    /* Read the log file and send it through the terminal emulator
     */
    while ((numRead = _lread(fh, buff, sizeof(buff))) > 0)
	emulAddText((char *)buff, numRead, FALSE); /* Not sure :-) LZsiga */
    _lclose(fh);
}

/* Return whether or not we are currently logging
 */
BOOL logLogging()
{
    return logging;
}

/* Return whether or not we are currently logging the session
 */
BOOL logLoggingSession()
{
    return loggingSession;
}

/* Return whether or not we are currently logging the protocol
 */
BOOL logLoggingProtocol()
{
    return loggingProtocol;
}

/* Load logging parameters from the .INI file
 */
void logGetProfile()
{
    loggingSession = GetPrivateProfileInt("Terminal", "Log Session", 0,
					  telnetIniFile());
    loggingProtocol = GetPrivateProfileInt("Terminal", "Log Protocol", 0,
					   telnetIniFile());
}

/* Save logging parameters to the .INI file
 */
void logSaveProfile()
{
    char str[20];		/* format flags for saving */

    sprintf(str, "%d", loggingSession);
    WritePrivateProfileString("Terminal", "Log Session", str, telnetIniFile());
    sprintf(str, "%d", loggingProtocol);
    WritePrivateProfileString("Terminal", "Log Protocol", str, telnetIniFile());
}
