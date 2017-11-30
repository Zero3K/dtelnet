/* protocol.h
 *
 * Describe the Telnet Protocol
 */
#ifndef __protocol_h
#define __protocol_h

/* All telnet commands from rfc854
 */
#define SE	240		/* End of subnegotiation parameters */
#define NOP	241		/* No operation */
#define DM	242		/* The data stream portion of a Synch */
#define BRK	243		/* NVT character BRK (Break) */
#define IP	244		/* The function IP (Interrupt Process) */
#define AO	245		/* The function AO (Abort Output) */
#define AYT	246		/* The function AYT (Are You There) */
#define EC	247		/* The function EC (Erase Character) */
#define EL	248		/* The function EL (Erase Line) */
#define GA	249		/* The GA signal (Go Ahead) */
#define SB	250		/* Indicates that what follows is
				 * subnegotiation of the indicated
				 * option */
#define WILL	251		/* Indicates the desire to begin
				 * performing, or confirmation that
				 * you are now performing, the
				 * indicated option. */
#define WONT	252		/* Indicates the refusal to perform,
				 * or continue performing, the
				 * indicated option. */
#define DO	253		/* Indicates the request that the
				 * other party perform, or
				 * confirmation that you are expecting
				 * the other party to perform, the
				 * indicated option. */
#define DONT	254		/* Indicates the demand that the
				 * other party stop performing,
				 * or confirmation that you are no
				 * longer expecting the other party
				 * to perform, the indicated option. */
#define IAC	255		/* Data Byte 255 (Interpret As Command) */

#define TELNET_IS   0           /* an option IS */
#define TELNET_SEND 1           /* SEND an option */

#ifdef TELNET_STRINGS
static char* telnetCmds[] = {
    "SE", "NOP", "DM", "BRK", "IP", "AO", "AYT", "EC", "EL", "GA",
    "SB", "WILL", "WONT", "DO", "DONT", "IAC"
};

#define	TELCMD_OK(c)	((unsigned int)(c) <= IAC && \
			 (unsigned int)(c) >= SE)
#define	TELCMD(c)	telnetCmds[(c) - SE]
#endif

#define TELOPT_BINARY	0	/* rfc856: 8-bit data path */
#define TELOPT_ECHO	1	/* rfc857: echo */
#define	TELOPT_SGA	3	/* rfc858: suppress go ahead */
#define	TELOPT_TTYPE	24	/* rfc1091: terminal type */
#define	TELOPT_NAWS	31	/* rfc1073: window size */
#define	TELOPT_LFLOW	33	/* rfc1372: remote flow control */
#define TELOPT_XDISPLOC 35      /* rfc1096: X-display location */
#define	TELOPT_NEWENV	39	/* rfc1572: new environment */

#ifdef TELNET_STRINGS
static char* telnetOpts[TELOPT_NEWENV + 1] = {
    "BINARY",  "ECHO",  "RCP",      "SGA",    "NAME",
    "STATUS",  "MARK",  "RCTE",     "NAOL",   "NAOP",
    "NAOCRD",  "NAOHTS","NAOHTD",   "NAOFFD", "NAOVTS",
    "NAOVTD",  "NAOLFD","EXTASCII", "LOGOUT", "BYTEMACRO",
    "DATATERM","SUPDUP","SUPDUPOUT","SENDLOC","TTYPE",
    "EODOFREC","TACACS","OUTPUTMRK","TTYLOC", "3270",
    "X.3PAD",  "NAWS",  "TSPEED",   "LFLOW",  "LINEMODE",
    "XDISPLOC","OLDENV","AUTH",     "ENCRYPT","NEWENV"
};

#define	TELOPT_OK(o)	((unsigned int)(o) < numElem(telnetOpts) \
			 && telnetOpts[(unsigned int)(o)] != NULL)
#define	TELOPT(o)	telnetOpts[o]
#endif

#define	LFLOW_OFF		0	/* Disable remote flow control */
#define	LFLOW_ON		1	/* Enable remote flow control */
#define	LFLOW_RESTART_ANY	2	/* Restart output on any char */
#define	LFLOW_RESTART_XON	3	/* Restart output only on XON */

#endif
