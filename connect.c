/* connect.c
 * Copyright (C) 1998 David Cole
 *
 * Manage connect dialog, connection history, and connect parameters.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "platform.h"
#include "resource.h"
#include "utils.h"
#include "emul.h"
#include "connect.h"
#include "argv.h"
#include "dtelnet.h"
#include "socket.h"
#include "term.h"
#include "raw.h"
#include "dialog.h"
#include "status.h"
#include "dtchars.h"

#ifndef strcasecmp
#define strcasecmp strcmpi /* sigh */
#endif

extern EmulNames emuls;

static BOOL haveDialog;		/* have we created the Connect dialog? */
/* static connectDlgExitProc DlgOnExit;    exit */

static struct {
    Connect* history;		/* connection history */
    int historySize;		/* number of entries in the history */
    int activeHistoryIdx;	/* current history entry in use */
} hist = {
    NULL,
    0,
    -1
};

static const Connect defVars = {
    "",				/* default host (none) */
    "telnet",			/* default port */
    "telnet",			/* default protocol */
    "xterm",                    /* default terminal emulation -- 20140327.LZS old: linux; new: xterm */
    "",				/* default user (none) */
    FALSE,			/* default bs2del state */
    "UTF8",			/* default server charset */
    "dtelnet from Dave Cole",   /* answerback */
    22,                         /* length of answerback */
    FALSE,			/* local execute disabled */
    {KPM_PRESET_FLAG, KPM_PRESET_FLAG},
				/* keypad application mode is numeric */
    FALSE			/* no vt kepmap */
};				/* default variables */
static Connect dlgVars;		/* current dialog variables */
static OptSource dlgVarsSource= OptFromDefault;
static BOOL exitOnDisconnect;	/* Exit when the socket disconnects? */

extern struct Protocols protocols[];
extern unsigned int numProtocols;

/* Identifiers used in the .INI file that are related to connections.
 */
static const char connectStr[] = "Connect";
static const char hostStr[] = "Host";
static const char portStr[] = "Port";
static const char protocolStr[] = "Protocol";
static const char termStr[] = "Term";
static const char userStr[] = "User";
static const char numStr[] = "History Len";
static const char historyStr[] = "History %d";
static const char exitStr[] = "Exit On Disconnect";
static const char abackStr [] = "AnswerBack";
static const char kpamStr [] = "Kpam";
/* Fred */
static const char bs2DelStr[] = "BackspaceToDelete";
static const char charsetStr[] = "ServerCharSet";
static const char vtKeyMapStr[] = "VtKeyMap";

/* Add the dlgVars session to the connection history.  If the session
 * is already in the history, do nothing.  When this is a new session,
 * add it to the top.
 */
static void addConnectHistory(void)
{
    int idx;			/* count sessions */
    Connect* connect;		/* session iterator */

    /* If already in the history - refresh it
     */
    for (idx = 0, connect = hist.history; idx < hist.historySize; ++idx, ++connect) {
	if (stricmp(connect->host, dlgVars.host) == 0
	    && stricmp(connect->port, dlgVars.port) == 0) {
	    memcpy(connect, &dlgVars, sizeof(*connect));
	    return;
	}
    }
    /* Add to head of the history
     */
    if (hist.history == NULL) {
	hist.history = (Connect *)xmalloc(MAX_HISTORY * sizeof *hist.history);
	hist.historySize = 1;
    } else {
	if (hist.historySize != MAX_HISTORY)
	    ++hist.historySize;
	memmove(&hist.history[1], &hist.history[0],
		(hist.historySize - 1) * sizeof *hist.history);
    }

    hist.history[0] = dlgVars;
}

const char *connectCodeKpamValue (const Connect *conn, char buff[16])
{
    termKeypadModeStr (conn->keypadMode, buff);

    return buff;
}

/* the returned pointer has to be freed */
char *connectCmdLine (const Connect *connect, BuffData *into)
{
    char buff [512], *p= buff;
    BuffData myinto;

    if (!connect) connect= &dlgVars;
    if (!into)    into= &myinto;

	/* Format for .INI file, eg. "/Hterm1 /Ptelnet /Stelnet /Tlinux /Exterm"
	 */
    p += sprintf(p, "/H%s /P%s /S%s /T%s",
		connect->host, connect->port,
		connect->protocol, connect->term);
    if (connect->user[0] != '\0')
	p += sprintf(p, " /U%s", connect->user);

    if (connect->bs2Del) {
	p= stpcpy(p, " /D");
    }

    p += sprintf (p, " /C%s", connect->charset);

    p += sprintf (p, " /%s=", abackStr);
    *p++ = '"';
    if (connect->abacklen) {
	p += escape (connect->answerback, connect->abacklen,
		     p, 128);
    }
    *p++ = '"';

    {
	char tmpbuff [64];
	const char *q;
	
	q= connectCodeKpamValue (connect, tmpbuff);
	if (q) p += sprintf (p, " /Kpam=%s", q);
    }

    if (connect->vtkeymap) {
	p= stpcpy(p, " /VtKeyMap");
    }
	
    *p = '\0';

    BuffDupS (into, buff);

    return into->ptr;
}

/* Format the specified session and return result as string
 *
 * Args:
 * idx -     index of session to be formatted
 * forMenu - format as menu item?
 *
 * Return pointer to static formatted string.
 */
char *connectGetHistory(char format[MAX_HISTORY_SIZE], int idx, BOOL forMenu)
{
    const Connect* connect;		/* session to be formatted */

    if (idx >= hist.historySize)
	return NULL;
    connect = &hist.history[idx];

    if (!connect || !format) {
	fprintf (stderr, "Breakpoint for debugger\n");
    }

    if (forMenu) {
	/* Format for menu entry, eg. "&1 term1/telnet"
	 */
	sprintf(format, "&%d %s/%s",
		idx + 1,
		connect->host, connect->port);
    } else {
	/* Format for .INI file, eg. "/Hterm1 /Ptelnet /Stelnet /Tlinux /Exterm"
	 */
	char *tmp= connectCmdLine (connect, NULL);
	
	strcpy (format, tmp);
	xfree (tmp);
    }
    return format;
}

/* Remember the terminal type used with the current session, if any
 *
 * Args:
 * termName - terminal type used
 */
void connectRememberTerm(const char* termName)
{
    Connect* active;

    if (hist.activeHistoryIdx < 0
	|| hist.activeHistoryIdx >= hist.historySize)
	return;

    active = hist.history + hist.activeHistoryIdx;
    strncpy(active->term, termName, sizeof(active->term));
    active->term[sizeof(active->term) - 1] = '\0';
}

/* Open a connection to the host/port currently specified in the dialog
 */
void connectOpenHost()
{
    int rc;
    Emul *emul;

    socketConnect(dlgVars.host, dlgVars.port);
    /* Reset the application window title
     */
    termSetTitle("", DTCHAR_ASCII);
    /* Set connection options
     */
    emulSetTerm(dlgVars.term);
    emulResetTerminal (TRUE);
    emul= emulGetTerm ();

    rc= emulSetCharset (emul, dlgVars.charset);
    if (rc) strcpy (dlgVars.charset, "ANSI");

    emulSetAnswerBack (dlgVars.abacklen, dlgVars.answerback);
    emulSetExecuteFlag (dlgVars.locexec);
    emulSetKeypadMode (dlgVars.keypadMode);

    dtcEC_CheckMenuItem(dlgVars.charset);
    rawEnableProtocol(findPortProtocol(dlgVars.protocol));
    rawSetTerm(emul->servname[0]? emul->servname: emul->name);
    /* Indicate the terminal type emulated
     */
    statusSetTerm();
}

/* Connect to the specified historic session
 *
 * Args:
 * idx - index of historic session to connect to
 */
void connectHistory(int idx)
{
    if (idx >= hist.historySize)
	return;
    dlgVars = hist.history[idx];
    dlgVarsSource= OptFromIniFile;
    hist.activeHistoryIdx = idx;
    connectOpenHost();
}

char* connectGetHost()
{
    return dlgVars.host;
}
char* connectGetPort()
{
    return dlgVars.port;
}
char* connectGetProtocol()
{
    return dlgVars.protocol;
}
/* Return the terminal type of the current session
 */
char* connectGetTerm()
{
    return dlgVars.term;
}

/* Return the user name in the current session
 */
char* connectGetUser()
{
    return dlgVars.user;
}

/* Fred. Return the bs2Del option in the current session
 */
BOOL connectGetBs2DelOption()
{
    return dlgVars.bs2Del;
}

BOOL connectGetVtKeyMapOption()
{
    return dlgVars.vtkeymap;
}

/* Return server-character-set */
char *connectGetServerCharSet(void)
{
    return dlgVars.charset;
}

/* Set Server CharacterSet */
void connectSetServerCharSet(char *set)
{
    strcpy (dlgVars.charset, set);
}

/* Return the state of exit-on-disconnect
 */
BOOL connectGetExitOnDisconnect()
{
    return exitOnDisconnect;
}

/* Initialize connect dialog controls from a Connect structure
 */
void connectDlgSetVars(HWND dlg, const Connect *conn)
{
    LRESULT res;
    unsigned int idx;
    int chkitem;
    const char *proto;

    SetDlgItemText(dlg, IDC_HOSTNAME, conn->host);
    SetDlgItemText(dlg, IDC_PORT, conn->port);
    for (idx = 0; idx < numProtocols; idx++) {
	SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_ADDSTRING, 0, (LPARAM)protocols[idx].name);
    }
    proto= conn->protocol;
    if (*proto=='\0') proto= protocols[protoNone].name;
    res = SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPSTR)proto);
    if (res != CB_ERR) {
	SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_SETCURSEL, (WPARAM) res, 0);
    }
    EnableWindow(GetDlgItem(dlg, IDC_USER), stricmp(proto, protocols[protoRlogin].name) == 0);

    telnetEnumTermProfiles(&emuls);
    for (idx = 0; idx < emuls.num; idx++) {
	SendDlgItemMessage(dlg, IDC_TERM, CB_ADDSTRING, 0, (LPARAM)emuls.names[idx]);
    }
    res = SendDlgItemMessage(dlg, IDC_TERM, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)(LPSTR)conn->term);
    if (res != CB_ERR) {
	SendDlgItemMessage(dlg, IDC_TERM, CB_SETCURSEL, (WPARAM)res, 0);
    }
    SetDlgItemText(dlg, IDC_USER, conn->user);
    /* Fred */
    SendDlgItemMessage(dlg, IDC_BS2DEL, BM_SETCHECK, conn->bs2Del, 0);
    SendDlgItemMessage(dlg, ID_TERMINAL_VT_KEY_MAP, BM_SETCHECK, conn->vtkeymap, 0);

    dtcEC_NamesToCombo(GetDlgItem(dlg, IDC_CHARSET));
    SendDlgItemMessage(dlg, IDC_CHARSET, CB_SELECTSTRING, 0, (LPARAM)(LPSTR)conn->charset);
    SendDlgItemMessage(dlg, IDC_LOCEXEC, BM_SETCHECK, conn->locexec, 0);

    if (!(conn->keypadMode[0]&KPM_PRESET_FLAG))    chkitem= IDC_KPAM0_VAR;
    else if (conn->keypadMode[0]&KPM_PRESET_APPLI) chkitem= IDC_KPAM0_APPL;
    else					   chkitem= IDC_KPAM0_NUM;
    CheckRadioButton (dlg, IDC_KPAM0_NUM, IDC_KPAM0_VAR, chkitem);

    if (!(conn->keypadMode[1]&KPM_PRESET_FLAG))    chkitem= IDC_KPAM1_VAR;
    else if (conn->keypadMode[1]&KPM_PRESET_APPLI) chkitem= IDC_KPAM1_APPL;
    else					   chkitem= IDC_KPAM1_NUM;
    CheckRadioButton (dlg, IDC_KPAM1_NUM, IDC_KPAM1_VAR, chkitem);
}

/* Save dialog controls values into a Connect structure
 */
void connectDlgGetVars(HWND dlg, Connect *conn)
{
    LRESULT res;
    int i;

    GetDlgItemText(dlg, IDC_HOSTNAME, conn->host, HOST_NAME_LEN);
    GetDlgItemText(dlg, IDC_PORT, conn->port, PORT_NAME_LEN);
    res = SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_GETCURSEL, 0, 0);
    if (res != CB_ERR) {
	SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_GETLBTEXT, (WPARAM)res, (LPARAM)(LPSTR)conn->protocol);
    }
    res = SendDlgItemMessage(dlg, IDC_TERM, CB_GETCURSEL, 0, 0);
    if (res != CB_ERR) {
	SendDlgItemMessage(dlg, IDC_TERM, CB_GETLBTEXT, (WPARAM)res, (LPARAM)(LPSTR)conn->term);
    }
    GetDlgItemText(dlg, IDC_USER, conn->user, USER_NAME_LEN);
    /* Fred */
    conn->bs2Del = (BOOL)SendDlgItemMessage(dlg, IDC_BS2DEL, BM_GETCHECK, 0, 0);
    conn->vtkeymap = (BOOL)SendDlgItemMessage(dlg, ID_TERMINAL_VT_KEY_MAP, BM_GETCHECK, 0, 0);

    res = SendDlgItemMessage(dlg, IDC_CHARSET, CB_GETCURSEL, 0, 0);
    if (res != CB_ERR) {
	SendDlgItemMessage(dlg, IDC_CHARSET, CB_GETLBTEXT, (WPARAM)res, (LPARAM)(LPSTR)conn->charset);
    }
    conn->locexec = (BOOL)SendDlgItemMessage(dlg, IDC_LOCEXEC, BM_GETCHECK, 0, 0);

    for (i=0; i<2; ++i) {
	static const unsigned char KpamRadioValues [3] = {KPM_PRESET_FLAG, KPM_PRESET_FLAG | KPM_PRESET_APPLI, 0};
	int j, found;

	for (found=-1, j= 0; j<3; ++j) {
	    if (IsDlgButtonChecked (dlg, IDC_KPAM0_NUM + 3*i + j))
		found= j;
	}
	if (found==-1) found= 0;	/* default: preset to numeric */
	conn->keypadMode[i]= KpamRadioValues[found];
    }
}

/* hostname has been changed
 */
void connectDlgOnHostChange(HWND dlg)
{
    BOOL enable = FALSE;
    char host[HOST_NAME_LEN];
    char port[PORT_NAME_LEN];

    GetDlgItemText(dlg, IDC_HOSTNAME, host, sizeof(host));
    GetDlgItemText(dlg, IDC_PORT, port, sizeof(port));
    if (isNonBlank(host) && isNonBlank(port)) {
	enable = TRUE;
    }
    EnableWindow(GetDlgItem(dlg, IDOK), enable);
}

/* port has been changed
 */
void connectDlgOnPortChange(HWND dlg)
{
    char port[PORT_NAME_LEN];

    GetDlgItemText(dlg, IDC_PORT, port, sizeof(port));
    switch (findPortProtocol(port)) 
    {
    case protoRlogin:
	SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_SETCURSEL, (WPARAM)protoRlogin, 0);
	EnableWindow(GetDlgItem(dlg, IDC_USER), TRUE);
	break;
    case protoTelnet:
	SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_SETCURSEL, (WPARAM)protoTelnet, 0);
	EnableWindow(GetDlgItem(dlg, IDC_USER), FALSE);
	break;
    default:
	SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_SETCURSEL, (WPARAM)protoNone, 0);
	EnableWindow(GetDlgItem(dlg, IDC_USER), FALSE);
	break;
    }
    connectDlgOnHostChange(dlg);
}

/* protocol has been changed
 */
void connectDlgOnProtocolChange(HWND dlg)
{
    LRESULT res;
    char protocol[PROTOCOL_NAME_LEN];

    res = SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_GETCURSEL, 0, 0);
    if (res == CB_ERR) {
	return;
    }
    SendDlgItemMessage(dlg, IDC_PROTOCOL, CB_GETLBTEXT, (WPARAM)res, (LPARAM)(LPSTR)protocol);
    EnableWindow(GetDlgItem(dlg, IDC_USER), stricmp(protocol, protocols[protoRlogin].name) == 0);
}

/* Dialog procedure for the Connect dialog
 */
static DIALOGRET CALLBACK connectDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_INITDIALOG:
	/* Flag that we have created the dialog and register the
	 * dialog window handle.
	 */
	haveDialog = TRUE;
	dialogRegister(dlg);
	/* Initialise the dialog controls
	 */
	connectDlgSetVars(dlg, &dlgVars);
	return TRUE;

    case WM_COMMAND:
	switch (LOWORD(wparam)) {
	case IDOK:
	    /* Read all dialog controls into the current session
	     */
	    connectDlgGetVars(dlg, &dlgVars);
	    dlgVarsSource= OptFromDialog;

	    /* Add the session to the history
	     */
	    addConnectHistory();
	    hist.activeHistoryIdx = 0;
	    /* initiate the connection.
	     */
	    connectOpenHost();

	    /* Fall through and destroy dialog
	     */

	case IDCANCEL:
	    /* Destroy the dialog and unregister the dialog handle.
	     */
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    haveDialog = FALSE;
	    return TRUE;

	case IDC_PROTOCOL:
#ifdef WIN32
	    if (HIWORD(wparam) != CBN_SELCHANGE)
		break;
#else
	    if (HIWORD(lparam) != CBN_SELCHANGE)
		break;
#endif
	    connectDlgOnProtocolChange(dlg);
	    break;

	case IDC_PORT:
#ifdef WIN32
	    if (HIWORD(wparam) != EN_CHANGE)
		break;
#else
	    if (HIWORD(lparam) != EN_CHANGE)
		break;
#endif
	    connectDlgOnPortChange(dlg);
	    break;

	case IDC_HOSTNAME:
#ifdef WIN32
	    if (HIWORD(wparam) != EN_CHANGE)
		break;
#else
	    if (HIWORD(lparam) != EN_CHANGE)
		break;
#endif
	    connectDlgOnHostChange(dlg);
	    break;
	}
	break;
    }
    return FALSE;
}

/* Display the Connect dialog
 */
void showConnectDialog(HINSTANCE instance, HWND wnd)
{
    if (!haveDialog)
    {
	 CreateDialog(instance, MAKEINTRESOURCE(IDD_CONNECT_DIALOG),
		     wnd, connectDlgProc);
    }
}

/* Toggle the state of exit-on-disconnect
 */
void connectToggleExitOnDisconnect()
{
    exitOnDisconnect = !exitOnDisconnect;
}

/* Set the target host from the command line.  Do not use .INI
 * settings for connections if driven via command line.
 */
void connectSetHost(const char* host)
{
    strncpy(dlgVars.host, host, sizeof(dlgVars.host));
    dlgVars.host[sizeof(dlgVars.host) - 1] = '\0';
}

/* Set the target port from the command line.  Do not use .INI
 * settings for connections if driven via command line.
 */
void connectSetPort(const char* portName)
{
    strncpy(dlgVars.port, portName, sizeof(dlgVars.port));
    dlgVars.port[sizeof(dlgVars.port) - 1] = '\0';
    strncpy(dlgVars.protocol, portName, sizeof(dlgVars.protocol));
    dlgVars.protocol[sizeof(dlgVars.protocol) - 1] = '\0';
}

/* Set the port protocol from the command line.  Do not use .INI
 * settings for connections if driven via command line.
 */
void connectSetProtocol(const char* protocolName)
{
    strncpy(dlgVars.protocol, protocolName, sizeof(dlgVars.protocol));
    dlgVars.protocol[sizeof(dlgVars.protocol) - 1] = '\0';
}

/* Set the terminal emulation from the command line.  Do not use .INI
 * settings for connections if driven via command line.
 */
void connectSetTerm(const char* termName)
{
    strncpy(dlgVars.term, termName, sizeof(dlgVars.term));
    dlgVars.term[sizeof(dlgVars.term) - 1] = '\0';
}

/* Set the remote user for rlogin from the command line.  Do not use
 * .INI settings for connections if driven via command line.
 */
void connectSetUser(const char* userName)
{
    strncpy(dlgVars.user, userName, sizeof(dlgVars.user));
    dlgVars.user[sizeof(dlgVars.user) - 1] = '\0';
}

/* Fred. Set the bs2Del option from the command line.  Do not use .INI
 * settings for connections if driven via command line.
 */

 void connectSetBs2Del(BOOL b)
{
    dlgVars.bs2Del = b;
}

void connectSetVtKeyMap(BOOL b)
{
    dlgVars.vtkeymap = b;
}
 
void connectSetAnswerBack(const char *abackStr)
{
    dlgVars.abacklen =
	(short)unescape (abackStr, strlen (abackStr),
			 dlgVars.answerback, sizeof (dlgVars.answerback));
}

typedef struct DecodeKeypadTypeOld {
    const char *name;
    unsigned char value[2];
} DecodeKeypadTypeOld;

static const DecodeKeypadTypeOld DecodeKeypadOld [] = {
    {"",
	{KPM_PRESET_FLAG,
	 KPM_PRESET_FLAG}}, /* default */
    {"All",
	{KPM_PRESET_FLAG | KPM_PRESET_APPLI,
	 KPM_PRESET_FLAG | KPM_PRESET_APPLI}},
    {"Disabled",
	{KPM_PRESET_FLAG, 
	 KPM_PRESET_FLAG}},
    {"Enabled",  {0, 0}},
    {"Fkeys",
	{KPM_PRESET_FLAG | KPM_PRESET_APPLI,
	 0}}
};

#define NDecodeKeypadOld (sizeof (DecodeKeypadOld)/sizeof (DecodeKeypadOld[0]))

int connectSetKeypadMode (const char *kpam)
{
    return connectSetKeypadModeTo (&dlgVars, kpam);
}

static unsigned char connectDecodeKpamCode (const char code)
{
    unsigned char retval;

    if      (code=='N') retval= KPM_PRESET_FLAG;
    else if (code=='A') retval= KPM_PRESET_FLAG | KPM_PRESET_APPLI;
    else		retval= 0;

    return retval;
}

int connectSetKeypadModeTo (Connect *to, const char *kpam)
{
    int rc, i;
    const DecodeKeypadTypeOld *found;

    to->keypadMode[0]= KPM_PRESET_FLAG;
    to->keypadMode[1]= KPM_PRESET_FLAG;

    if (!kpam || !*kpam) {
	rc= 0;
	goto RETURN;
    }

    if (kpam[1]=='\0') { /* new style, one character */
	to->keypadMode[0]= connectDecodeKpamCode (kpam[0]);
	to->keypadMode[1]= to->keypadMode[0];
	rc= 0;
	goto RETURN;

    } else if (kpam[2]=='\0') { /* new style, two characters */
	to->keypadMode[0]= connectDecodeKpamCode (kpam[0]);
	to->keypadMode[1]= connectDecodeKpamCode (kpam[1]);
	rc= 0;
	goto RETURN;
    }
    
    for (i=0, found=NULL; !found && i<(int)NDecodeKeypadOld; ++i) {
	const DecodeKeypadTypeOld *tp= &DecodeKeypadOld[i];

	if (strcasecmp (kpam, tp->name)==0)
	    found= tp;
    }
    if (found) {
	to->keypadMode[0]= found->value[0];
	to->keypadMode[1]= found->value[1];
	rc= 0;

    } else {
	rc= -1;
    }

RETURN:
    return rc;
}

/* Use variables loaded from preset file */
void connectLoadVars(Connect *connVars, OptSource src){
    memcpy(&dlgVars, connVars, sizeof(dlgVars));
    dlgVarsSource= src;
}

/* Copy current connect dialog variables */
void connectGetVars(Connect *connVars) {
    memcpy(connVars, &dlgVars, sizeof(dlgVars));
}

/* Copy default connect dialog variables */
void connectGetDefVars(Connect *connVars) {
    memcpy(connVars, &defVars, sizeof(dlgVars));
}

/* Load the connection history/parameters from the .INI file
 */
void connectGetProfile()
{
    int num;			/* Number of sessions in .INI file */
    int idx;			/* Iterate over sessions */
    char tmp [256];
    const char *tifile;
/* if you have to reorder options, change OPTION_**** too */
/*                              1 2 3 4 5 67  8            9*/
static const char options [] = "H:P:S:T:U:DC:(AnswerBack):(Kpam):(VtKeyMap):";
#define OPTION_ANSWERBACK (GETOPT_LONGOPT+8)
#define OPTION_KPAM       (GETOPT_LONGOPT+9)
#define OPTION_VTKEYMAP   (GETOPT_LONGOPT+10)
    OptRet opt;

    tifile = telnetIniFile ();
    /* Get value of exit-on-disconnect
     */
    exitOnDisconnect = GetPrivateProfileBoolean(telnetIniFile(), connectStr, exitStr, FALSE);

    /* Get number of sessions in the .INI file, then load them in
     * reverse order.
     */
    num = GetPrivateProfileInt(connectStr, numStr, 0, tifile);
    for (idx = num -1; idx >= 0; --idx) {
	char id[32];		/* format .INI file session-id */
	char value[256];	/* formatted .INI file entry */
	const char* argv[32];	/* parse entry as argc/argv */
	int argc;		/* number of session arguments */
	int c;			/* iterate over argc/argv */

	/* Get the next formatted session from the .INI file.
	 */
	sprintf(id, historyStr, idx);
	GetPrivateProfileString(connectStr, id, "", value,
				sizeof(value), tifile);
	if (value[0] == '\0')
	    break;

	/* Parse session into current session
	 */
	memset(&dlgVars, 0, sizeof(dlgVars));
	strcpy(dlgVars.charset, "ANSI");

	argc = getoptInit(value, (char **)argv, 32);

	getoptStart(argc, argv, options);
	while ((c = getopt(&opt)) != EOF)
	    switch (c) {
	    case 'H':
		strcpy(dlgVars.host, opt.optarg);
		break;
	    case 'P':
		strcpy(dlgVars.port, opt.optarg);
		break;
	    case 'S':
		strcpy(dlgVars.protocol, opt.optarg);
		break;
	    case 'T':
		strcpy(dlgVars.term, opt.optarg);
		break;
	    case 'U':
		strcpy(dlgVars.user, opt.optarg);
		break;
	    case 'D':
		/* Fred */
		dlgVars.bs2Del = TRUE;
		break;
	    case 'C':
		strcpy(dlgVars.charset, opt.optarg);
		break;
	    case OPTION_ANSWERBACK:
		connectSetAnswerBack (opt.optarg);
		break;
	    case OPTION_KPAM:
		connectSetKeypadMode (opt.optarg);
		break;
	    case OPTION_VTKEYMAP:
		dlgVars.vtkeymap= TRUE;
		break;
	    }
	/* Restore session to history
	 */
	if (dlgVars.host[0] != '\0'
	    && dlgVars.port[0] != '\0'
	    && dlgVars.term[0] != '\0')
	    addConnectHistory();
    }

    /* Get current session parameters
     */
    GetPrivateProfileString(connectStr, hostStr, defVars.host, dlgVars.host,
			    sizeof(dlgVars.host), tifile);
    GetPrivateProfileString(connectStr, portStr, defVars.port, dlgVars.port,
			    sizeof(dlgVars.port), tifile);
    GetPrivateProfileString(connectStr, protocolStr, defVars.protocol,
			    dlgVars.protocol, sizeof(dlgVars.port),
			    tifile);
    GetPrivateProfileString(connectStr, termStr, defVars.term, dlgVars.term,
			    sizeof(dlgVars.term), tifile);
    GetPrivateProfileString(connectStr, userStr, defVars.user, dlgVars.user,
			    sizeof(dlgVars.user), tifile);

    dlgVars.bs2Del   = GetPrivateProfileBoolean (tifile, connectStr, bs2DelStr,   defVars.bs2Del);
    dlgVars.vtkeymap = GetPrivateProfileBoolean (tifile, connectStr, vtKeyMapStr, defVars.vtkeymap);

    GetPrivateProfileString(connectStr, charsetStr, defVars.charset, dlgVars.charset,
			    sizeof(dlgVars.charset), tifile);

    GetPrivateProfileString(connectStr, abackStr, defVars.answerback, tmp, sizeof (tmp),
			    tifile);
    connectSetAnswerBack (tmp);

    GetPrivateProfileString(connectStr, kpamStr, "", tmp, sizeof (tmp),
			    tifile);
    connectSetKeypadMode(tmp);
}

/* Save the connection history/parameters to the .INI file
 */
void connectSaveProfile()
{
    const char *tofile;
    char tmp [256];
    const char *p;
    char str[20];		/* format .INI file entry */
    int idx;		/* iterate over history */

    if (dlgVarsSource==OptFromTsFile) { /* for now, we don't save to the *.ts file */
	return;
    }

    tofile= telnetIniFile();		

	/* Save number of history sessions
	 */
	sprintf(str, "%d", hist.historySize);
	WritePrivateProfileString(connectStr, numStr, str, telnetIniFile());
	/* Format and save all history sessions
	 */
	for (idx = 0; idx < hist.historySize; ++idx) {
	    char id[32], value[MAX_HISTORY_SIZE];
	    connectGetHistory(value, idx, FALSE);

	    sprintf(id, historyStr, idx);
	    WritePrivateProfileString(connectStr, id, value, tofile);
	}

	/* Save current session parameters
	 */
	WritePrivateProfileString(connectStr, hostStr,
				  dlgVars.host, tofile);
	WritePrivateProfileString(connectStr, portStr,
				  dlgVars.port, tofile);
	WritePrivateProfileString(connectStr, protocolStr,
				  dlgVars.protocol, tofile);
	WritePrivateProfileString(connectStr, termStr,
				  dlgVars.term, tofile);
	WritePrivateProfileString(connectStr, userStr,
				  dlgVars.user, tofile);

	WritePrivateProfileBoolean(tofile, connectStr, exitStr,     exitOnDisconnect);
	WritePrivateProfileBoolean(tofile, connectStr, bs2DelStr,   dlgVars.bs2Del);
	WritePrivateProfileBoolean(tofile, connectStr, vtKeyMapStr, dlgVars.vtkeymap);

	WritePrivateProfileString (connectStr, charsetStr, dlgVars.charset, tofile);

	escape (dlgVars.answerback, dlgVars.abacklen, tmp, sizeof (tmp));
	WritePrivateProfileString(connectStr, abackStr, tmp, tofile);

	p= connectCodeKpamValue (&dlgVars, tmp);
	if (!p) p= "";
	WritePrivateProfileString(connectStr, kpamStr, p, tofile);
}

void connectReleaseResources (void)
{
    /* Free history memory
     */
    if (hist.history != NULL) {
	free (hist.history);
	memset (&hist, 0, sizeof hist);
    }
}
