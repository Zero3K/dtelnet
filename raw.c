/* raw.c
 * Copyright (c) 1997 David Cole
 *
 * Strip off and process raw protocol from session
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "utils.h"
#define TELNET_STRINGS
#include "protocol.h"
#include "emul.h"
#include "connect.h"
#include "socket.h"
#include "term.h"
#include "raw.h"
#include "log.h"
#include "envvars.h"
#include "buffdata.h"

/* Describe the different protocol types we can use on the connection.
 * The order of the entries in this array is important - the index of
 * array must match the RawProtocol enum defined in raw.h
 */
struct Protocols protocols[] = {
    { "telnet", 23, protoTelnet },
    { "login", 513, protoRlogin },
    { "none", -1, protoNone }
};
unsigned int numProtocols = 3;

static BOOL ignoreTextUntilDM;	/* processing TELNET Synch */

static RawProtocol rawProtocol;	/* which protocol are we using? */

static BOOL startSession;	/* we have just started a new telnet
				 * session */

static BOOL dontNAWS;		/* do not send NAWS any more */
static BOOL doneNAWS;		/* have done NAWS exchange */
static BOOL doneSGA;		/* have done the SGA exchange */
static BOOL doneLFLOW;		/* have done the LFLOW exchange */

static BOOL enableBINARY;	/* binary mode negotiation is disabled
				 * by default */
static BOOL doneBINARY;		/* have done the BINARY exchange */
static BOOL inBINARY;		/* are we in binary mode? */

static BOOL compressResize;	/* wait until user has finished
				 * resizing before sending new size to
				 * server? */
static BOOL needResize;		/* is a user resize queued? */

static BOOL fDoSgaSent;         /* TRUE if I've already sent DO SGA */

static struct {
    BOOL inCommand;		/* are we reading a telnet command? */
    Buffer cmd;			  /* build up telnet command */
    TelnetLocalOption toXdisploc; /* XDISPLOC telnet option */
    TelnetLocalOption toNewEnv;   /* New environment telnet option */
} rawvar;

/* Enable telnet BINARY mode negotiation
 */
void rawEnableBinaryMode()
{
    enableBINARY = TRUE;
}

/* Return whether or not we are in telnet BINARY mode
 */
BOOL rawInBinaryMode()
{
    return inBINARY;
}

/* From rfc1258.  As soon as the connection is established, we send a
 * message of the form:
 *  <null>
 *  client-user-name<null>
 *  server-user-name<null>
 *  terminal-type/speed<null>
 */
static void rawRloginStart(void)
{
    char msg[64];	/* build message here */
    int idx;			/* current position in msg */
    char* userName = connectGetUser(); /* user to client/server user-name */
    char* term = connectGetTerm(); /* terminal type */
    char* str;			/* copy strings into msg */
    int p2offset;		/* index of parameter-2 (server-user-name) */
    int p3offset;		/* index of parameter-3 */

    /* Copy in initial null
     */
    msg[0] = 0;
    /* Copy in client-user-name + null
     */
    for (idx = 1, str = userName; *str; ++idx, ++str)
	msg[idx] = *str;
    msg[idx++] = '\0';
    /* Copy in server-user-name + null
     */
    p2offset = idx;
    for (str = userName; *str; ++idx, ++str)
	msg[idx] = *str;
    msg[idx++] = '\0';
    /* Copy in terminal-type
     */
    p3offset = idx;
    for (str = term; *str; ++idx, ++str)
	msg[idx] = *str;
    /* Copy in /speed (fixed at 9600) + null
     */
    for (str = "/9600"; *str; ++idx, ++str)
	msg[idx] = *str;
    msg[idx++] = '\0';

    /* Send message to remote end and log protocol
     */
    socketWrite(msg, idx);
    logProtocol("send \\0 %s\\0 %s\\0 %s\\0\n",
		msg + 1, msg + p2offset, msg + p3offset);
}

static char* telcmd(unsigned int cmd)
{
    static char buff[16];

    if (TELCMD_OK(cmd))
	return TELCMD(cmd);
    sprintf(buff, "0x%02x", cmd);
    return buff;
}

static char* telopt(unsigned char opt)
{
    static char buff[16];

    if (TELOPT_OK(opt))
	return TELOPT(opt);
    sprintf(buff, "0x%02x", opt);
    return buff;
}

/* Send a NOP telnet command to the server
 */
void rawNop(void)
{
    char msg[2];	/* format command */

    /* Build command, send and log it
     */
    msg[0] = (char)IAC;
    msg[1] = (char)NOP;
    socketWrite(msg, sizeof(msg));
    logProtocol("send NOP\n");
}


/* Send a telnet command to the server
 *
 * Args:
 * cmd - the telnet command to send
 * opt - option to send with the comand
 */
void rawCmd(unsigned char cmd, unsigned char opt)
{
    char msg[3];	/* format command */

    /* Build command, send and log it
     */
    msg[0] = (char)IAC;
    msg[1] = (char)cmd;
    msg[2] = (char)opt;
    socketWrite(msg, sizeof(msg));
    logProtocol("send %s %s\n", telcmd(cmd), telopt(opt));
}

/* Make sure we ask the telnet server about options that it did not
 * ask us about.
 */
static void rawTelnetStart(void)
{
    logProtocol("Client initiated options\n");
    ignoreTextUntilDM = FALSE;
    if (!doneNAWS) {
	/* Server did not ask us about NAWS, tell it we will NAWS
	 */
	rawCmd(WILL, TELOPT_NAWS);
	doneNAWS = TRUE;
    }
    if (!doneLFLOW) {
	/* Server did not ask us about LFLOW, tell it we will LFLOW
	 */
	rawCmd(WILL, TELOPT_LFLOW);
	socketSetLocalFlowControl(TRUE);
    }
    if (!doneSGA) {
	/* Tell the server not to send us GA commands.
	 *   TCP/IP Illustrated Volume 1 pg. 407
	 */
	rawCmd(DO, TELOPT_SGA);
	fDoSgaSent = TRUE;
    }

    /* Do not perform binary mode negotiation unless told to
     */
    if (enableBINARY && !doneBINARY) {
	rawCmd(DO, TELOPT_BINARY);
	doneBINARY = TRUE;
    }

    /* XDISPLOC -- if we think there is a X-server on the local computer */
    if (socketHaveXServer ()) {
	rawCmd(WILL, TELOPT_XDISPLOC);
	rawvar.toXdisploc = toIWPending;
    } else {
	rawvar.toXdisploc = toIDontWant;
    }

    /* NEWENV -- if we have any */
    if (!EV_IsEmpty (DefaultEnvVars)) {
	rawCmd(WILL, TELOPT_NEWENV);
	rawvar.toNewEnv = toIWPending;
    } else {
	rawvar.toNewEnv = toIDontWant;
    }
}

/* Perform initialisation for session on new connection
 */
void rawStartSession()
{
    /* Clear telnet state
     */
    rawvar.cmd.len = 0;
    dontNAWS = TRUE;
    doneNAWS = doneSGA = doneLFLOW = doneBINARY = FALSE;
    fDoSgaSent = FALSE;

    /* Flag that we have just started a new session
     */
    startSession = TRUE;
    /* Flag that we are not in BINARY mode
     */
    inBINARY = FALSE;

    if (rawProtocol == protoRlogin)
	/* When doing rlogin, we have to get the ball rolling
	 */
	rawRloginStart();
    else if (rawProtocol == protoTelnet)
	rawTelnetStart();
}

/* User has started resizing the window
 */
void rawResizeBegin()
{
    compressResize = TRUE;
}

/* User just finished resizing the window
 */
void rawResizeEnd()
{
    compressResize = FALSE;
    if (needResize)
	rawSetWindowSize();
}

/* Tell the telnet/rlogin server what our window size is
 */
void rawSetWindowSize()
{
    char msg[12];	/* format message */

    if (!socketConnected() || dontNAWS) { 
	/* not connected or
	 * the server does not want to receive window size
	 * messages
	 */
	needResize = FALSE;
	return;
    }

    if (compressResize) {
	needResize = TRUE;
	return;
    } else {
	needResize = FALSE;
    }

    termCheckSizeLimits(&term.winSize);	// Khader

    switch (rawProtocol) {
    case protoTelnet:
	/* Build telnet message for communicating window size
	 */
	msg[0] = (char)IAC;
	msg[1] = (char)SB;
	msg[2] = (char)TELOPT_NAWS;
	msg[3] = (char)(term.winSize.cx >> 8);
	msg[4] = (char)(term.winSize.cx % 0xff);
	msg[5] = (char)(term.winSize.cy >> 8);
	msg[6] = (char)(term.winSize.cy % 0xff);
	msg[7] = (char)IAC;
	msg[8] = (char)SE;
	/* Send and log message
	 */
	socketWrite(msg, 9);
	logProtocol("send SB NAWS %d %d IAC SE\n",
		    term.winSize.cx, term.winSize.cy);
	break;

    case protoRlogin:
	/* Build rlogin message for communicating window size
	 */
	msg[0] = (char)0xff;
	msg[1] = (char)0xff;
	msg[2] = 's';
	msg[3] = 's';
	msg[4] = (char)(term.winSize.cy >> 8);
	msg[5] = (char)(term.winSize.cy % 0xff);
	msg[6] = (char)(term.winSize.cx >> 8);
	msg[7] = (char)(term.winSize.cx % 0xff);
	msg[8] = msg[4];
	msg[9] = msg[5];
	msg[10] = msg[6];
	msg[11] = msg[7];

	/* Send and log message
	 */
	socketWrite(msg, 12);
	logProtocol("send 0xff 0xff ss %d %d %d %d\n",
		    term.winSize.cy, term.winSize.cx,
		    term.winSize.cy, term.winSize.cx);
	break;

    case protoNone:
	break;
	/* do nothing, only avoid gcc warning */
    }
}

/* Tell the telnet server what terminal we are using
 */
/* 20130902.LZS:

    rfc1091:
	The terminal type information is an NVT ASCII string.  Within this
	string, upper and lower case are considered equivalent.  The complete
	list of valid terminal type names can be found in the latest
	"Assigned Numbers" RFC [4].

    Actual AIX szerver:
	TELOPT_TTYOPE="RXVT" is converted to TERM=rxvt),
	TELOPT_TTYOPE="RXVT-256COLOR" stays TERM=RXVT-256COLOR -- wrong)

    Decision:
	don't capitalize any more (I guess it will raise other problems)
 */

void rawSetTerm(const char *termname)
{
    unsigned char msg[64];	/* build message */
    unsigned idx;		/* current position in message */
    const unsigned char *term= (unsigned char *)termname;
    
    if (!socketConnected()) {
	/* Do not send message if not connected
	 */
	return;
    }

    /* Build message
     */
    msg[0] = IAC;
    msg[1] = SB;
    msg[2] = TELOPT_TTYPE;
    msg[3] = 0;
    for (idx = 4; *term && idx<sizeof(msg)-2; ++idx, ++term) {
	msg[idx] = *term;
    }
    msg[idx++] = IAC;
    msg[idx++] = SE;

    /* Send and log message
     */
    socketWrite((char*)msg, idx);
    logProtocol("send SB TTYPE IS %.*s IAC SE\n", idx - 6, msg + 4);
}

/* routine of parseTelnetCommand (handle SB) */
static BOOL parseTelnetCommand_SB (void);

/* Add a character to the current command and try to process it.
 * Return whether or not to remain in command mode.
 *
 * Args:
 * ch - character to add to command
 */
static BOOL parseTelnetCommand(unsigned char ch)
{
    unsigned char cmd;		/* extract telnet command */
    unsigned char opt;		/* extract command option */

    if (!rawvar.cmd.ptr) {
	rawvar.cmd.maxlen= 40;
	rawvar.cmd.ptr= xmalloc (rawvar.cmd.maxlen);
    }

    /* First copy the character into the command buffer
     */
    if (rawvar.cmd.len == rawvar.cmd.maxlen) {
	logProtocol("TELNET Command buffer overflow\n");
	rawvar.cmd.len = 0;
	return FALSE;
    }

    rawvar.cmd.ptr[rawvar.cmd.len++] = ch;
    if (rawvar.cmd.len == 1)
	return TRUE;		/* Minimum command length is 2 */

    /* Get command
     */
    cmd = rawvar.cmd.ptr[1];
    switch (cmd) {
    case IAC:
	/* Command was escaped IAC (255) data byte
	 */
	emulAddTextServer (rawvar.cmd.ptr, 1);
	rawvar.cmd.len = 0;
	return FALSE;		/* Not in command any more */

    case DO:			/* server telling us to do something */
    case DONT:			/* server telling us not to do something */
    case WILL:			/* server telling us it will do something */
    case WONT:			/* server telling us it won't do something */
	if (rawvar.cmd.len < 3)
	    return TRUE;	/* Options require 3 bytes */

	/* Get command option
	 */
	opt = rawvar.cmd.ptr[2];
	logProtocol("recv %s %s\n", telcmd(cmd), telopt(opt));

	switch (opt) {
	case TELOPT_BINARY:
	    /* Code added 11/25/98, patrick@narkinsky.ml.org
	     */
	    switch (cmd) {
	    case DO:
		/* Server has asked us to send BINARY
		 */
		if (enableBINARY) {
		    rawCmd(WILL, TELOPT_BINARY);
		    inBINARY = TRUE;
		} else
		    rawCmd(WONT, TELOPT_BINARY);
		break;
	    case DONT:
		/* Server told us not to BINARY, tell it we WONT
		 */
		rawCmd(WONT, TELOPT_BINARY);
		inBINARY = FALSE;
		break;
	    case WILL:
		/* Server has told us it will send BINARY
		 */
		if (enableBINARY) {
		    rawCmd(DO, TELOPT_BINARY);
		    inBINARY = TRUE;
		} else
		    rawCmd(DONT, TELOPT_BINARY);
		break;
	    case WONT:
		/* Server has told us it WONT BINARY, tell it not to
		 */
		rawCmd(DONT, TELOPT_BINARY);
		inBINARY = FALSE;
		break;
	    }
	    if (enableBINARY)
		doneBINARY = TRUE;
	    break;

	case TELOPT_NAWS:
	    switch (cmd) {
	    case DO:
		/* Server has asked us to NAWS
		 */
		dontNAWS = FALSE;
		if (!doneNAWS)
		    rawCmd(WILL, TELOPT_NAWS);
		rawSetWindowSize();
		break;
	    case DONT:
		/* Server has told us not to NAWS
		 */
		dontNAWS = TRUE;
		break;
	    }
	    doneNAWS = TRUE;
	    break;

	case TELOPT_ECHO:
	    switch (cmd) {
	    case DO: /* Server has told us to ECHO - reject */
		     /* Note: netkit-telnetd sends "DO ECHO" */
		     /* just to detect broken clients */
                     /* but we are not (so) broken */
		rawCmd(WONT, TELOPT_ECHO);
		break;
	    case DONT:
		/* Server told us not to ECHO - ignore */
		break;
	    case WILL:
		/* Server told us it WILL ECHO, tell it to ECHO */
		rawCmd(DO, TELOPT_ECHO);
		emulSetEcho(FALSE);
		break;
	    case WONT:
		/* Server has told us it WONT ECHO, tell it not to */
		rawCmd(DONT, TELOPT_ECHO);
		emulSetEcho(TRUE);
		break;
	    }
	    break;

	case TELOPT_SGA:
	    switch (cmd) {
	    case DO:
		/* Server told us to SGA, tell it we WILL SGA
		 */
		rawCmd(WILL, TELOPT_SGA);
		break;
	    case WILL:
		/* Server told us it WILL SGA, tell it to SGA
		 */
		if (!fDoSgaSent) {
		    rawCmd(DO, TELOPT_SGA);
		    fDoSgaSent = TRUE;
		}
		break;
	    }
	    doneSGA = TRUE;
	    break;

	case TELOPT_TTYPE:
	    switch (cmd) {
	    case DO:
		/* Server told us to TTYPE, tell it we WILL TTYPE
		 */
		rawCmd(WILL, TELOPT_TTYPE);
		break;
	    }
	    break;

	case TELOPT_LFLOW:
	    switch (cmd) {
	    case DO:
		/* Server told us DO LFLOW, tell it we will LFLOW
		 */
		if (!socketLocalFlowControl()) {
		    rawCmd(WILL, TELOPT_LFLOW);
		    socketSetLocalFlowControl(TRUE);
		}
		break;
	    case DONT:
		/* Server told us not to LFLOW, tell it we WONT LFLOW
		 */
		rawCmd(WONT, TELOPT_LFLOW);
		socketSetLocalFlowControl(FALSE);
		break;
	    }
	    doneLFLOW = TRUE;
	    break;

	case TELOPT_XDISPLOC:

	    switch (cmd) {
	    case WILL: /* Server should never send me his DISPLAY ! */
		rawCmd(DONT, TELOPT_XDISPLOC);
		break;
	    case WONT: /* No reply required */
		break;

	    case DO:
	    case DONT:
	        rawTLOMachine (TELOPT_XDISPLOC, &rawvar.toXdisploc, cmd);
		break;
	    }
	    break;

	case TELOPT_NEWENV:
	    switch (cmd) {
	    case WILL: /* Not interested in server's environment */
		rawCmd(DONT, TELOPT_NEWENV);
		break;
	    case WONT: /* No reply required */
		break;

	    case DO:
	    case DONT:
	        rawTLOMachine (TELOPT_NEWENV, &rawvar.toNewEnv, cmd);
		break;
	    }
	    break;

	default:
	    switch (cmd) {
	    case DO:
	    case DONT:
		/* For all other commands, say we WONT do them
		 */
		rawCmd(WONT, opt);
		break;
	    case WILL:
	    case WONT:
		/* For all other commands tell the server not to do them
		 */
		rawCmd(DONT, opt);
		break;
	    }
	    break;
	}
	rawvar.cmd.len = 0;
	return FALSE;		/* Not in command any more */

    case SB:
	return parseTelnetCommand_SB ();

    case DM:	/* Data mark--for connect. cleaning */
    case GA:	/* You may reverse the line */
    case EL:	/* Erase the current line */
    case EC:	/* Erase the current character */
    case BRK:	/* Break */
    case NOP:	/* NOP */
    case AYT:	/* Are you there */
    case AO:	/* Abort output--but let prog finish */
    case IP:	/* Interrupt process--permanently */
    case SE:	/* End sub negotiation */
	rawvar.cmd.len = 0;
	return FALSE;		/* Not in command any more */
    }

    return TRUE;
}

static void EnvVars_SendFormat (DynBuffer *into, const EnvVars *from, int *pn);

static BOOL parseTelnetCommand_SB (void)
{
    unsigned char opt;
    char msg [300], display[200];
    int msglen;
	/* Server wants to negotiate something we agreed to do
	 */
	 
	if (rawvar.cmd.len < 3)
	    return TRUE;

	opt = rawvar.cmd.ptr[2];
	switch (opt) {
	case TELOPT_TTYPE:
	    /* IAC SB TERMINAL-TYPE SEND IAC SE
	     * Server wants us to send our terminal type
	     */
	    if (rawvar.cmd.len < 6)
		return TRUE;
	    if (rawvar.cmd.ptr[3] == 1 &&
		rawvar.cmd.ptr[4] == (char)IAC &&
		rawvar.cmd.ptr[5] == (char)SE) {
		const Emul *emul = emulGetTerm ();

		logProtocol("recv SB TERMINAL-TYPE SEND IAC SE\n");
		rawSetTerm(emul->servname[0]? emul->servname: emul->name);
	    }
	    break;
	case TELOPT_LFLOW:
	    /* IAC SB TOGGLE-FLOW-CONTROL ? IAC SE
	     * Server is telling us to modify local flow control
	     */
	    if (rawvar.cmd.len < 6 || rawvar.cmd.ptr[rawvar.cmd.len-1] != (char)SE)
		return TRUE;
	    switch (rawvar.cmd.ptr[3]) {
	    case LFLOW_OFF:
		socketSetLocalFlowControl(FALSE);
		logProtocol("recv SB LFLOW OFF IAC SE\n");
		break;
	    case LFLOW_ON:
		socketSetLocalFlowControl(TRUE);
		logProtocol("recv SB LFLOW ON IAC SE\n");
		break;
	    case LFLOW_RESTART_ANY:
		emulLocalFlowAny(TRUE);
		logProtocol("recv SB LFLOW RESTART-ANY IAC SE\n");
		break;
	    case LFLOW_RESTART_XON:
		emulLocalFlowAny(FALSE);
		logProtocol("recv SB LFLOW RESTART-XON IAC SE\n");
		break;
	    default:
		logProtocol("recv SB LFLOW 0x%02x IAC SE\n", rawvar.cmd.ptr[3]);
		break;
	    }
	    break;

	case TELOPT_XDISPLOC:
	    if (rawvar.cmd.len < 6 || rawvar.cmd.ptr[rawvar.cmd.len-1] != (char)SE)
		return TRUE;
	    if (rawvar.cmd.ptr[3] == (char)TELNET_SEND && 
                rawvar.cmd.ptr[4] == (char)IAC && 
                rawvar.cmd.ptr[5] == (char)SE) {
		logProtocol("recv SB XDISPLOC SEND IAC SE\n");
		{
		    char tmpip[64];

		    socketGetAddrStr (tmpip, SOCGA_LOCAL|SOCGA_NO_PORT|SOCGA_IPv6_BRACKETS, 0);
		    sprintf (display, "%s:0", tmpip);
		}
		msglen  = sprintf (msg, "%c%c%c%c%s%c%c",
				   IAC, SB, TELOPT_XDISPLOC, 0,
				   display, IAC, SE);
		socketWrite(msg, msglen);
		logProtocol("send SB XDISPLOC IS %s IAC SE\n",
			    display);
	    }
	case TELOPT_NEWENV:
	    if (rawvar.cmd.len < 6 || rawvar.cmd.ptr[rawvar.cmd.len-1] != (char)SE)
		return TRUE;
	    if (rawvar.cmd.ptr[3] == (char)TELNET_SEND && 
                rawvar.cmd.ptr[rawvar.cmd.len-2] == (char)IAC && 
                rawvar.cmd.ptr[rawvar.cmd.len-1] == (char)SE) {
		DynBuffer db= EmptyBuffer;
		int nvars;

		logProtocol ("recv SB NEWENV SEND IAC SE\n");

		EnvVars_SendFormat (&db, DefaultEnvVars, &nvars);
		if (nvars) {
		    socketWrite (db.ptr, db.len);
		    logProtocol ("send SB NEWENV IS (%d vars) IAC SE\n",
			    nvars);
		}
		if (db.ptr) xfree (db.ptr);
	    }
	}

	rawvar.cmd.len = 0;
	return FALSE;		/* Not in command any more */
}

/* Set the protocol to interpret in the session
 *
 * Args:
 * proto - the protocol to interpret
 */
void rawEnableProtocol(RawProtocol proto)
{
    rawProtocol = proto;

    switch (proto) {
    case protoNone:
	/* When not doing any protocol, echo all characters
	 */
	emulSetEcho(TRUE);
	logProtocol("no protocol\n");
	break;
    case protoTelnet:
	/* When doing telnet, start by not echoing characters
	 */
	emulSetEcho(TRUE);  /* Changed to TRUE by LZS on 2001.04.02 */
	logProtocol("telnet protocol enabled\n");
	break;
    case protoRlogin:
	/* When doing rlogin, start by not echoing characters
	 */
	emulSetEcho(FALSE);
	logProtocol("rlogin protocol enabled\n");
	break;
    }
}

/* Return the protocol that we are interpreting
 */
RawProtocol rawGetProtocol()
{
    return rawProtocol;
}

/* Interpret some data from the telnet server
 *
 * Args:
 * text - the text received from the server
 * len -  the amount of text received
 */
static int rawProcessTelnet(unsigned char* text, int len)
{
    int numChars = 0;		/* number of consecutive session
				 * characters found */
    int idx;			/* iterate over text */

    ASSERT(text != NULL);
    ASSERT(len > 0);

    if (startSession) {
	/* This is the first data received from the telnet server
	 */
	logProtocol("Server initiated options\n");
	startSession = FALSE;
    }

    /* Process the text received
     */
    for (idx = 0; idx < len; idx++) {
	if (rawvar.inCommand) {
	    /* If we are in a command, process the command one
	     * character at a time.
	     */
	    rawvar.inCommand = parseTelnetCommand(text[idx]);
	} else {
	    /* Scan over text until we hit a command
	     */
	    if (text[idx] != IAC) {
		/* Normal text, just count it for now
		 */
		if (ignoreTextUntilDM && text[idx] == DM) {
		    /* Server has told us to ignore text until we see DM
		     */
		    ignoreTextUntilDM = FALSE;
		    numChars = 0;
		} else
		    numChars++;
	    } else {
		/* Some kind of command
		 */
		if (numChars > 0) {
		    /* If we scanned any text, pass this text to the
		     * text receive processing.
		     */
		    if (!ignoreTextUntilDM)
			emulAddTextServer ((char *)text + idx - numChars, numChars);
		    numChars = 0;
		}
		/* Pass the IAC into the command parser
		 */
		rawvar.inCommand = parseTelnetCommand(text[idx]);
	    }
	}
    }

    if (numChars > 0 && !ignoreTextUntilDM)
	/* We finished the loop with scanned text, pass this text to
	 * the text receive processing.
	 */
	emulAddTextServer((char *)text + idx - numChars, numChars);

    return len;
}

/* Interpret some data from the rlogin server
 *
 * Args:
 * text - text received from the server
 * len -  the amount of text received
 */
static int rawProcessRlogin(unsigned char* text, int len)
{
    int idx = 0;		/* index to process from */

    ASSERT(text != NULL);
    ASSERT(len > 0);
    if (startSession) {
	/* The server might send a nul as the very first character,
	 * skip over it
	 */
	if (*text == '\0')
	    idx = 1;
	startSession = FALSE;
    }

    /* Send the rest of the text to the terminal emulator
     */
    if (idx < len)
	emulAddTextServer ((char *)text + idx, len - idx);

    return len;
}

/* Process some data received from the server
 *
 * Args:
 * text - text received from the server
 * len -  amount of text received
 */
int rawProcessData(unsigned char* text, int len)
{
    ASSERT(text != NULL);
    ASSERT(len > 0);
    switch (rawProtocol) {
    case protoNone:
	/* No protocol - send text directly to terminal
	 */
	emulAddTextServer ((char *)text, len);
	return len;

    case protoTelnet:
	return rawProcessTelnet(text, len);

    case protoRlogin:
	return rawProcessRlogin(text, len);
    }
    /* Just in case...
     */
    return len;
}

/* Process some out-of-band data from the server
 *
 * Args:
 * text - text received from server
 * len  - length of text received
 */
void rawProcessOob(unsigned char* text, int len)
{
#if 0
    /* Display the text via OutputDebugString
     */
    int idx;

    ods("OOB:\n");
    for (idx = 0; idx < len; idx += 16) {
	int off;
	for (off = 0; idx + off < len && off < 16; ++off)
	    ods("%02x ", text[idx + off]);
	while (off++ < 16)
	    ods("   ");
	for (off = 0; idx + off < len && off < 16; ++off)
	    if (isprint(text[idx + off]))
		ods("%c", text[idx + off]);
	    else
		ods(".");
	ods("\n");
    }
#endif
    ASSERT(text != NULL);
    ASSERT(len > 0);
    switch (rawProtocol) {
    case protoTelnet:
	switch (*text) {
	case 0xff:
	    logProtocol("recv TELNET Synch\n");
	    ignoreTextUntilDM = TRUE;
	    break;
	}
	break;
    case protoRlogin:
	switch (*text) {
	case 0x02:		/* discard buffered data from server */
	    break;
	case 0x10:		/* raw mode - send ^S/^Q to server */
	    socketSetLocalFlowControl(FALSE);
	    break;
	case 0x20:		/* handle ^S/^Q locally */
	    socketSetLocalFlowControl(TRUE);
	    break;
	case 0x80:
	    dontNAWS = FALSE;
	    rawSetWindowSize();
	    break;
	}
	break;
    case protoNone:
	break;
	/* do nothing, only avoid gcc warning */
    }
}

/* Find the protocol appropriate for the specified port
 */
RawProtocol findPortProtocol(char* portName)
{
    unsigned idx;

    for (idx = 0; idx < numElem(protocols); idx++)
	if (stricmp(portName, protocols[idx].name) == 0
	    || atoi(portName) == protocols[idx].port)
	    return protocols[idx].proto;
    return protoNone;
}

void rawTLOMachine (unsigned char option, TelnetLocalOption *state,
                    int input /* DO or DONT */)
{
    if (*state==toIDontWant) { /* never change this state */
	if (input==DO) {
	    rawCmd (WONT, option);
	}

    } else if (input==DO) {
	if (*state == toIWDisabled) {
	    rawCmd (WILL, option);
	}
	*state = toIWEnabled;

    } else { /* input==DONT */
	if (*state == toIWEnabled) {
	    rawCmd (WONT, option);
	}
	*state = toIWDisabled;
    }
}

static unsigned EVSF_copy (char *to, const char *from)
{
    const char *p= from;
    char *q= to;
    unsigned len= 0;
    int c;

    for (; (c= (unsigned char)*p)!=0; ++p) {
	if (c>=0 && c<=4) { 	/* escaping required */
	    if (q) *q++ = 2;	/* escape character */
	    ++len;
	} else if (c==IAC) { 	/* escaping required (duplication) */
	    if (q) *q++ = IAC;	/* escape character (itself) */
	    ++len;
	}
	if (q) *q++ = (char)c;
	++len;
    }
    return len;
}

static void EnvVars_SendFormat (DynBuffer *into, const EnvVars *from, int *pn)
{
    const char *name, *value;
    int rc;
    unsigned len;
    char *p;
    int n= 0;

    len= 0;
    p= into->ptr + into->len;

    for (rc= EV_Get (from, EVGET_FIRST, NULL, &name, &value);
	rc==0;
	rc= EV_Get (from, EVGET_GT, name, &name, &value)) {
	++n;
	len += 1 + EVSF_copy (NULL, name);
	len += 1 + EVSF_copy (NULL, value);
/*	printf ("\t%s = %s\n", name, value); */
    }
/*  printf ("required length is %u\n", len); */

    if (len==0) goto RETURN;

    BuffSecX (into, 5 + len + 2);
    p= into->ptr + into->len;

    *p++ = IAC;
    *p++ = SB;
    *p++ = TELOPT_NEWENV;
    *p++ = TELNET_IS;

    for (rc= EV_Get (from, EVGET_FIRST, NULL, &name, &value);
	rc==0;
	rc= EV_Get (from, EVGET_GT, name, &name, &value)) {
	*p++ = 0;	/* var */
	p += EVSF_copy (p, name);
	*p++ = 1;	/* value */
	p += EVSF_copy (p, value);
    }

    *p++ = IAC;
    *p++ = SE;

RETURN:
    into->len= (size_t)(p - into->ptr);
    if (pn) *pn= n;
}
