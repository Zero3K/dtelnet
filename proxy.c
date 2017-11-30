
/* proxy.c
 * 2001-10-27 Enrique Grandes
 * Manage Proxy protocols and Proxy dialog
 */
#if defined (_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <winsock.h>
#endif

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "resource.h"
#include "proxy.h"
#include "socksdef.h"
#include "utils.h"
#include "dtelnet.h"
#include "term.h"
#include "dialog.h"
#include "socket.h"

#define HOSTNAME_LEN 	128
#define USERNAME_LEN 	24
#define PASSWORD_LEN 	24
#define PORT_LEN 	6

/* Protocol state handler
 */
typedef int (StateHandler)(void);

/* dialog variables
 */
typedef enum {
    PP_DISABLED,    /* Direct connection */
    PP_SOCKS4,	    /* SOCKS v4 */
    PP_SOCKS4A,	    /* SOCKS v4 with A Extension */
    PP_SOCKS5,	    /* SOCKS v5 */
    PP_HTTP	    /* HTTP Proxy */
} ProxyProtocol;

typedef struct {
    ProxyProtocol protocol;	  /* protocol */
    char host[HOSTNAME_LEN];      /* proxy server address */
    char username[USERNAME_LEN];  /* username */
    char password[PASSWORD_LEN];  /* password */
    char port[PORT_LEN]; 	  /* proxy server port */
} Proxy;

/* Protocol strings
 */
static char disabledStr[]  = "Disabled";
static char socks4Str[]    = "SOCKS v4";
static char socks4aStr[]   = "SOCKS v4A";
static char socks5Str[]    = "SOCKS v5";
static char httpStr[]	   = "HTTP Proxy";

/* .INI file Strings 
 */
static char proxyStr[]     = "Proxy";
static char protocolStr[]  = "Protocol";
static char hostStr[]      = "Host";
static char usernameStr[]  = "Username";
static char passwordStr[]  = "Password";
static char portStr[]      = "Port";

static HWND hProxyDlg = NULL; 		/* dialog handle */
static Proxy proxy;			/* dialog variables */
static char dstHost[HOSTNAME_LEN]; 	/* host for the connect request */
static int dstPort; 			/* port for the connect request */
static int handlerIndex = 0; 		/* state handler index */
static int emptyLine;			/* for skip HTTP Proxy mime header */
static StateHandler **curProto = NULL;	/* pointer to current protocol handlers */

/* Error definitions
 */
typedef enum {
    PE_DATA,
    PE_VERSION,
    PE_CLOSE,
    PE_FAILED,
    PE_USERID,
    PE_IDENT,
    PE_METHODS,
    PE_AUTH,
    PE_SERVFAULT,
    PE_ACCESS,
    PE_NETUNREACH,
    PE_CONNREFUSED,
    PE_TLLEXPIRED,
    PE_OPNOTSUP,
    PE_ATNOTSUP,
    PE_HTTP
} ProxyError ;

static char *errorString[] = {
    "Invalid data from Proxy server.",
    "Proxy server has given an invalid version.",
    "Proxy server has closed the connection unexpectedly.",
    "SOCKS: conection failed.",
    "SOCKS: bad userid.",
    "SOCKS: ident error.",
    "SOCKS: no acceptable autentication methods.",
    "SOCKS: invalid username/password.",
    "SOCKS: undefined server error.",
    "SOCKS: access denied.",
    "SOCKS: network unreachable.",
    "SOCKS: connection refused.",
    "SOCKS: connection timedout.",
    "SOCKS: operation not supported.",
    "SOCKS: address type not supported.",
    "HTTP Proxy error.\nResponse: %s %s %s "
};

/* Report a error to the user
 */
static void proxyError(ProxyError error) 
{
    MessageBox(
	termGetWnd(), 
	errorString[error], 
	telnetAppName(), 
	MB_APPLMODAL | MB_ICONHAND | MB_OK
    );
}

/* Send the SOCKS4 CONNECT request
 */
static int socks4SendConnectRequest(void) 
{
    int ret, len;
    struct s4Packet req;
    /* fill the packet */
    req.version = S4_VERSION;
    req.command = S4_REQ_CONNECT;
    req.port = htons((u_short) dstPort);
    req.addr = inet_addr(dstHost);
    /* copy the userid */
    strcpy(req.userid, proxy.username);
    len = strlen(proxy.username) + 1;
    /* if socks4a extension is enabled, copy the hostname */
    if (proxy.protocol == PP_SOCKS4A) {
	if (req.addr == INADDR_NONE) { 
	    req.addr = inet_addr("0.0.0.1");
	    strcpy(&req.userid[len], dstHost);
	    len += strlen(dstHost) + 1;
	}
    }
    len += 8;
    /* send to server */
    ret = send((SOCKET)socketGetHandle(), (char *) &req, len, 0);
    if (ret == -1) {
	socketError("send()", WSAGetLastError());
	return PROXY_ERROR;
    }
    handlerIndex++;
    return PROXY_OK;
}

/* Get the SOCKS4 CONNECT reply
 */
static int socks4GetConnectReply(void)
{
    int ret, len;
    struct s4Packet rep;
    /* receive the packet */
    len = sizeof(rep) - sizeof(rep.userid);
    ret = recv((SOCKET)socketGetHandle(), (char *) &rep, len, 0);
    if (ret == -1) {
	socketError("recv()", WSAGetLastError());
	return PROXY_ERROR;
    }
    /* size check */
    if (ret != len) {
	proxyError(PE_DATA);
	return PROXY_ERROR;
    }
    /* eval server reply */
    switch (rep.command)
    {
    case S4_REP_SUCCESS:
	return PROXY_CONNECTED;
    case S4_REP_EFAILED:
	proxyError(PE_FAILED);
	break;
    case S4_REP_EIDENT:
	proxyError(PE_IDENT);
	break;
    case S4_REP_EBADUSRID:
	proxyError(PE_USERID);
	break;
    default:
	proxyError(PE_DATA);
	break;
    }
    return PROXY_ERROR;
}

/* State machine for SOCKS4
 */
static StateHandler *socks4Handler[] = {
    socks4SendConnectRequest,
    socks4GetConnectReply
};

/* Send the SOCKS5 CONNECT request
 */
static int socks5SendConnectRequest(void)
{
    char* ptr;
    int ret, len;
    struct s5Packet req;
    unsigned short *ptr16;
    unsigned long *ptr32;
    /* fill the packet */
    req.version = S5_VERSION;
    req.command = S5_REQ_CONNECT;
    req.reserved = 0;
    if (inet_addr(dstHost) == INADDR_NONE) {
	/* fill req.dst with hostname length, hostname and the port */
	req.addr_type = S5_AT_DOMAINNAME;
	ptr = req.dst;
	len = strlen(dstHost);
	*ptr++ = (char)len;
	memcpy(ptr, dstHost, len);
	ptr += len;
	ptr16 = (unsigned short*) ptr;
	*ptr16 = htons((u_short) dstPort);
	len = 4 + 1 + len + 2;
    }
    else {
	/* fill req.dst with the ip address and the port */
	req.addr_type = S5_AT_IPV4;
	ptr32 = (unsigned long *) &req.dst;
	*ptr32++ = inet_addr(dstHost);
	ptr16 = (unsigned short *) ptr32;
	*ptr16 = htons((u_short) dstPort);
	len = 4 + 4 + 2;
    }
    /* send to server */
    ret = send((SOCKET)socketGetHandle(), (char *) &req, len, 0);
    if (ret == -1) {
	socketError("send()", WSAGetLastError());
	return PROXY_ERROR;
    }
    handlerIndex++;
    return PROXY_OK;
}

/* Get the SOCKS5 CONNECT reply
 */
static int socks5GetConnectReply(void)
{
    int ret;
    struct s5Packet rep;
    /* receive the packet */
    ret = recv((SOCKET)socketGetHandle(), (char *) &rep, sizeof(rep), 0);
    if (ret == -1) {
	socketError("recv()", WSAGetLastError());
	return PROXY_ERROR;
    }
    /* min size check */
    if (ret < 10) {
	proxyError(PE_DATA);
	return PROXY_ERROR;
    }
    /* eval server reply */
    switch (rep.command)
    {
    case S5_REPLY_SUCCESS:
	return PROXY_CONNECTED;
    case S5_REPLY_ESERVFAULT:
	proxyError(PE_SERVFAULT);
	break;
    case S5_REPLY_EACCESS:
	proxyError(PE_ACCESS);
	break;
    case S5_REPLY_ENETUNREACH:
	proxyError(PE_NETUNREACH);
	break;
    case S5_REPLY_ECONNREFUSED:
	proxyError(PE_CONNREFUSED);
	break;
    case S5_REPLY_ETLLEXPIRED:
	proxyError(PE_TLLEXPIRED);
	break;
    case S5_REPLY_EOPNOTSUP:
	proxyError(PE_OPNOTSUP);
	break;
    case S5_REPLY_EATNOTSUP:
	proxyError(PE_ATNOTSUP);
	break;
    default:
	proxyError(PE_DATA);
	break;
    }
    return PROXY_ERROR;
}

/* Send the SOCKS5 username/password autentication
 */
static int socks5SendAuthRequest(void)
{
    char *ptr;
    int ret, len, packlen = 3;
    char buf[256];
    /* fill the packet, length is variable */
    ptr = buf;
    /* set username/password version */
    *ptr++ = S5_AUTH_UP_VERSION;
    /* set  username length */
    len = strlen(proxy.username);
    *ptr++ = (char)len;
    /* set username */
    memcpy(ptr, proxy.username, len);
    ptr += len;
    packlen += len;
    /* set password length */
    len = strlen(proxy.password);
    *ptr++ = (char)len;
    /* set password */
    memcpy(ptr, proxy.password, len);
    packlen += len;
    /* send to server */
    ret = send((SOCKET)socketGetHandle(), buf, packlen, 0);
    if (ret == -1) {
	socketError("send()", WSAGetLastError());
	return PROXY_ERROR;
    }
    handlerIndex++;
    return PROXY_OK;
}

/* Get the SOCKS5 autentication method selected by server
 */
static int socks5GetAuthMethod(void)
{
    int ret;
    struct s5AuthMethodRep rep;
    /* receive the packet */
    ret = recv((SOCKET)socketGetHandle(), (char *) &rep, sizeof(rep), 0);
    if (ret == -1) {
	socketError("recv()", WSAGetLastError());
	return PROXY_ERROR;
    }
    /* size check */
    if (ret != sizeof(rep)) {
	proxyError(PE_DATA);
	return PROXY_ERROR;
    }
    /* eval server reply */
    switch (rep.method)
    {
    case S5_AUTH_NOACCEPTABLE:
	proxyError(PE_METHODS);
	break;
    case S5_AUTH_NOTREQUIRED:
	handlerIndex++;
	return socks5SendConnectRequest();
    case S5_AUTH_USERPASSWORD:
	return socks5SendAuthRequest();
    default:
	proxyError(PE_DATA);
	break;
    }
    return PROXY_ERROR;
}

/* Get the SOCKS5 username/password autentication reply
 */
static int socks5GetAuthReply(void)
{
    int ret;
    struct s5UPAuthRep rep;
    /* receive the packet */
    ret = recv((SOCKET)socketGetHandle(), (char *) &rep, sizeof(rep), 0);
    if (ret == -1) {
	socketError("recv()", WSAGetLastError());
	return PROXY_ERROR;
    }
    /* size check */
    if (ret != sizeof(rep)) {
	proxyError(PE_DATA);
	return PROXY_ERROR;
    }
    /* eval server reply */
    switch (rep.status) 
    {
    case S5_AUTH_UP_SUCCESS:
	return socks5SendConnectRequest();
    case S5_AUTH_UP_EFAILED:
	proxyError(PE_AUTH);
	break;
    default:
	proxyError(PE_DATA);
	break;
    }
    return PROXY_ERROR;
}

/* Send the SOCKS5 supported autentication methods
 */
static int socks5SendAuthMethods(void)
{
    int ret;
    struct s5AuthMethodReq req;
    /* set supported autentication methods (only username/password) */
    req.version = S5_VERSION;
    req.nmethods = 1;
    req.methods[0] = S5_AUTH_USERPASSWORD;
    /* send to server */
    ret = send((SOCKET)socketGetHandle(), (char *) &req, sizeof(req), 0);
    if (ret == -1) {
	socketError("send()", WSAGetLastError());
	return PROXY_ERROR;
    }
    handlerIndex++;
    return PROXY_OK;
}

/* State machine for SOCKS5
 */
static StateHandler *socks5Handler[] = {
    socks5SendAuthMethods,
    socks5GetAuthMethod,
    socks5GetAuthReply,
    socks5GetConnectReply
};

/* Send an HTTP Proxy connect request
 */
static int httpSendConnectRequest(void)
{
    int ret;
    char buf[256];

    sprintf(buf, "CONNECT %s:%d HTTP/1.0\r\n\r\n", dstHost, dstPort);
    ret = send((SOCKET)socketGetHandle(), buf, strlen(buf), 0);
    if (ret == -1) {
	socketError("send()", WSAGetLastError());
	return PROXY_ERROR;
    }
    handlerIndex++;

    return PROXY_OK;
}

/* Get HTTP Proxy reply from server
 */
static int httpGetConnectReply(void)
{
    unsigned i;
    int ret;
    char ch, msg[128];
    char buf[256], *http, *reply, *desc;

    for ( i = 0 ; ; ) 
    {
	ret = recv((SOCKET)socketGetHandle(), &ch, 1, 0);
	if (ret == -1) {
	    socketError("recv()", WSAGetLastError());
	    return PROXY_ERROR;
	}
	if (ch == '\n') {
	    buf[i] = 0;
	    break;
	}
	else if (ch != '\r') {
	    if (i < (sizeof(buf) - 1)) {
		buf[i++] = ch;
	    }
	}
    }
    http = buf;
    reply = strchr(http, ' ');
    if (!reply) {
	proxyError(PE_DATA);
	return PROXY_ERROR;
    }
    *reply++ = '\0';
    desc = strchr(reply, ' ');
    if (desc) {
	*desc++ = '\0';
    }
    if (!strcmp(reply, "200")) {
	emptyLine = TRUE;
	handlerIndex++;
	return PROXY_OK;
    }
    sprintf(msg, errorString[PE_HTTP], http, reply, desc);
    MessageBox(telnetGetWnd(), msg, telnetAppName(), MB_APPLMODAL | MB_ICONHAND | MB_OK);

    return PROXY_ERROR;
}

/* Skip HTTP Proxy mime header
 */
static int httpSkipMimeHeader(void)
{
    int ret;
    char ch;

    while (1) {
	ret = recv((SOCKET)socketGetHandle(), &ch, 1, 0);
	if (ret == -1) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) {
		return PROXY_OK;
	    }
	    socketError("recv()", WSAGetLastError());
	    return PROXY_ERROR;
	}
	if (ch == '\n') {
	    if (emptyLine) {
		break;
	    }
	    emptyLine = TRUE;
	}
	else if (ch != '\r') {
	    emptyLine = FALSE;
	}
    }
    return PROXY_CONNECTED;
}

/* State machine for HTTP Proxy
 */
static StateHandler *httpHandler[] = {
    httpSendConnectRequest,
    httpGetConnectReply,
    httpSkipMimeHeader,
};

/* Handle Proxy protocols
 */
int proxyProtocolHandler(void)
{
    return curProto[handlerIndex]();
}

/* Start the request
 */
int proxyStartRequest(char *host, int port)
{
    handlerIndex = 0;
    dstPort = port;
    memset(dstHost, 0, sizeof(dstHost));
    strncpy(dstHost, host, sizeof(dstHost)-1);
    switch (proxy.protocol) 
    {
    case PP_SOCKS4:
    case PP_SOCKS4A:
        curProto = socks4Handler;
	break;
    case PP_SOCKS5:
        curProto = socks5Handler;
	break;
    case PP_HTTP:
        curProto = httpHandler;
	break;
    case PP_DISABLED:
	curProto = NULL;	/* Not sure. LZS 20130515 */
    }
    return proxyProtocolHandler();
}

/* Remote end has closed the connection with a request in progress
 */
void proxyRemoteHasClosed(void) {
    proxyError(PE_CLOSE);
}

/* Enable ok button only if hostname and port have text
 */
static void proxyDlgEnableOk(HWND dlg)
{
    int len;
    BOOL enabled = TRUE;
    char port[PORT_LEN];
    char host[HOSTNAME_LEN];
    int sel;

    sel = (int)SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_GETCURSEL, 0, 0);
    if (sel != PP_DISABLED) 
    {
	len = GetDlgItemText(dlg, IDC_PROXY_PORT, port, sizeof(port));
	if (len > 0) {
	    len = GetDlgItemText(dlg, IDC_PROXY_HOSTNAME, host, sizeof(host));
	    if (len > 0) {
		if (!isNonBlank(port) || !isNonBlank(host)) {
		    enabled = FALSE;
		}
	    } else {
    		enabled = FALSE;
	    }
	} else {
	    enabled = FALSE;
	}
    }
    EnableWindow(GetDlgItem(dlg, IDOK), enabled);
}

/* Enable/disable edit controls
 */
static void proxyDlgEnableEdit(HWND dlg)
{
    int protocol;

    protocol = (int)SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_GETCURSEL, 0, 0);
    EnableWindow(GetDlgItem(dlg, IDC_PROXY_HOSTNAME), protocol != PP_DISABLED);
    EnableWindow(GetDlgItem(dlg, IDC_PROXY_USERID), protocol != PP_DISABLED && protocol != PP_HTTP);
    EnableWindow(GetDlgItem(dlg, IDC_PROXY_PASSWORD), protocol == PP_SOCKS5);
    EnableWindow(GetDlgItem(dlg, IDC_PROXY_PORT), protocol != PP_DISABLED);
}

/* Proxy dialog proc
 */
static DIALOGRET CALLBACK proxyDlgProc(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam) 
{
    switch (message)
    {
    case WM_INITDIALOG:
	/* Flag that we have created the dialog and register the dialog window handle. 
	 */
	hProxyDlg = dlg;
	dialogRegister(dlg);
	/* Initialise the dialog controls 
	 */
	SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_ADDSTRING, 0, (LPARAM) disabledStr);
	SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_ADDSTRING, 0, (LPARAM) socks4Str);
	SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_ADDSTRING, 0, (LPARAM) socks4aStr);
	SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_ADDSTRING, 0, (LPARAM) socks5Str);
	SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_ADDSTRING, 0, (LPARAM) httpStr);
        SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_SETCURSEL, proxy.protocol, 0);
	proxyDlgEnableEdit(dlg);
        proxyDlgEnableOk(dlg);
	SetDlgItemText(dlg, IDC_PROXY_PORT, proxy.port);
	SetDlgItemText(dlg, IDC_PROXY_HOSTNAME, proxy.host);
	SetDlgItemText(dlg, IDC_PROXY_USERID, proxy.username);
	SetDlgItemText(dlg, IDC_PROXY_PASSWORD, proxy.password);
	return TRUE;

    case WM_COMMAND:
	switch (LOWORD(wparam))
	{
	case IDC_PROXY_HOSTNAME:
	case IDC_PROXY_PORT:
#ifdef WIN32
	    if (HIWORD(wparam) != EN_CHANGE)
		break;
#else
	    if (HIWORD(lparam) != EN_CHANGE)
		break;
#endif
	    proxyDlgEnableOk(dlg);
	    break;

	case IDC_PROXY_PROTOCOL:
#ifdef WIN32
	    if (HIWORD(wparam) != CBN_SELCHANGE)
		break;
#else
	    if (HIWORD(lparam) != CBN_SELCHANGE)
		break;
#endif
	    proxyDlgEnableOk(dlg);
	    proxyDlgEnableEdit(dlg);
	    break;

	case IDOK:
	    /* Read all dialog controls into the current session 
	     */
	    proxy.protocol = SendDlgItemMessage(dlg, IDC_PROXY_PROTOCOL, CB_GETCURSEL, 0, 0);
            GetDlgItemText(dlg, IDC_PROXY_PORT, proxy.port, sizeof(proxy.port));
	    GetDlgItemText(dlg, IDC_PROXY_HOSTNAME, proxy.host, sizeof(proxy.host));
	    GetDlgItemText(dlg, IDC_PROXY_USERID, proxy.username, sizeof(proxy.username));
	    GetDlgItemText(dlg, IDC_PROXY_PASSWORD, proxy.password, sizeof(proxy.password));

	case IDCANCEL:
	    /* Destroy the dialog and unregister the dialog handle. 
	     */
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    hProxyDlg = NULL;
	    return TRUE;

	}
   }
   return FALSE;
}

/* display the Proxy dialog 
 */
void showProxyDialog(HINSTANCE instance, HWND wnd)
{ 
    if (!hProxyDlg) {
        CreateDialog(instance, MAKEINTRESOURCE(IDD_PROXY_DIALOG), wnd, proxyDlgProc);
    }
}

/* return TRUE if Proxy is enabled 
 */
BOOL proxyEnabled(void) {
    return (proxy.protocol != PP_DISABLED);
}

/* return the current Proxy host
 */
char* proxyGetHost(void) {
    return proxy.host;
}

/* return the current Proxy port 
 */
char* proxyGetPort(void) {
    return proxy.port;
}

/* return TRUE if destination hostname can be resolved by proxy server
 */
BOOL proxySupportHostname(void) {
    return (proxy.protocol != PP_SOCKS4);
}

/* Get proxy parameters from .ini file
 */
void proxyGetProfile(void)
{
    /* Get current proxy parameters */
    GetPrivateProfileString(proxyStr, hostStr, "", proxy.host, sizeof(proxy.host), telnetIniFile());
    GetPrivateProfileString(proxyStr, portStr, "1080", proxy.port, sizeof(proxy.port), telnetIniFile());
    proxy.protocol = GetPrivateProfileInt(proxyStr, protocolStr, PP_DISABLED, telnetIniFile());
    if (((int)proxy.protocol < PP_DISABLED) || (proxy.protocol > PP_HTTP)) {
        proxy.protocol = PP_DISABLED;
    }
    GetPrivateProfileString(proxyStr, usernameStr, "", proxy.username, sizeof(proxy.username), telnetIniFile());
    GetPrivateProfileString(proxyStr, passwordStr, "", proxy.password, sizeof(proxy.password), telnetIniFile());
}

/* Save proxy parameters to .ini file
 */
void proxySaveProfile(void)
{
    char str[20];
    /* Save current socks parameters */
    sprintf(str, "%d", proxy.protocol);
    WritePrivateProfileString(proxyStr, protocolStr, str, telnetIniFile());
    WritePrivateProfileString(proxyStr, hostStr, proxy.host, telnetIniFile());
    WritePrivateProfileString(proxyStr, usernameStr, proxy.username, telnetIniFile());
    WritePrivateProfileString(proxyStr, passwordStr, proxy.password, telnetIniFile());
    WritePrivateProfileString(proxyStr, portStr, proxy.port, telnetIniFile());
}
