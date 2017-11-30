/* version.h */

#ifndef VERSION_H
#define VERSION_H

#define DTELNET_ID2 "Copyright © 1997-2017 David Cole"

#if defined(_WIN64)
	#define DTELNET_ID1 "Dave's Telnet - Version 1.3.9 - Win64"
#elif defined(_WIN32)
	#define DTELNET_ID1 "Dave's Telnet - Version 1.3.9 - Win32"
#else
	#define DTELNET_ID1 "Dave's Telnet - Version 1.3.9 - Win16"
#endif

#endif
