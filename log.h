/* log.h
 * Copyright (c) 1997 David Cole
 *
 * Log telnet session and / or protocol
 */
#ifndef __log_h
#define __log_h

/* Return the string to add to the application window title */
char* logGetTitle(void);

/* Initiate logging by querying the user for log file. */
void logStart(void);
/* Stop protocol / session logging */
void logStop(void);
/* Toggle whether or not to log the session */
void logToggleSession(void);
/* Toggle whether or not to log the protocol */
void logToggleProtocol(void);

/* Log some session text */
void logSession(const char* data, int len);
/* Log some protocol using printf style formatting */
void logProtocol(char* fmt, ...);
/* Open log file and replay it through the terminal */
void logReplay(const char* fileName);

/* Return whether or not we are currently logging */
BOOL logLogging(void);
/* Return whether or not we are currently logging the session */
BOOL logLoggingSession(void);
/* Return whether or not we are currently logging the protocol */
BOOL logLoggingProtocol(void);

/* Load logging parameters from the .INI file */
void logGetProfile(void);
/* Save logging parameters to the .INI file */
void logSaveProfile(void);

#endif
