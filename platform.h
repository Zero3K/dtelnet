/* platform.h
 * Copyright (C) 1998 David Cole
 *
 * Windows Platform Dependencies
 */
#ifndef __platform_h
#define __platform_h

#if defined(_WIN64)
	#define _export
	#define MoveTo(dc, x, y) MoveToEx(dc, x, y, NULL)
	typedef DWORD EnumFontTypeType;
	typedef INT_PTR DIALOGRET;
	typedef int int32_t;
	typedef unsigned int uint32_t;
	#ifndef _INTPTR_T_DEFINED
	#define _INTPTR_T_DEFINED
	    typedef LPARAM intptr_t;
	#endif
	typedef WPARAM uintptr_t;

#elif defined (_WIN32)
	#define _export
	#define MoveTo(dc, x, y) MoveToEx(dc, x, y, NULL)
	typedef DWORD EnumFontTypeType;
	typedef int DIALOGRET;
	typedef int int32_t;
	typedef unsigned int uint32_t;
	#ifndef _INTPTR_T_DEFINED
	#define _INTPTR_T_DEFINED
	    typedef LPARAM intptr_t;
	#endif
	typedef WPARAM uintptr_t;
    #if !defined(GetWindowLongPtr)
	#define GetWindowLongPtr(hwnd,n)     GetWindowLong(hwnd,n)
    #endif
    #if !defined(SetWindowLongPtr)
	#define SetWindowLongPtr(hwnd,n,val) SetWindowLong(hwnd,n,val)
    #endif
    #ifndef DWLP_USER
	#define DWLP_USER DWL_USER
    #endif

#else
	#define GetOEMCP() 0
	#define GetACP()   0
	#define GetWindowLongPtr(hwnd,n)     GetWindowLong(hwnd,n)
	#define SetWindowLongPtr(hwnd,n,val) SetWindowLong(hwnd,n,val)
	#define DWLP_USER DWL_USER
	typedef int EnumFontTypeType;
	typedef BOOL DIALOGRET;
	typedef long int32_t;
	typedef unsigned long uint32_t;
	#ifndef _INTPTR_T_DEFINED
	#define _INTPTR_T_DEFINED
	    typedef long intptr_t;
	#endif
	typedef unsigned long uintptr_t;

	#define MultiByteToWideChar(CodePage,dwFlags,lpMultiByteStr,cbMultiByte,lpWideCharStr,cchWideChar) 0
#endif

typedef   signed  char   int8_t;
typedef unsigned  char  uint8_t;
typedef   signed short  int16_t;
typedef unsigned short uint16_t;

#ifndef _MAX_PATH
#define _MAX_PATH 256
#endif

#if !defined(WM_UNICHAR)
#define WM_UNICHAR 0x0109
#endif
#if !defined(UNICODE_NOCHAR)
#define UNICODE_NOCHAR 0xFFFF
#endif

#if !defined(WM_SETTINGCHANGE)
#define WM_SETTINGCHANGE 26
#endif
#if !defined(WM_MOUSEWHEEL)
#define WM_MOUSEWHEEL 0x20A
#endif
#if !defined(WM_MOUSEHWHEEL)
#define WM_MOUSEHWHEEL 0x20E
#endif
#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif
#if !defined(GET_WHEEL_DELTA_WPARAM)
#define GET_WHEEL_DELTA_WPARAM(wparam) (short)((wparam)>>16)
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lparam) ((short)LOWORD((lparam)))
#define GET_Y_LPARAM(lparam) ((short)HIWORD((lparam)))
#endif

#if !defined(SW_SHOWDEFAULT) && defined(SW_SHOWNORMAL)
#define SW_SHOWDEFAULT SW_SHOWNORMAL
#endif

#if !defined(SPI_GETWHEELSCROLLLINES)
#define SPI_GETWHEELSCROLLLINES 0x0068
#endif
#if !defined(SPI_GETWHEELSCROLLCHARS)
#define SPI_GETWHEELSCROLLCHARS 0x006C
#endif

#if !defined(SIO_KEEPALIVE_VALS)
	struct tcp_keepalive {
	    unsigned long onoff;		/* 0/1 = off/on */
	    unsigned long keepalivetime;	/* in ms, eg 10min = 10*60*1000L */
	    unsigned long keepaliveinterval; 	/* in ms, default is 1sec = 1000L */
	};
	#define SIO_KEEPALIVE_VALS  0x98000004	/* IOC_IN | IOC_VENDOR | 0x04 */
#endif

#if !defined(AI_NUMERICHOST)
	struct addrinfo {
	    int             ai_flags;
	    int             ai_family;
	    int             ai_socktype;
	    int             ai_protocol;
	    size_t          ai_addrlen;
	    char            *ai_canonname;
	    struct sockaddr *ai_addr;
	    struct addrinfo *ai_next;
	};

	int WINAPI getaddrinfo (const char *nodename, const char *portname,
	    const struct addrinfo *hints, struct addrinfo **ppinfo);

	void WINAPI freeaddrinfo (struct addrinfo *pinfo);

	#define GetAddrInfoA getaddrinfo

	#define AI_NUMERICHOST	0x4
#endif
#if !defined(AI_ADDRCONFIG)
    #define AI_ADDRCONFIG	0x0400
#endif

/* from winnls.h */
#define CP_ACP 0
#define CP_OEMCP 1

#if ! defined(HH_DISPLAY_TOC)
    #define HH_DISPLAY_TOC 0x0001
    HWND WINAPI HtmlHelpA (HWND, const char *,    UINT, uintptr_t);
    HWND WINAPI HtmlHelpW (HWND, const wchar_t *, UINT, uintptr_t);
    #ifdef UNICODE
	#define HtmlHelp HtmlHelpW
    #else
	#define HtmlHelp HtmlHelpA
    #endif
#endif

#endif
