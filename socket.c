/* socket.c
 * Copyright (c) 1997 David Cole
 *
 * Handle the WINSOCK connection to server
 */

#include "preset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <malloc.h>
#include <ctype.h>
#if defined (_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <winreg.h>
#else
    #include <winsock.h>
#endif
#include <windows.h>

#include "platform.h"
#include "utils.h"
#include "dtelnet.h"
#include "emul.h"
#include "term.h"
#include "connect.h"
#include "raw.h"
#include "socket.h"
#include "status.h"
#include "log.h"
#include "proxy.h"

#define SOCKLIB_VERSION 0x0101

typedef union SockAddr {
    struct sockaddr    sa;
    struct sockaddr_in sin;
#if defined(_WIN32)
    struct sockaddr_in6 sin6;
#endif
} SockAddr;

typedef struct SockAddrL {
    uint16_t len;	/* nettó, pl: sizeof (addr.sin) */
    SockAddr addr;
} SockAddrL;

static int saPrints  (char *to, const SockAddr  *from, int opts, int portsep);
static int salPrints (char *to, const SockAddrL *from, int opts, int portsep);

/* opts */
#define SALP_IPv6_BRACKETS	1	/* 127.0.0.1 or [::1] */
#define SALP_NO_PORT		2	/* portsep==0 means this too */

/* Windows doesn't have 'inet_aton' */
static int my_inet_aton (const char *from, struct in_addr *into);
#if defined(_WIN32)
/* Windows XP doesn't have inet_ntop/inet_pton */
static char *my_inet_ntop (int af, const void *from, char *dst, unsigned dstsize);
static int  my_inet_pton (int af, const char *from, void *dst);
#endif

static char hostbuff[MAXGETHOSTSTRUCT];	/* data from host buffer */
static struct WSAData WSAData;	/* SocketLib data buffer */
static HWND sockWnd;		/* window for handling socket messages */
static SOCKET sock = INVALID_SOCKET; /* The socket associated with the class */
static SockAddrL addr;		/* Remote address to connect to */
static SockAddrL proxyAddr;	/* Build address for proxy server */
static SockAddrL localAddr;	/* Local address */

static BOOL remoteHasClosed;	/* Remember if remote end has closed socket */
static BOOL readyForRead;	/* Remember if we have a read pending */
static BOOL readyForWrite;	/* Remember if we have a write pending */
static BOOL localFlowControl;	/* Are we doing local flow control? */
static BOOL flowStopped;	/* Has the user pressed ^S? */

static char socketHost[128];	/* where we connecting to */
static char socketService[64];	/* which service we are connecting to */

static char readBuffer[512]; /* limit amount we read every
				       * time to 512 so we can have
				       * some ability to interrupt
				       * output via ^S */
static int readIdx;		/* current position in readBuffer */

/* Hold all data to be sent to the remote end in a queue.
 */
typedef struct Data Data;
struct Data {
    Data* next;			/* next data in queue */
    unsigned char* text;	/* text to be sent to remote end */
    int len;			/* length of text being sent */
};

static Data* dataQueue;		/* head of data queue */
static Data* dataEnd;		/* end of data queue */
static int writeIdx;		/* write position in queue head */

/* Define the states that communication to the remote end can be in
 */
typedef enum {
    sockOffline,		/* idle and offline */
    sockGetProxyHostName,	/* offline - resolving proxy host */
    sockGetHostName,		/* offline - resolving host */
    sockConnect,		/* offline - connecting */
    sockProxy,			/* offline - proxy request */
    sockOnline			/* online */
} SocketState;

static SocketState socketState = sockOffline; /* current socket state */
static SocketState maxSocketState = sockOffline; /* maximum socket state so far */

static void socketSetState(SocketState state);

typedef enum { JP_TODO, JP_DOING, JP_DONE } JobPhase;
/* 'DONE' can be 'not necessary' too:
    e.g. if no proxy required, it's DONE */

static struct {
    JobPhase proxy_host_resolv;
    JobPhase host_resolv;
    JobPhase connect;
    JobPhase proxy_req;
} socketStateExt;

static char* socketWinClass = "DTelnetSocketWClass";

/* Report a socket error to the user
 *
 * Args:
 * operation - the operation being performed when error was encountered
 * error -     the error that was encountered
 */
void socketError(const char* operation, int error)
{
    char msg[512];		/* format error message */
    char *dstHost;              /* pointer to destination host */
    char *dstService;           /* pointer to destination service */

    /* set destination host/service
     */
    if (proxyEnabled()) {
        dstHost = proxyGetHost();
        dstService = proxyGetPort();
    }
    else {
        dstHost = socketHost;
        dstService = socketService;
    }
    switch (error) {
    case WSAEINTR:
	sprintf(msg, "%s was interrupted", operation);
	break;
    case WSAEBADF:
	sprintf(msg, "%s used a bad file descriptor", operation);
	break;
    case WSAEACCES:
	sprintf(msg, "Permission denied for %s", operation);
	break;
    case WSAEFAULT:
	sprintf(msg, "%s used a bad address", operation);
	break;
    case WSAEINVAL:
	sprintf(msg, "Invalid argument in %s", operation);
	break;
    case WSAEMFILE:
	sprintf(msg, "Too many open files during %s", operation);
	break;
    case WSAEWOULDBLOCK:
	sprintf(msg, "Operation %s would block", operation);
	break;
    case WSAEINPROGRESS:
	sprintf(msg, "Called %s while blocking operation in progress",
		operation);
	break;
    case WSAEALREADY:
	sprintf(msg, "%s already in progress", operation);
	break;
    case WSAENOTSOCK:
	sprintf(msg, "Called %s on a non-socket", operation);
	break;
    case WSAEDESTADDRREQ:
	sprintf(msg, "Destination address missing during %s", operation);
	break;
    case WSAEMSGSIZE:
	sprintf(msg, "Message too long during %s", operation);
	break;
    case WSAEPROTOTYPE:
	sprintf(msg, "Protocol wrong type for socket during %s", operation);
	break;
    case WSAENOPROTOOPT:
	sprintf(msg, "Protocol not available during %s", operation);
	break;
    case WSAEPROTONOSUPPORT:
	sprintf(msg, "Protocol not supported during %s", operation);
	break;
    case WSAESOCKTNOSUPPORT:
	sprintf(msg, "Socket type not supported during %s", operation);
	break;
    case WSAEOPNOTSUPP:
	sprintf(msg, "Option not supported during %s", operation);
	break;
    case WSAEPFNOSUPPORT:
	sprintf(msg, "Protocol family not supported during %s", operation);
	break;
    case WSAEAFNOSUPPORT:
	sprintf(msg, "Address family not supported during %s", operation);
	break;
    case WSAEADDRINUSE:
	sprintf(msg, "Address already in use during %s", operation);
	break;
    case WSAEADDRNOTAVAIL:
	sprintf(msg, "Cannot assign requested address during %s", operation);
	break;
    case WSAENETDOWN:
	sprintf(msg, "Network is down");
	break;
    case WSAENETUNREACH:
	sprintf(msg, "Network is unreachable");
	break;
    case WSAENETRESET:
	sprintf(msg, "Network dropped connection because of reset");
	break;
    case WSAECONNABORTED:
	sprintf(msg, "Software caused connect abort during %s", operation);
	break;
    case WSAECONNRESET:
	sprintf(msg, "Connect has been reset by server");
	break;
    case WSAENOBUFS:
	sprintf(msg, "No buffer space available during %s", operation);
	break;
    case WSAEISCONN:
	sprintf(msg, "Already connected during %s", operation);
	break;
    case WSAENOTCONN:
	sprintf(msg, "Not connected during %s", operation);
	break;
    case WSAESHUTDOWN:
	sprintf(msg, "Cannot send after shutdown during %s", operation);
	break;
    case WSAETOOMANYREFS:
	sprintf(msg, "Too many references during %s", operation);
	break;
    case WSAETIMEDOUT:
	sprintf(msg, "Connection to %s/%s timed out", dstHost, dstService);
	break;
    case WSAECONNREFUSED:
	sprintf(msg, "Connection to %s/%s refused", dstHost, dstService);
	break;
    case WSAELOOP:
	sprintf(msg, "Too many symbolic links during %s", operation);
	break;
    case WSAENAMETOOLONG:
	sprintf(msg, "File name too long during %s", operation);
	break;
    case WSAEHOSTDOWN:
	sprintf(msg, "Host %s is down", dstHost);
	break;
    case WSAEHOSTUNREACH:
	sprintf(msg, "No route to host %s", dstHost);
	break;
    case WSAENOTEMPTY:
	sprintf(msg, "Directory not empty during %s", operation);
	break;
    case WSAEUSERS:
	sprintf(msg, "Too many users during %s", operation);
	break;
    case WSAEDQUOT:
	sprintf(msg, "Quota exceeded during %s", operation);
	break;
    case WSAESTALE:
	sprintf(msg, "Stale NFS handle during %s", operation);
	break;
    case WSAEREMOTE:
	sprintf(msg, "Object is remote during %s", operation);
	break;
    case WSASYSNOTREADY:
	sprintf(msg, "Network subsystem is not usable");
	break;
    case WSAVERNOTSUPPORTED:
	sprintf(msg, "WINSOCK DLL cannot support this application");
	break;
    case WSANOTINITIALISED:
	sprintf(msg, "WSAStartup has not been called during %s", operation);
	break;
    case WSAHOST_NOT_FOUND:
	sprintf(msg, "Authoritative host not found");
	break;
    case WSATRY_AGAIN:
	sprintf(msg, "Try again");
	break;
    case WSANO_RECOVERY:
	sprintf(msg, "No recovery during %s", operation);
	break;
    case WSANO_DATA:
	switch (socketState) {
	case sockGetProxyHostName:
	    sprintf(msg, "No such host %s", proxyGetHost());
	    break;
	case sockGetHostName:
	    sprintf(msg, "No such host %s", socketHost);
	    break;
/*	case sockGetServName:
 *	    sprintf(msg, "No such service %s", socketService);
 *	    break;
 */
	default:
	    sprintf(msg, "No data during %s", operation);
	    break;
	}
	break;
    }
    MessageBox(termGetWnd(), msg, telnetAppName(),
	       MB_APPLMODAL | MB_ICONHAND | MB_OK);
}

/* Close our connection to the remote end and discard any data that
 * was queued to be sent
 */
static void closeSocket(void)
{
    remoteHasClosed = FALSE;
    readIdx = writeIdx = 0;
    if (sock != INVALID_SOCKET) {
        if (WSAAsyncSelect(sock, sockWnd, 0, 0) == SOCKET_ERROR)
            socketError("WSAAsyncSelect()", WSAGetLastError());
        if (closesocket(sock) == SOCKET_ERROR)
            socketError("closesocket()", WSAGetLastError());
        sock = INVALID_SOCKET;
    }

    /* Reset the application window title
     */
    telnetSetTitle();
    socketSetState(sockOffline);

    /* Discard all data that was queued to be sent to remote end
     */
    while (dataQueue != NULL) {
	Data* tmp = dataQueue->next;
	ASSERT(dataQueue->text != NULL);
	free(dataQueue->text);
	free(dataQueue);
	dataQueue = tmp;
    }
    dataEnd = NULL;
}

/* Return whether or not we have a connection to the remote end */
BOOL socketConnected()
{
    return socketState == sockOnline;
}

BOOL socketConnectedEver(void)
{
    return maxSocketState == sockOnline;
}

/* Return whether or not we are offline and idle
 */
BOOL socketOffline(void)
{
    return socketState == sockOffline;
}

/* Close the connection to the server
 */
void socketDisconnect(void)
{
    closeSocket();
}

/* Read some data from the remote end and send it through the raw
 * protocol processing code.
 */
static void socketReadReady(void)
{
    int error;   		/* Error returned from socket read */
    int numRead;		/* Number of bytes read this time around */
    int numUsed;		/* Number of bytes used by processing */

    if (sock == INVALID_SOCKET) return;

    /* we give the control to proxy.c to handle the
     * proxy protocol while socketState == sockProxy
     */
    if (socketState == sockProxy)
    {
	int ret;
	ret = proxyProtocolHandler();
	if (ret == PROXY_ERROR) {
	    closeSocket();
	}
	else if (ret == PROXY_CONNECTED) {
            socketSetState(sockOnline);
            telnetSetTitle();
            rawStartSession();
	}
	return;
    }
    /* Try to read as much as possible from the server.
     */
    ASSERT((size_t)readIdx < sizeof(readBuffer));

    numRead = recv(sock, ((char*)readBuffer) + readIdx,
		   sizeof(readBuffer) - readIdx, 0);

    if (numRead == 0)
	/* No more data ready
	 */
	return;
    if (numRead == SOCKET_ERROR) {
	error = WSAGetLastError();
	if (error == WSAEWOULDBLOCK)
	    /* No more data ready
	     */
	    return;
	/* Error: close socket
	 */
	socketError("recv()", error);
	closeSocket();
	return;
    }

    /* Got some data from the remote end, send it through protocol
     * processing
     */
    ASSERT(numRead > 0);
    readIdx += numRead;

    numUsed = rawProcessData((unsigned char *)readBuffer, readIdx);
    ASSERT(numUsed >= 0);
    ASSERT(numUsed <= readIdx);

    /* Skip over data that was consumed by protocol code
     */
    if (numUsed == readIdx)
	readIdx = 0;
    else {
	memmove(readBuffer, readBuffer + numUsed, readIdx - numUsed);
	readIdx -= numUsed;
    }
    if (readIdx == sizeof(readBuffer)) {
	/* Buffer is full.
	 */
	readIdx = 0;
    }
}

/* Write queued data to the remote end
 */
static void socketWriteReady(void)
{
    while (dataQueue != NULL) {
	int numWritten;

	/* Get the data at the head of the queue and send as
	 * much as possible to the remote end.
	 */
	ASSERT(dataQueue->text != NULL);
	ASSERT(dataQueue->len > writeIdx);
	numWritten = send(sock, (char *)dataQueue->text + writeIdx,
			  dataQueue->len - writeIdx, 0);

	if (numWritten != SOCKET_ERROR) {
	    /* Managed to send data
	     */
	    writeIdx += numWritten;
	    if (writeIdx == dataQueue->len) {
		Data* tmp = dataQueue;

		/* Completed sending the data - remove it from the queue
		 * and delete it.
		 */
		dataQueue = dataQueue->next;
		if (dataQueue == NULL)
		    dataEnd = NULL;
		free(tmp->text);
		free(tmp);
		writeIdx = 0;
	    }
	} else {  
	    int error;		/* query the error encountered */

	    /* Some sort of error sending to remote end.
	     */
	    error = WSAGetLastError();
	    if (error == WSAEWOULDBLOCK)
		/* No problems, remote end can not accept any more
		 * data just now
		 */
		return;
	    else {
		/* Error: close socket
		 */
		socketError("send()", error);
		closeSocket();
		return;
	    }
	}
    }   
}

/* Receive some out-of-band data from the other end of the socket
 */
static void socketOobReady(void)
{
    int numRead;		/* Number of bytes read this time around */
    char oobBuffer[256]; /* Buffer for OOB data */

    /* Try to read as much as possible from the remote end.
     */
    numRead = recv(sock, oobBuffer, sizeof(oobBuffer), MSG_OOB);

    if (numRead == 0)
	/* No more data ready
	 */
	return;
    if (numRead == SOCKET_ERROR) {
	int error = WSAGetLastError();
	if (error == WSAEWOULDBLOCK)
	    /* No more data ready
	     */
	    return;
	/* Error: close socket
	 */
	socketError("recv()", error);
	closeSocket();
	return;
    }

    /* Got some OOB data from remote end
     */
    rawProcessOob((unsigned char*)oobBuffer, numRead);

    /* Assume that the OOB data was in response to ^C, eventually we
     * will get the prompt back - we must allow it to be shown be
     * restoring character flow.
     */
    if (rawGetProtocol() != protoNone && localFlowControl)
	socketSetFlowStop(FALSE);
}

/* Perform a select on the socket.  When an event occurs the WINSOCK
 * library will sent a WM_SOCK_EVENT message to the socket window
 * message proc.
 *
 * Return TRUE if the operation was successful.
 */
static int socketSelect (BOOL fConnect)
{
    LONG events;		/* socket events we are interested in */
    int rc;

    if (!fConnect)
	/* We are online - listen for close, read, write, and out-of-band
	 */
	events = FD_CLOSE|FD_READ|FD_WRITE|FD_OOB;
    else
	/* We are not connected yet, just listen for connect
	 */
	events = FD_CONNECT;

    if (WSAAsyncSelect(sock, sockWnd, WM_SOCK_EVENT, events) != SOCKET_ERROR)
	return 0;

    rc= WSAGetLastError ();
    socketError ("WSAAsyncSelect()", rc);

    return rc;
}

#if defined(_WIN32)
static void KeepAliveTime (void)
{
    LONG rc;
    DWORD mytype= 0;
    DWORD dwSysKeepAliveTime= 0;
    DWORD mysize= sizeof (dwSysKeepAliveTime);
    HKEY mykey= 0;
    DWORD keepalivesec= 10*60;	/* sec */
    BOOL fSet= TRUE;

    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\",
		0, KEY_QUERY_VALUE, &mykey);
    if (rc) goto CLOSED;

    rc= RegQueryValueEx (mykey, "KeepAliveTime",
	NULL, &mytype, (void *)&dwSysKeepAliveTime, &mysize);

    if (rc==0 && mytype == REG_DWORD && dwSysKeepAliveTime < keepalivesec*1000L) {
	fSet= FALSE;
    }
    RegCloseKey (mykey);

CLOSED:
    if (fSet) {
	struct tcp_keepalive alivepar = {1, 10*60*1000L, 1000L };
	DWORD err, dwBytesRet;

	alivepar.keepalivetime= keepalivesec * 1000L;
	rc= WSAIoctl (sock, SIO_KEEPALIVE_VALS, &alivepar, sizeof (alivepar), NULL, 0,
		&dwBytesRet, NULL, NULL);
	if (rc) {
	    err= WSAGetLastError ();
	    fprintf (stderr, "WSAIoctl error %lx\n", (unsigned long)err);
	}
    }
}
#endif

static int socketRloginBind (SOCKET sock, int af);

/* Build a new socket
 */
static int socketMakeNew (void)
{
    DWORD yes = 1;		/* ioctl parameter */
    char iptmp[64];

    remoteHasClosed = FALSE;
    /* Log that we are trying to connect
     */
    salPrints (iptmp, &addr, SALP_IPv6_BRACKETS, '/');
    logProtocol ("Connecting to %s\n", iptmp);
    socketSetState(sockConnect);

    /* Set up socket address family
     */
    /* addr.sin_family = PF_INET; <FIXME> should happen earlier */
    /* proxyAddr.sin_family = PF_INET;				*/

    /* Create a socket
     */
    sock = socket (addr.addr.sa.sa_family, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
	socketError("socket()", WSAGetLastError());
	return -1;
    }
    /* Set the socket non-blocking
     */
    if (ioctlsocket(sock, FIONBIO, &yes) == SOCKET_ERROR) {
	socketError("ioctlsocket()", WSAGetLastError());
	return -1;
    }

    { /* 20130805.LZS: keepalive on */
	BOOL optval;
	int rc;

	optval= TRUE;
	rc= setsockopt (sock, (int)SOL_SOCKET, SO_KEEPALIVE, (const void *)&optval, sizeof (optval));
	if (rc) {
	    fprintf (stderr, "setsockopt error %x\n", (int)WSAGetLastError());
	}
#if defined(_WIN32)
	KeepAliveTime();
#endif
    }

    if (rawGetProtocol() == protoTelnet) {
	/* If we are connecting to the Telnet port, disable local flow
	 * control.
	 */
	localFlowControl = FALSE;

    } else if (rawGetProtocol() == protoRlogin) {
	/* for 'rlogin' we should use a privileged port -- unless we use a proxy */
	if (! proxyEnabled ()) {
	    int rc= socketRloginBind (sock, addr.addr.sa.sa_family);

	    if (rc!=0) return -1;
	}

	/* Enable local flow control
	 */
	localFlowControl = TRUE;

    } else {
	/* Enable local flow control
	 */
	localFlowControl = TRUE;
    }

    /* We start connection with flow enabled.  Flow can not be
     * restarted with any character unless we negotiate that option.
     */
    flowStopped = FALSE;
    emulLocalFlowAny(FALSE);

    return 0;
}

static int socketRloginBind (SOCKET sock, int af)
{
	SockAddrL localAddr;
	u_short localPort = IPPORT_RESERVED; /* first port to try for
					      * local end of rlogin
					      * connection */
	int error;

#if defined(AF_INET6)
	if (af!=AF_INET && af!=AF_INET6) return -1;

	memset (&localAddr, 0, sizeof *&localAddr);
	localAddr.addr.sa.sa_family= (uint16_t)af;
	localAddr.len= af==AF_INET?
		(uint16_t)sizeof localAddr.addr.sin:
		(uint16_t)sizeof localAddr.addr.sin6;
#else
	if (af!=AF_INET) return -1;

	memset (&localAddr, 0, sizeof *&localAddr);
	localAddr.addr.sa.sa_family= (uint16_t)af;
	localAddr.len= sizeof localAddr.addr.sin;
#endif
	/* Rlogin servers will only accept connections from ports in
	 * the reserved range (below 1024).  Start at 1023 and keep
	 * trying until we either get a valid local endpoint, or we
	 * run out of ports.
	 */

	for (;;) {
	    localPort--;
	    if (localPort == IPPORT_RESERVED / 2) {
		MessageBox(termGetWnd(), "All reserved ports in use",
			   telnetAppName(),
			   MB_APPLMODAL | MB_ICONHAND | MB_RETRYCANCEL);
		return -1;
	    }

	    localAddr.addr.sin.sin_port = htons(localPort);	/* works for IPv6 too */
	    if (bind(sock, &localAddr.addr.sa, localAddr.len) == 0)
		break;

	    if ((error = WSAGetLastError()) != WSAEADDRINUSE) {
		socketError("bind()", error);
		return -1;
	    }
	}
	return 0;
}

/* Initiate an asynchronous get host by name */

static int socketGetHostByName (const char *hoststr, const char *portstr,
	SockAddrL *into)
{
    int success;
    int rc;

    memset (into,   0, sizeof *into);

/* port -- number, getservbyname -- step 1: number */
    success= 0;
    if (success!=1) {
	long nport;
	char *endptr;

	nport= strtol (portstr, &endptr, 10);
	if (*endptr=='\0') { /* numeric */
	    if (nport<=0 || nport>65535L) {
		rc= -1;				/* plain wrong, stop here */
		goto RETURN;
	    } else {
		into->addr.sin.sin_port= htons ((short)nport);	/* OK for IPv6 too */
		success= 1;
	    }
	}
    }

/* port -- number, fix value, getservbyname -- step 2: fix value */
    if (success!=1) {
	if (strcmpi (portstr, "telnet")==0) {
	    into->addr.sin.sin_port = htons (23);		/* OK for IPv6 too */
	    success= 1;

	} else if (strcmpi (portstr, "login")==0) {
	    into->addr.sin.sin_port = htons (513);		/* OK for IPv6 too */
	    success= 1;
	}
    }

/* port -- number, fix value, getservbyname -- step 3: getservbyname */
    if (success!=1) {
        struct servent *se;

	se= getservbyname (portstr, "tcp");
	if (se != NULL) {
	    into->addr.sin.sin_port = (short)se->s_port;	/* OK for IPv6 too; both in network byte-order */
	    success= 1;
	}
    }
    if (success!=1) {
	rc= -1;
	goto RETURN;
    }

/* host -- IPv4, IPv6, name -- step 1: IPv4 */
    success= 0;
    if (success!=1) {
	success= my_inet_aton (hoststr, &into->addr.sin.sin_addr);
	if (success==1) {
	    into->len= sizeof into->addr.sin;
	    into->addr.sin.sin_family= AF_INET;
	}
    }

/* host -- IPv4, IPv6, name -- step 2: IPv6 */
#if defined (_WIN32)
    if (success!=1) {
	success= my_inet_pton (AF_INET6, hoststr, &into->addr.sin6.sin6_addr);
	if (success==1) {
	    into->len= sizeof into->addr.sin6;
	    into->addr.sin6.sin6_family= AF_INET6;
	}
    }
#endif

/* host -- IPv4, IPv6, name -- step 3: name */
    if (success!=1) {
#if defined (_WIN32)
	int errcode;
	struct addrinfo hints, *ap=NULL;

	memset (&hints, 0, sizeof *&hints);
	hints.ai_flags    = AI_ADDRCONFIG;
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	ap= NULL;
	errcode= getaddrinfo (hoststr, NULL, &hints, &ap);

	if (errcode==0 && ap != NULL &&
	    ap->ai_addr && ap->ai_addrlen!=0) {

	    unsigned short saveport= into->addr.sin.sin_port; /* keep the port-number */

	    into->len= (uint16_t)ap->ai_addrlen;
	    memcpy (&into->addr, ap->ai_addr, ap->ai_addrlen);
	    into->addr.sin.sin_port= saveport;

	    success= 1;
	}
	if (ap) freeaddrinfo (ap);
#else
	if (WSAAsyncGetHostByName (sockWnd, WM_SOCK_GETHOSTBYNAME,
				   hoststr, hostbuff, sizeof hostbuff)) {
	    rc= 1;		/* just doing it */
	    goto RETURN;	/* important! */

	} else {
	    socketError("WSAAsyncGetGetHostByName()", WSAGetLastError());
	}
#endif
    }

    rc= success==1? 0: -1;

RETURN:
    return rc;
}

static int socketGotHostNameByName (const struct hostent *from, SockAddrL *into)
{
    int rc;

    if (from->h_addrtype==AF_INET && from->h_length==4 && 
	from->h_addr_list && from->h_addr_list[0]) {
	into->len= sizeof into->addr.sin;
	into->addr.sin.sin_family= AF_INET;
	memcpy (&into->addr.sin.sin_addr, from->h_addr_list[0], 4);
	rc= 0;
#if defined(AF_INET6)
    } else if (from->h_addrtype==AF_INET6 && from->h_length==16 &&
	from->h_addr_list && from->h_addr_list[0]) {
	into->len= sizeof into->addr.sin6;
	into->addr.sin6.sin6_family= AF_INET6;
	memcpy (&into->addr.sin6.sin6_addr, from->h_addr_list[0], 16);
	rc= 0;
#endif
    } else {
	rc= -1;
    }

    return rc;
}

/* Initiate an asynchronous get host by name for Proxy server
 *
 * Return TRUE if successful.
 */

/* We delay our read / write on the socket until a convenient
 * processing time.  This prevents the situation where a telnet to
 * CHARGEN totally locks up Windows.  In our case, we wait until the
 * message queue is empty.
 */
void socketProcessPending()
{
    if (readyForRead) {
	/* When we receive an FD_READ event on the socket, the code
	 * processing the event sets readyForRead TRUE.  When we are
	 * performing local flow control, and we have blocked flow, we
	 * do not want to read from the socket.  If we are not running
	 * telnet or rlogin, then there is no local flow control.
	 */
	if (rawGetProtocol() == protoNone
	    || !(localFlowControl && flowStopped)) {
            /* the following two statements are reversed in they */
            /* execution order (Khader Al-Atrash)                */
	    readyForRead = FALSE;
	    socketReadReady();
	}
    }
    if (remoteHasClosed) {
        /* if we have a proxy request in progress, proxyHasClosed() display a error */
        if (socketState == sockProxy) {
            proxyRemoteHasClosed();
        }
	closeSocket();
	/* If the user has set exit-on-disconnect, exit the application
	 */
	if (connectGetExitOnDisconnect())
	    telnetExit();
    } else if (readyForWrite) {
	/* When we receive an FD_WRITE event on the socket, the
	 * processing code sets readyForWrite TRUE.  We perform the
	 * write here, once the event queue is empty.
	 */
        /* the following two statements are reversed in they */
        /* execution order (Khader Al-Atrash)                */
	readyForWrite = FALSE;
	socketWriteReady();
    }
}

/* Return whether or not we are performing local flow control
 */
BOOL socketLocalFlowControl(void)
{
    return localFlowControl;
}

/* Set local flow control on or off
 *
 * Args:
 * local - do we wish to switch to local flow control?
 */
void socketSetLocalFlowControl(BOOL local)
{
    if (local == localFlowControl)
	/* No change in value
	 */
	return;

    if (rawGetProtocol() != protoNone && !local)
	/* Hve just switched from local flow control to remote flow
	 * control, initiate a read to catch any data that has been
	 * sent by remote end.
	 */
	readyForRead = TRUE;

    localFlowControl = local;
}

/* Stop or restart flow locally
 *
 * Args:
 * stop - should flow be stopped?
 */
void socketSetFlowStop(BOOL stop)
{
    if (stop == flowStopped)
	/* No change in value
	 */
	return;

    if (rawGetProtocol() != protoNone && localFlowControl && !stop)
	/* Have just restarted flow locally.  Initiate a read to catch
	 * any data that we have already received from the remote end
	 */
	readyForRead = TRUE;

    flowStopped = stop;
}

/* Return whether or not flow has been stopped locally
 */
BOOL socketFlowStopped(void)
{
    return flowStopped;
}

/* Display the current communications state in the status bar
 */
void socketShowStatus(void)
{
    char tmp [64];

    switch (socketState) {
    case sockOffline:
	statusSetMsg("Offline");
	termSetTitle("Dave's Telnet - offline", DTCHAR_ASCII);
	break;
    case sockGetProxyHostName:
	statusSetMsg("Resolving Proxy server host name %s", proxyGetHost());
	break;
    case sockGetHostName:
	statusSetMsg("Resolving host name %s", socketHost);
	break;
/*  case sockGetServName:
 *	statusSetMsg("Resolving service name %s", socketService);
 *	break;
 */
    case sockConnect:
	if (proxyEnabled ()) salPrints (tmp, &proxyAddr, SALP_IPv6_BRACKETS, '/');
	else		     salPrints (tmp, &addr,      SALP_IPv6_BRACKETS, '/');
	statusSetMsg("Connecting to %s", tmp);
        break;
    case sockProxy:
	statusSetMsg("Sending Proxy request...");
	break;
    case sockOnline:
	if (proxyEnabled() && proxySupportHostname()) {
	    statusSetMsg("Online to %s/%s",
		socketHost, htons(addr.addr.sin.sin_port));
	} else {
	    salPrints (tmp, &addr, SALP_IPv6_BRACKETS, '/');
	    statusSetMsg("Online to %s", tmp);
	}
	break;
    }
}

/* Set and display the socket state
 */
static void socketSetState(SocketState state)
{
    socketState = state;
    if (state >= maxSocketState) maxSocketState= state;
    socketShowStatus();
}

static int socketContinueConnect (WPARAM w, LPARAM l);

/* Window procedure for the socket window
 */
static LRESULT CALLBACK socketWndProc
	(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    WORD event;

    switch (message) {
    case WM_SOCK_GETHOSTBYNAME:
    case WM_SOCK_GETSERVBYNAME:
	/* Have just resolved the address of the remote end.  Save the
	 * data and then request the service port.  When the
	 * getservbyname operation finishes, windows sends the
	 * WM_SOCK_GETSERVBYNAME message back here.
	 */
	socketContinueConnect (wparam, lparam);
	break;
		
    case WM_SOCK_EVENT:
	event = WSAGETSELECTEVENT(lparam);

	switch (event) {
	case FD_CLOSE:
	    /* Socket has been closed by the remote end.
	     */
	    remoteHasClosed = TRUE;
	    break;

	case FD_CONNECT:
	    socketContinueConnect (wparam, lparam);
	    break;

	case FD_READ:
	    /* There is some data ready to be read from the remote
	     * end.  Do not read the data now, the message loop will
	     * service the network IO once all GUI interaction is
	     * complete.
	     */
	    readyForRead = TRUE;
	    break;

	case FD_WRITE:
	    /* The socket is ready for us to send some more data to
	     * the remote end.  Handle this as per FD_READ
	     */
	    readyForWrite = TRUE;
	    break;

	case FD_OOB:
	    /* Received some urgent data from remote end.  Probably
	     * result of pressing ^C.
	     */
	    socketOobReady();
	    break;
	}
	break;

    case WM_DESTROY:
	/* Our window has been destroyed, close up the socket
	 */
	closeSocket();
	return 0;

    default:
	return DefWindowProc(wnd, message, wparam, lparam);
    }
    return 0;
}

/* Return the remote host passed into socketConnect()
 */
char* socketGetHost()
{
    return socketHost;
}

/* Return the remote service passed into socketConnect()
 */
char* socketGetPort()
{
    return socketService;
}

/* Return the handle of the socket
 */
HANDLE socketGetHandle(void)
{
    return (HANDLE)sock;
}

char *socketGetAddrStr (char *into, int flags, char portsep)
{
    SockAddrL *paddr;
    int opts= 0;

    if ((flags&SOCGA_WHITCH)==SOCGA_LOCAL) paddr= &localAddr;
    else				   paddr= &addr;

    if (flags&SOCGA_IPv6_BRACKETS) opts |= SALP_IPv6_BRACKETS;
    if (flags&SOCGA_NO_PORT)	   opts |= SALP_NO_PORT;

    salPrints (into, paddr, opts, portsep);
    return into;
}

/* Initiate a connection to the remote end.
 *
 * Args:
 * host -    the hostname or IP address (as string) to connect to
 * service - te service name or port number (as string) to connect to
 */
int socketConnect (const char* host, const char* service)
{
    int rc;

    /* Store the session details for later reference
     */
    strcpy (socketHost, host);
    strcpy (socketService, service);
    socketSetState (sockOffline);

    socketStateExt.proxy_host_resolv = proxyEnabled()? JP_TODO: JP_DONE;
    socketStateExt.host_resolv       = JP_TODO;
    socketStateExt.connect           = JP_TODO;
    socketStateExt.proxy_req         = proxyEnabled()? JP_TODO: JP_DONE;

    rc= socketContinueConnect (0, 0);
    return rc;
}

#define CONT_RC_SUCCESS  0
#define CONT_RC_WAIT	 1	/* in progress; WSAAsyncX in progress, call me again when window message arrives */
#define CONT_RC_ERR	-1	/* unspecified error */
/* any other value: error code */

static int socketContinueConnect (WPARAM wParam, LPARAM lParam)
{
    int rc= -1;

    if (socketStateExt.proxy_host_resolv == JP_TODO) {
	rc= socketGetHostByName (proxyGetHost (), proxyGetPort (), &proxyAddr);
	if (rc==0) {
	    socketStateExt.proxy_host_resolv = JP_DONE;
	} else if (rc==1) {
	    socketSetState (sockGetProxyHostName);
	    socketStateExt.proxy_host_resolv = JP_DOING;
	    goto RETURN;
	} else {
	    goto RETURN;
	}

    } else if (socketStateExt.proxy_host_resolv == JP_DOING) {
	unsigned short errcode= WSAGETASYNCERROR (lParam);

	if (errcode) {
	    rc= errcode;
	    goto RETURN;
	}
	rc= socketGotHostNameByName ((struct hostent *)hostbuff, &proxyAddr);
	if (rc) {
	    goto RETURN;
	}
	socketStateExt.proxy_host_resolv = JP_DONE;
    }

    if (socketStateExt.host_resolv == JP_TODO) {
	rc= socketGetHostByName (socketHost, socketService, &addr);
	if (rc==0) {
	    socketStateExt.host_resolv = JP_DONE;
	} else if (rc==1) {
	    socketSetState (sockGetHostName);
	    socketStateExt.host_resolv = JP_DOING;
	    goto RETURN;
	} else {
	    goto RETURN;
	}
    } else if (socketStateExt.host_resolv == JP_DOING) {
	int rc= WSAGETASYNCERROR (lParam);

	if (rc) goto RETURN;

	rc= socketGotHostNameByName ((struct hostent *)hostbuff, &addr);
	if (rc) goto RETURN;

	socketStateExt.host_resolv = JP_DONE;
    }

    if (socketStateExt.connect == JP_TODO) {
	SockAddrL *dst;

	rc= socketMakeNew ();
	if (rc) goto RETURN;

	dst = proxyEnabled() ? &proxyAddr : &addr;
	rc= connect (sock, &dst->addr.sa, dst->len);
	if (rc==0) {
	    socketStateExt.connect = JP_DONE;
CONNECTED:  {
		int addrlen;

		addrlen = sizeof (localAddr.addr);
		getsockname (sock, &localAddr.addr.sa, &addrlen);
		localAddr.len= (uint16_t)addrlen;
	    }
	} else {
	    rc= WSAGetLastError();
	    if (rc != WSAEWOULDBLOCK) goto RETURN;

	    rc= socketSelect (TRUE);	/* fConnect==TRUE: only FD_CONNECT */
	    if (rc) goto RETURN;

	    socketStateExt.connect = JP_DOING;
	    rc= 1;
	    goto RETURN;
	}

    } else if (socketStateExt.connect == JP_DOING) {
	int rc= WSAGETASYNCERROR (lParam);

	if (rc==0) {
	    goto CONNECTED;
	} else {
	    socketError ("connect()", rc);
	    goto RETURN;
	}
    }

/* connected to proxy/server */

/* proxy connect */
    if (socketStateExt.proxy_req == JP_TODO) {
	int ret;
	char *dstHost;
	char iptmp [64];

	socketSetState (sockProxy);
	if (proxySupportHostname ()) {
	    dstHost= socketHost;
	} else {
	    salPrints (iptmp, &addr, SALP_NO_PORT, 0);
	    dstHost= iptmp;
	}
	ret = proxyStartRequest (dstHost, ntohs (addr.addr.sin.sin_port));
	if (ret != PROXY_OK) {
	    rc= -1;
	    goto RETURN;
	}
	socketStateExt.proxy_req = JP_DONE;

/*  } else if (socketStateExt.proxy_req == JP_DOING) { -- for now, no asynchronous operation for proxy */
    }

    socketSetState(sockOnline);
    telnetSetTitle();
    rawStartSession();

    socketSelect(FALSE);	/* fConnect=FALSE: normal operation */
    rc= 0;

RETURN:
    if (rc!=0 && rc!=1) {
	if (sock!=INVALID_SOCKET) closeSocket ();
	socketSetState (sockOffline);
    }
    return rc;
}

/* Queue some data to be written to the remote end.
 *
 * Args:
 * text - the data to be sent to the remote end
 * len -  the amount of data to be sent
 */
void socketWrite(const char* text, int len)
{
    Data* data;			/* queue entry for data */
    BOOL wasEmpty = dataQueue == NULL; /* was the queue empty when function called */

    ASSERT(text != NULL);
    ASSERT(len > 0);
    ASSERT(len < INT_MAX);
    if (socketState != sockOnline)
	/* Do not queue data if we are not connected
	 */
	return;

    /* Allocate a queue entry and get a local copy of the data to be
     * queued
     */
    data = (Data *)xmalloc(sizeof(*data));
    ASSERT(data != NULL);
    memset(data, 0, sizeof(*data));
    data->text = (unsigned char *)xmalloc(len);
    ASSERT(data->text != NULL);
    data->len = len;
    memcpy(data->text, text, len);

    /* Add the data to the end of the queue
     */
    if (dataEnd != NULL)
	dataEnd->next = data;
    else
	dataQueue = data;
    dataEnd = data;

    /* If there is already data in the queue, just queue the data.  If
     * this is the only data, and the socket is ready for writing then
     * try to write this data.
     */
    if (wasEmpty)
	socketWriteReady();
}

/* Check for a local X-server */
BOOL socketHaveXServer (void)
{
    SOCKET s;
    SockAddrL tmpaddr;
    int rc;
    DWORD err;
    static BOOL fTested= FALSE, fHave= FALSE;

    if (fTested) return fHave;

    memset (&tmpaddr, 0, sizeof *&tmpaddr);
    tmpaddr.len= localAddr.len;
    tmpaddr.addr.sa.sa_family= localAddr.addr.sa.sa_family;

    s = socket (tmpaddr.addr.sa.sa_family, SOCK_STREAM, 0);
    if (s==(SOCKET)-1) {
	fHave = FALSE;
	fTested = TRUE;
	return fHave;
    }

    tmpaddr.addr.sin.sin_port = htons (6000); /* X-server' port; works for IPv6 too */
    rc = bind (s, &tmpaddr.addr.sa, tmpaddr.len);
    if (rc==-1) {
	err = WSAGetLastError ();
	if (err == WSAEADDRINUSE) fHave = TRUE;
    }
    closesocket (s);
    fTested = TRUE;
    return fHave;
}

/* Register the window class that will receive the socket messages
 */
BOOL socketInitClass(HINSTANCE instance)
{
    WNDCLASS wc;

    wc.style = 0;
    wc.lpfnWndProc = socketWndProc;
    wc.cbClsExtra = wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = NULL;
    wc.hCursor = 0;
    wc.hbrBackground = 0;
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = socketWinClass;
    return RegisterClass(&wc);
}

/* Initialise WINSOCK
 */
BOOL socketInit(HINSTANCE instance)
{
    int ret;

    if ((ret = WSAStartup(SOCKLIB_VERSION, &WSAData))!=0) {
	socketError("WSAStartup()", ret);
	return FALSE;
    }
    sockWnd = CreateWindow(socketWinClass, "", 0,
			   0, 0, 0, 0,
			   NULL, NULL, instance, NULL);
    if (!sockWnd) {
	if (WSACleanup())
	    socketError("WSACleanup()", WSAGetLastError());
	return FALSE;
    }
    return TRUE;
}

/* Unregister with WINSOCK
 */
void socketCleanup()
{
    if (WSAIsBlocking()) {
	WSACancelBlockingCall();
    }
    WSACleanup();
}

static int salPrints (char *to, const SockAddrL *from, int opts, int portsep)
{
    return saPrints (to, &from->addr, opts, portsep);
}

static int saPrints (char *to, const SockAddr *from, int opts, int portsep)
{
    int len;

    if (from->sa.sa_family == AF_INET) {
	len = sprintf (to, "%s", inet_ntoa (from->sin.sin_addr));
	if (!(opts&SALP_NO_PORT) && portsep) {
	    len += sprintf (to+len,
		"%c%d", (char)portsep, ntohs (from->sin.sin_port));
	}
#if defined (AF_INET6)
    } else if (from->sa.sa_family == AF_INET6) {
	char buff [512];

	my_inet_ntop (AF_INET6, &from->sin6.sin6_addr, buff, sizeof buff);

	if (opts&SALP_IPv6_BRACKETS) len = sprintf (to, "[%s]", buff);
	else			     len = sprintf (to, "%s", buff);

	if (!(opts&SALP_NO_PORT) && portsep) {
	    len += sprintf (to+len,
		"%c%d", portsep, ntohs (from->sin.sin_port));
	}
#endif
    } else {
	*to = '\0';
	len = 0;
    }
    return len;
}

#if defined(_WIN32)
static int my_inet_ntop6 (const void *pfrom, char *dst)
{
    int maxzeropos=0, maxzerolen=0;
    int aktzeropos=0;
    int inzero= 0;
    char *q= dst;
    int i;
    unsigned short from[8];

    for (i=0; i<8; ++i) {
	from[i]= ntohs (((unsigned short *)pfrom)[i]);
    }

    for (i=0; i<8; ++i) {
	if (from[i]==0) {
	    if (!inzero) {
		inzero= 1;
		aktzeropos= i;
	    }
	    if (i+1-aktzeropos > maxzerolen) {
		maxzeropos= aktzeropos;
		maxzerolen= i+1-aktzeropos;
	     }
	} else {
	    inzero= 0;
	}
    }

    if (maxzeropos==0 && maxzerolen>=5 &&
	(from[5]==0x0000 || from[5]==0xffff) &&
	 from[6]!=0x0000) {
	unsigned char *fchar=  (unsigned char *)pfrom+12;

	q += sprintf (q, "::");
	if (from[5]) {
	    q += sprintf (q, "%x:", from[5]);
	}
	q += sprintf (q, "%d.%d.%d.%d",
	    fchar[0], fchar[1], fchar[2], fchar[3]);
    } else {
	int starti;

	for (starti= 0, i=starti; i<maxzeropos; ++i) {
	    q += sprintf (q, "%s%x"
			 , i==starti? "": ":"
			 , from[i]
			 );
	}
	if (maxzerolen) {
	    q += sprintf (q, "::");
	}
	for (starti= maxzeropos+maxzerolen, i=starti; i<8; ++i) {
	    q += sprintf (q, "%s%x"
			 , i==starti? "": ":"
			 , from[i]
			 );
	}
    }

    return (int)(q-dst);
}

static char *my_inet_ntop (int af, const void *from, char *dst, unsigned dstsize)
{
    char tmp[64];
    int tmplen;

    if (af!=AF_INET && af!=AF_INET6) {
	WSASetLastError (WSAEAFNOSUPPORT);
	return NULL;
    }

    if (af==AF_INET) {
	const unsigned char *p= from;

	tmplen= sprintf (tmp, "%d.%d.%d.%d",
	    p[0], p[1], p[2], p[3]);

    } else if (af==AF_INET6) {
	tmplen= my_inet_ntop6 (from, tmp);
    }

    if (dstsize < (unsigned)(tmplen+1)) {
	WSASetLastError (WSAEFAULT);
	return NULL;
    } else {
	strcpy (dst, tmp);
	return dst;
    }
}

static int my_inet_pton (int af, const char *src, void *dst)
{
    struct addrinfo hints, *ap=NULL;
    int errcode;
    int success= 0;

    memset (&hints, 0, sizeof *&hints);
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_family= af;
    hints.ai_socktype = SOCK_STREAM;	/* less answers: spare some time/memory */

    errcode= getaddrinfo (src, NULL, &hints, &ap);
    if (errcode==0 && ap!=0 && ap->ai_addrlen && ap->ai_addr) {
	switch (ap->ai_family) {
	case AF_INET:
	    success= 1;
	    *(struct in_addr *)dst= ((struct sockaddr_in *)ap->ai_addr)->sin_addr;
	    break;

	case AF_INET6:
	    success= 1;
	    *(struct in_addr6 *)dst= ((struct sockaddr_in6 *)ap->ai_addr)->sin6_addr;
	    break;
	}
    }
    if (ap) freeaddrinfo (ap);
    return success;
}
#endif

static int my_inet_aton (const char *from, struct in_addr *into)
{
    int success;
    uint32_t tmp;

    tmp= (uint32_t)inet_addr (from);
    *(uint32_t *)into= tmp;
    success= (uint32_t)tmp != 0xFFFFFFFFUL;	/* byte-order independent;) */
    return success;
}
