/* socket.h
 * Copyright (c) 1997 David Cole
 *
 * Handle the WINSOCK connection to server
 */
#ifndef __socket_h
#define __socket_h

/* Return whether or not we have a connection to the remote end */
BOOL socketConnected(void);
/* Return whether or not we have ever had a connection ('Reconnect' menu) */
BOOL socketConnectedEver(void);
/* Return whether or not we are offline and idle */
BOOL socketOffline(void);
/* Close the connection to the server */
void socketDisconnect(void);

/* Perform any network communication that is pending */
void socketProcessPending(void);
/* Return whether or not we are performing local flow control */
BOOL socketLocalFlowControl(void);
/* Set local flow control on or off */
void socketSetLocalFlowControl(BOOL local);
/* Stop or restart flow locally */
void socketSetFlowStop(BOOL stop);
/* Return whether or not flow has been stopped locally */
BOOL socketFlowStopped(void);
/* Report a socket error to the user */
void socketError(const char* operation, int error);

/* Display the current communications state in the status bar */
void socketShowStatus(void);
/* Return the remote host passed into socketConnect() */
char* socketGetHost(void);
/* Return the remote service passed into socketConnect() */
char* socketGetPort(void);
/* Return the handle of the socket */
HANDLE socketGetHandle(void);

/* Initiate a connection to the remote end */
int  socketConnect(const char* host, const char* service);
/* Queue some data to be written to the remote end. */
void socketWrite(const char* data, int len);

#define socketWriteByte(ubyte) { \
    unsigned char tmpchar= (unsigned char)(ubyte); \
    socketWrite ((void *)&tmpchar, 1); \
}

/* Return local IP-address as a string
 */
/* char *socketGetLocalIP(void); */
char *socketGetAddrStr (char *into, int flags, char portsep);
#define SOCGA_WHITCH   	   	0x1000
#define SOCGA_LOCAL   	   	0
#define SOCGA_REMOTE	   	0x1000	/* server or proxy */
#define SOCGA_IPv6_BRACKETS	1	/* 127.0.0.1 or [::1] */
#define SOCGA_NO_PORT		2	/* portsep==0 means this too */

/* Check for a local X-server */
BOOL socketHaveXServer (void);

/* Register the window class that will receive the socket messages */
BOOL socketInitClass(HINSTANCE instance);
/* Initialise WINSOCK */
BOOL socketInit(HINSTANCE instance);
/* Unregister with WINSOCK */
void socketCleanup(void);

#endif
