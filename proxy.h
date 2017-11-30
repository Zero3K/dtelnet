/* proxy.h
 * 2001-10-27 Enrique Grandes
 * Manage Proxy protocols and Proxy dialog
 */
#ifndef __proxy_h
#define __proxy_h

/* Return values for proxyProtocolHandler() */

#define PROXY_ERROR        -1 /* error on request */
#define PROXY_OK           0  /* continue handling the request */
#define PROXY_CONNECTED    1  /* connection established */

/* Display the Proxy dialog */
void showProxyDialog(HINSTANCE hInstance, HWND hParent);
/* Return TRUE if Proxy is enabled */
BOOL proxyEnabled(void);
/* Return the current Proxy host */
char* proxyGetHost(void);
/* Return the current Proxy port */
char* proxyGetPort(void);

/* Return TRUE if the selected protocol support host names as destination */
BOOL proxySupportHostname(void);
/* Start connect request */
int proxyStartRequest(char *host, int port);
/* Manage Proxy protocol */
int proxyProtocolHandler(void);
/* Remote end has closed the connection with a socks request in progress */
void proxyRemoteHasClosed(void);

/* Load Proxy parameters from the .INI file */
void proxyGetProfile(void);
/* Save Proxy parameters to the .INI file */
void proxySaveProfile(void);

#endif /* __proxy_h */

/* EOF */
