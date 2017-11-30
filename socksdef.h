/* socksdef.h
 * 2001-10-27 Enrique Grandes
 * SOCKS protocol definitions
 */
#ifndef __socksdef_h
#define __socksdef_h

/* SOCKS Version 4 Protocol
 * see http://www.socks.nec.com/protocol/socks4.protocol
 * SOCKS 4A Simple Extension
 * see http://www.socks.nec.com/protocol/socks4a.protocol
 */
#define S4_VERSION 		4
#define S4_REQ_CONNECT 		1
#define S4_REQ_BIND 		2
#define S4_REP_SUCCESS  	90
#define S4_REP_EFAILED  	91
#define S4_REP_EIDENT   	92
#define S4_REP_EBADUSRID 	93

struct s4Packet {
    unsigned char version;
    unsigned char command;
    unsigned short port;
    unsigned long addr;
    char userid[256];
};

/* SOCKS Version 5 Protocol - rfc1928
 * see http://www.socks.nec.com/rfc/rfc1928.txt 
 */
#define S5_VERSION              5
#define S5_AUTH_NOTREQUIRED     0x00
#define S5_AUTH_GSSAPI          0x01
#define S5_AUTH_USERPASSWORD    0x02
#define S5_AUTH_NOACCEPTABLE    0xff
#define S5_REQ_CONNECT          0x01
#define S5_REQ_BIND             0x02
#define S5_REQ_UDP              0x03
#define S5_REPLY_SUCCESS        0x00
#define S5_REPLY_ESERVFAULT     0x01
#define S5_REPLY_EACCESS        0x02
#define S5_REPLY_ENETUNREACH    0x04
#define S5_REPLY_ECONNREFUSED   0x05
#define S5_REPLY_ETLLEXPIRED    0x06
#define S5_REPLY_EOPNOTSUP      0x07
#define S5_REPLY_EATNOTSUP      0x08
#define S5_AT_IPV4              0x01
#define S5_AT_DOMAINNAME        0x03
#define S5_AT_IPV6              0x04

struct s5AuthMethodReq {
    unsigned char version;
    unsigned char nmethods;
    unsigned char methods[1];
};

struct s5AuthMethodRep {
    unsigned char version;
    unsigned char method;
};

/* structure of s5Packet.dst[]...
 *
 * for addr_type == S5_AT_DOMAINNAME is:
 *
 * [A][B...][CC]
 * A = hostname length (1 byte)
 * B = hostname string without null terminator (variable == A)
 * C = port number in network byte order (2 bytes)
 *
 * for addr_type == S5_AT_IPV4 is:
 *
 * [AAAA][BB]
 * A = ipv4 address in network byte order (4 bytes)
 * B = port number in network byte order (2 bytes)
 * 
 * for addr_type == S5_AT_IPV6 is:
 *
 * [AAAAAAAAAAAAAAAA][BB]
 * A = ipv6 address in network byte order (16 bytes)
 * B = port number in network byte order (2 bytes) 
 */
struct s5Packet {
    unsigned char version;
    unsigned char command;
    unsigned char reserved;
    unsigned char addr_type;
    char dst[256];
};

/* Username/Password Authentication for SOCKS V5 - rfc1929
 * see http://www.socks.nec.com/rfc/rfc1929.txt
 */
#define S5_AUTH_UP_VERSION    1
#define S5_AUTH_UP_SUCCESS    0
#define S5_AUTH_UP_EFAILED    1

/* the structure of the autentication request is:
 *
 * [A][B][C...][D][E...]
 * A = username/password version (1 byte)
 * B = username length (1 byte)
 * C = username string without null terminator (variable == B)
 * D = password length (1 byte)
 * E = password string without null terminator (variable == D)
 */

struct s5UPAuthRep {
    unsigned char version;
    unsigned char status;
};

#endif /* __socksdef_h */

/* EOF */
