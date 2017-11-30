/* preset.h */

#ifndef __preset_h
#define __preset_h

#ifndef STRICT
#define STRICT 1
#endif

/* miniumum/expected versions:
    16-bit:    Windows 3.1
    32/64-bit: Windows XP
*/

#if defined (_WIN32)
    #if !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0501
    #endif
#else
    #ifndef WINVER
	#define WINVER 0x030a
    #endif
#endif

#endif
