/* raw.h
 * Copyright (c) 1997 David Cole
 *
 * Strip off and process raw protocol from session
 */
#ifndef __raw_h
#define __raw_h

/* The protocol that we are currently using
 * The order of the values in this enum is important.  There is an
 * array in connect.c called protocols which is indexed by this enum.
 */
typedef enum {
    protoTelnet,		/* using telnet protocol */
    protoRlogin,		/* using rlogin protocol */
    protoNone			/* not using protocol - pass through */
} RawProtocol;

/* Describe the different protocol types we can use on the connection.
 * The order of the entries in this array is important - the index of
 * array must match the RawProtocol enum defined in raw.h
 */
struct Protocols{
    char* name;			/* string name of the protocol */
    int port;			/* port number which uses protocol */
    RawProtocol proto;		/* socket protocol mode */
};

/* Enable telnet BINARY mode negotiation */
void rawEnableBinaryMode(void);
/* Return whether or not we are in telnet BINARY mode */
BOOL rawInBinaryMode(void);
/* Perform initialisation for session on new connection */
void rawStartSession(void);

/* Send a telnet command to the server */
void rawCmd(unsigned char cmd, unsigned char opt);
/* Send a NOP telnet command to the server
 */
void rawNop(void);

/* User has started resizing the window */
void rawResizeBegin(void);
/* User just finished resizing the window */
void rawResizeEnd(void);
/* Tell the telnet/rlogin server what our window size is */
void rawSetWindowSize(void);
/* Tell the telnet server what terminal we are using */
void rawSetTerm(const char *termname);
/* Set the protocol to interpret in the session */
void rawEnableProtocol(RawProtocol proto);
/* Return the protocol that we are interpreting */
RawProtocol rawGetProtocol(void);
/* Process some data received from the server */
int rawProcessData(unsigned char* text, int len);
/* Process some out-of-band data from the server */
void rawProcessOob(unsigned char* text, int len);

/* Find the protocol appropriate for the specified port */
RawProtocol findPortProtocol(char* portName);

typedef enum TelnetLocalOption {
    toIDontWant,  /* the simplest case - we do not want this option */
    toIWDisabled, /* "DONT" received from server*/
    toIWPending,  /* "WILL" sent, no answer received yet */
    toIWEnabled   /* "WILL" sent and "DO" received (in any sequence) */
} TelnetLocalOption;

/* Finite State Machine for TelnetLocalOption:

   OldState   Input NewState   Output Note
   IDontWant  DO    stay       WONT   
   IDontWant  DONT  stay       -
   IWDisabled DO    IWEnabled  WILL
   IWDisabled DONT  stay       -
   IWPending  DO    IWEnabled  -
   IWPending  DONT  IWDisabled -
   IWEnabled  DO    stay       -
   IWEnabled  DONT  IWDisabled WONT
*/

/* rawTLOMachine: changes state, sends answer (if neccessary) */
void rawTLOMachine (unsigned char option, TelnetLocalOption *state,
                    int input /* DO or DONT */);

#endif
