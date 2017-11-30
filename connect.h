/* connect.h
 * Copyright (C) 1998 David Cole
 *
 * Manage connect dialog, connection history, and connect parameters.
 */
#ifndef __connect_h
#define __connect_h

#include "utils.h"
#include "emul.h"

#define USER_NAME_LEN 20	/* length of a user name */
#define PORT_NAME_LEN 33	/* length of a port name */
#define PROTOCOL_NAME_LEN 33	/* length of a protocol name */
#define HOST_NAME_LEN 128	/* length of host name */
#define CHARSET_NAME_LEN 33	/* length of characterset */
#define ANSWERBACK_LEN	128	/* length of answerback message */

/* Information remembered in connection history
 */
/* Fred. On some Unix platforms, the Backspace key doesn't work (on my
 * SUN servers, it makes appear a "^H"), and you must use the Delete
 * key instead.  The bs2Del treats a Backspace key pressed as if the
 * Delete key was pressed, in the current connection.
 */
typedef struct {
    char host[HOST_NAME_LEN];	/* host connected to */
    char port[PORT_NAME_LEN];	/* service connected to */
    char protocol[PROTOCOL_NAME_LEN]; /* protocol to use on connection */
    char term[TERM_NAME_LEN];	/* terminal type used for session */
    char user[USER_NAME_LEN];	/* user name used for rlogin */
    BOOL bs2Del;		/* Fred */
    char charset [CHARSET_NAME_LEN];  /* LZsiga 2002.01.02 */
    char answerback [ANSWERBACK_LEN]; /* LZsiga 2005.01.24 */
    short abacklen;                   /* LZsiga 2005.01.24 */
    BOOL locexec;		      /* LZsiga 2009.03.13 */
    unsigned char keypadMode [2];	/* see term.h:Terminal.keypadMode */
    BOOL vtkeymap;		/* LZsiga 2014.03.26 */
} Connect;

#define EmptyConnect {\
    "", "", "", "", "",\
    FALSE, "", "", 0,\
    FALSE, {KPM_PRESET_FLAG, KPM_PRESET_FLAG}, FALSE}

typedef void connectDlgExitProc();     /* Exit callback from the dialog */

#define MAX_HISTORY 5		/* maximum connections to remember */

#define MAX_HISTORY_SIZE 640

/* Return formatted session from 'connect'
   the returned string has to be freed
 */
char *connectCmdLine (const Connect *connect, BuffData *into);

/* Return formatted session from history */
char *connectGetHistory(char to[MAX_HISTORY_SIZE], int idx, BOOL forMenu);
/* Remember terminal type used in current session */
void connectRememberTerm(const char* termName);

/* Connect to current host/session */
void connectOpenHost(void);
/* Connect to specified session in history */
void connectHistory(int idx);

char* connectGetHost(void);
char* connectGetPort(void);
char* connectGetProtocol(void);
/* Return terminal emulation of current session */
char* connectGetTerm(void);
/* Return the user name in the current session */
char* connectGetUser(void);
/* Fred. Return the bs2Del flag in the current session */
BOOL connectGetBs2DelOption(void);
BOOL connectGetVtKeyMapOption(void);
/* Return Server CharacterSet */
char *connectGetServerCharSet(void);
/* Set Server CharacterSet */
void connectSetServerCharSet(char *set);

/* Return the state of exit-on-disconnect */
BOOL connectGetExitOnDisconnect(void);
/* Display the connect dialog */
void showConnectDialog(HINSTANCE instance, HWND wnd);
/* Toggle the state of exit-on-disconnect */
void connectToggleExitOnDisconnect(void);

/* Set current session parameters from the command line
 */
void connectSetTerm(const char* termName);
void connectSetEmul(const char* emulName);
void connectSetHost(const char* hostName);
void connectSetPort(const char* portName);
void connectSetUser(const char* userName);
void connectSetBs2Del(BOOL b);
void connectSetVtKeyMap(BOOL b);
void connectSetProtocol(const char* protocolName);

void connectSetAnswerBack(const char *abackStr);
int  connectSetKeypadMode(const char *kpam);

int  connectSetKeypadModeTo (Connect *to, const char *kpam);

const char *connectCodeKpamValue (const Connect *conn, char buff[16]);

/* Load connection variables from preset */
void connectLoadVars(Connect *connVars, OptSource src);
/* Copy current connect dialog variables */
void connectGetVars(Connect *connVars);
/* Copy default connect dialog variables */
void connectGetDefVars(Connect *connVars);

/* Initialize connect dialog controls from a Connect structure */
void connectDlgSetVars(HWND dlg, const Connect *conn);
/* Save dialog controls values into a Connect structure */
void connectDlgGetVars(HWND dlg, Connect *conn);
/* hostname has been changed */
void connectDlgOnHostChange(HWND dlg);
/* port has been changed */
void connectDlgOnPortChange(HWND dlg);
/* protocol has been changed */
void connectDlgOnProtocolChange(HWND dlg);

/* Load the connection history/parameters from the .INI file */
void connectGetProfile(void);
/* Save the connection history/parameters to the .INI file */
void connectSaveProfile(void);
/* connectReleaseResources: Separated from connectSaveProfile */
void connectReleaseResources(void);

#endif
