/* dtelnet.c
 * Copyright (c) 1997 David Cole
 *
 * Mainline for the dtelnet program.  The actions performed are:
 * - All menu management.
 * - .INI and .HLP file identification.
 * - Application window management.
 * - Child window placement.
 * - Command line argument parsing.
 *
 * The application window has two child windows; the terminal window,
 * and the status bar.  No drawing is performed in the application
 * window.
 */
#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <io.h>

#include "platform.h"
#include "utils.h"
#include "log.h"
#include "term.h"
#include "emul.h"
#include "socket.h"
#include "about.h"
#include "connect.h"
#include "filetype.h"
#include "status.h"
#include "font.h"
#include "argv.h"
#include "dialog.h"
#include "raw.h"
#include "termdef.h"
#include "printing.h"
#include "editor.h"
#include "proxy.h"
#include "dtchars.h"
#include "dtelnet.h"
#include "termkey.h"
#include "envvars.h"
#include "uconv.h"

/* #define WRITEDEBUGFILE 1 */
#ifdef WRITEDEBUGFILE
    #include "debuglog.h"
    #define TermDebugF(par) dtelnetDebugF par
#else
    #define TermDebugF(par)
#endif

/* Windows classname of application window
 */
#if defined(_WIN32)
static const wchar_t *telnetWinClassW = L"DTelnetWClass";
#else
static const char *telnetWinClass = "DTelnetWClass";
#endif

static HWND telnetWnd;		/* handle of application window */

static OptSource src_iniFileName= OptUnset;
static char *iniFileName= "";

EmulNames emuls;

HINSTANCE telnetGetInstance(void)
{
    return instanceHnd;
}

HWND telnetGetWnd(void)
{
    return telnetWnd;
}

static BOOL telnetConfirmClose(void)
{
    int mbret;
static const char closeFmt [] = "Do you really want to close connection to %s:%s?";
    char closeMsg [512];

    if (socketOffline()) return TRUE;
    sprintf (closeMsg, closeFmt, socketGetHost(), socketGetPort());

    mbret= MessageBox (telnetWnd, closeMsg, "Confirm", MB_ICONEXCLAMATION | MB_YESNO);
    return mbret==IDYES || mbret==IDOK;
}

/* Destroy the application window to trigger exit
 */
static void telnetQueryExit (void)
{
    if (telnetConfirmClose()) telnetExit ();
}

/* Destroy the application window to trigger exit
 */
void telnetExit()
{
    DestroyWindow(telnetWnd);
}

/* Return the name of the application
 */
const char* telnetAppName()
{
    return "Dave's Telnet";
}

#if defined(_WIN32)
const wchar_t* telnetAppNameW(void)
{
    return L"Dave's Telnet";
}
#endif

/* Report a fatal error then exit
 *
 * Args:
 * fmt - printf format string
 * ... - arguments to format string
 */
void telnetFatal(const char* fmt, ...)
{
    va_list ap;
    char msg[512];

    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    MessageBox(telnetWnd, msg, telnetAppName(),
	       MB_APPLMODAL|MB_ICONSTOP|MB_OK);
    telnetExit();
}

/* Return the name of the .INI file
 */
const char* telnetIniFile(void)
{
    return iniFileName;
}

/* Add previous connections to the history on the menu.
 *
 * Args:
 * menu - handle of application window menu
 *
 * Maintain the connection history in the Connect menu.  Gray entries
 * when we are online.
 */
static void addConnectHistory(HMENU menu)
{
    int num;			/* iterate over connection history */
    char histline [MAX_HISTORY_SIZE];
    char* text;			/* formatted session */
    UINT action = socketOffline() ? MF_ENABLED : MF_GRAYED;

    for (num = 0; (text = connectGetHistory (histline, num, TRUE)) != NULL; ++num) {
	int id = ID_CONNECT_HISTORY + num;
	if (GetMenuState(menu, id, MF_BYCOMMAND) == (UINT)-1) {
	    if (num == 0)
		AppendMenu(menu, MF_ENABLED | MF_SEPARATOR, 0, NULL);
	    AppendMenu(menu, action, id, text);
	} else
	ModifyMenu(menu, id, MF_BYCOMMAND | action, id, text);
    }
}

/* Menu is about to pop up - customise it
 *
 * Args:
 * wnd -  handle of application window
 * menu - handle of application window menu
 */
static void initMenu(HWND wnd, HMENU menu)
{
    const char *termname;
    HMENU terminalMenu, emulateMenu;
    unsigned int i;

    if (socketOffline()) {
	/* We are offline, enable connect, disable disconnect
	 */
	EnableMenuItem(menu, (UINT)ID_CONNECT_REMOTE_SYSTEM,
					 MF_BYCOMMAND | MF_ENABLED);
	EnableMenuItem(menu, (UINT)ID_CONNECT_DISCONNECT,
					 MF_BYCOMMAND | MF_GRAYED);
        /* Enable Proxy Settings
         */
        EnableMenuItem(menu, (UINT)ID_CONNECT_PROXY, MF_BYCOMMAND | MF_ENABLED);
    } else {
	/* We are online, disable connect, enable disconnect
	 */
	EnableMenuItem(menu, (UINT)ID_CONNECT_REMOTE_SYSTEM,
					 MF_BYCOMMAND | MF_GRAYED);
	EnableMenuItem(menu, (UINT)ID_CONNECT_DISCONNECT,
					 MF_BYCOMMAND | MF_ENABLED);

        /* Disable Proxy Settings
         */
        EnableMenuItem(menu, (UINT)ID_CONNECT_PROXY, MF_BYCOMMAND | MF_GRAYED);
    }

    /* Enable copy if we are in windows select mode, and there is
     * a selection.
     */
    EnableMenuItem(menu, ID_EDIT_COPY,
		   MF_BYCOMMAND |
		   (!termAutoCopy() && termHaveSelection() ? MF_ENABLED : MF_GRAYED));
    /* Enable Edit/Paste when there is text on clipboard
     */
    if (OpenClipboard(wnd)) {
	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
	    EnableMenuItem(menu, ID_EDIT_PASTE, MF_BYCOMMAND | MF_ENABLED);
	} else {
	    EnableMenuItem(menu, ID_EDIT_PASTE, MF_BYCOMMAND | MF_GRAYED);
	}
        CloseClipboard();
    }

    /* Initialise the Connect menu
     */
    CheckMenuItem(menu, (UINT)ID_CONNECT_EXIT_DISCONNECT,
	MF_BYCOMMAND | (connectGetExitOnDisconnect()
			? MF_CHECKED : MF_UNCHECKED));

    {
	int flg= MF_GRAYED;
	const char *str= "Reconnect";

	if (socketConnected()) {
	    str= "Duplicate";
	    flg= MF_ENABLED;

	} else if (socketOffline() && socketConnectedEver()) {
	    str= "Reconnect";
	    flg= MF_ENABLED;
	}

	ModifyMenu (menu, (UINT)ID_CONNECT_RECONN,
		MF_BYCOMMAND | MF_STRING | flg, ID_CONNECT_RECONN, str);
    }

    addConnectHistory(GetSubMenu(menu, 0));

    /* Initialise the Edit menu
     */
    CheckMenuItem(menu, (UINT)ID_EDIT_AUTO_COPY,
	MF_BYCOMMAND | (termAutoCopy()
			? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, (UINT)ID_TERMINAL_BOTTOM_ON_OUTPUT,
	MF_BYCOMMAND | (termBottomOnOutput()
			? MF_CHECKED : MF_UNCHECKED));

    if (editGetCMMode()==ID_CM_FREE) {
	CheckMenuItem(menu, ID_CM_FREE, MF_BYCOMMAND | MF_CHECKED);
	CheckMenuItem(menu, ID_CM_TEXT, MF_BYCOMMAND | MF_UNCHECKED);
    } else {
	CheckMenuItem(menu, ID_CM_FREE, MF_BYCOMMAND | MF_UNCHECKED);
	CheckMenuItem(menu, ID_CM_TEXT, MF_BYCOMMAND | MF_CHECKED);
    }

    CheckMenuItem (menu, ID_TERMINAL_BACKSPACE_ALT, MF_BYCOMMAND | (connectGetBs2DelOption()   ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem (menu, ID_TERMINAL_VT_KEY_MAP,    MF_BYCOMMAND | (connectGetVtKeyMapOption() ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuItem (menu, ID_TERMINAL_MOUSE_PASTE_M, MF_BYCOMMAND | (termGetPasteButton()==1 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem (menu, ID_TERMINAL_MOUSE_PASTE_R, MF_BYCOMMAND | (termGetPasteButton()==2 ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuItem(menu, ID_TERMINAL_CS_BLOCK,
	MF_BYCOMMAND | (termCursorStyle() == cursorBlock) ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(menu, ID_TERMINAL_CS_UNDERLINE,
	MF_BYCOMMAND | (termCursorStyle() == cursorUnderLine) ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(menu, ID_TERMINAL_CS_VERTLINE,
	MF_BYCOMMAND | (termCursorStyle() == cursorVertLine) ? MF_CHECKED : MF_UNCHECKED);

    EnableMenuItem(menu, (UINT)ID_TERMINAL_START_LOGGING,
    	MF_BYCOMMAND | (logLogging() ? MF_GRAYED : MF_ENABLED));
    EnableMenuItem(menu, (UINT)ID_TERMINAL_STOP_LOGGING,
	MF_BYCOMMAND | (logLogging() ? MF_ENABLED : MF_GRAYED));
    CheckMenuItem(menu, (UINT)ID_TERMINAL_LOG_SESSION,
    	MF_BYCOMMAND | (logLoggingSession()
			? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, (UINT)ID_TERMINAL_LOG_PROTOCOL,
	MF_BYCOMMAND | (logLoggingProtocol()
			? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, (UINT)ID_TERMINAL_ATTPRINT,
	MF_BYCOMMAND | (termAttachedPrinter()
    			? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(menu, (UINT)ID_TERMINAL_ENABLE_PRINTSCR,
	MF_BYCOMMAND | (termPrintScreenEnabled()
    			? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(menu, (UINT)ID_TERMINAL_PRINTSCREEN,
    	MF_BYCOMMAND | (termPrintScreenEnabled() ? MF_ENABLED : MF_GRAYED));

    /* Initialise the Terminal menu
     */
    telnetEnumTermProfiles(&emuls);
    termname = emulGetTerm()->name;

    terminalMenu = GetSubMenu(menu, MENU_TERMINAL_POSITION);
    if (!terminalMenu) return;

    emulateMenu = GetSubMenu(terminalMenu, MENU_EMULATE_POSITION);
    if (!emulateMenu) return;
    
    for (i=0; i < emuls.num; i++)
    {
	int id = ID_MENU_EMULATE_FIRST + i;
	if (GetMenuState(menu, id, MF_BYCOMMAND) == (UINT)-1) {
	    AppendMenu(emulateMenu, MF_STRING, id, emuls.names[i]);
	} else
	ModifyMenu(menu, id, MF_BYCOMMAND | MF_STRING, id, emuls.names[i]);
	CheckMenuItem(emulateMenu, id, MF_BYCOMMAND
           | (strcmp(termname, emuls.names[i])==0 ? MF_CHECKED : MF_UNCHECKED));
    }
    CheckMenuItem (terminalMenu, (UINT)ID_ENABLE_LOCAL_EXECUTE, MF_BYCOMMAND |
		   (emulGetExecuteFlag() ? MF_CHECKED : MF_UNCHECKED));
}

/* Try to keep the window within the workspace */
static BOOL rationalisePlacement (RECT *window);

/* The telnet window has been created, the terminal window has been
 * created.  Set our initial window size. (Don't call it if SW_MAXIMIZE is specified.)
 */
static void initSetWindowSize(void)
{
    PixelSize pixelSize;

/* size of termWnd (with scrollbar) */
    termCalculatePixelSize (&term.winSize, &pixelSize, TRUE);
    telnetTerminalSize (pixelSize.cx, pixelSize.cy);
}

/* The telnet window has been created, the terminal window has been
 * created and sized.  Set our initial window position if given.
 */
static void initSetWindowPos(HWND telnetWnd)
{
    RECT workspace;		/* the area of the useable workspace */
    RECT window;		/* the area of the application window */
    int x, y;			/* the new coordinates of the window */

#ifdef WIN32
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workspace, 0);
#else
    GetWindowRect (GetDesktopWindow (), &workspace);
#endif
    GetWindowRect(telnetWnd, &window);

    if (!term.useInitPos) {
	BOOL fChanged;

	fChanged= rationalisePlacement (&window);
	if (!fChanged) return;

	x= window.left;
	y= window.top;

    } else {
	if (term.winInitPos.cx < 0)
	    x = workspace.right - (window.right - window.left) + term.winInitPos.cx;
	else
	    x = term.winInitPos.cx;

	if (term.winInitPos.cy < 0)
	    y = workspace.bottom - (window.bottom - window.top) + term.winInitPos.cy;
	else
	    y = term.winInitPos.cy;
    }

    SetWindowPos(telnetWnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void recalcInitPos(void)
{
    RECT rect;
    RECT workspace;		/* the area of the useable workspace */

    if (!term.useInitPos)
	return;
    GetWindowRect(telnetWnd, &rect);
#ifdef WIN32
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workspace, 0);
#else
    workspace.top = workspace.left = 0;
		workspace.right = GetSystemMetrics(SM_CXSCREEN);
    workspace.bottom = GetSystemMetrics(SM_CYSCREEN);
#endif

    if (rect.left >= 0)
				term.winInitPos.cx = rect.left;
    else
	term.winInitPos.cx = rect.right - workspace.right;
    if (rect.top >= 0)
	term.winInitPos.cy = rect.top;
    else
	term.winInitPos.cy = rect.bottom - workspace.bottom;
    statusInvalidate ();
}

static BOOL rationalisePlacement (RECT *window)
{
    RECT workspace;
    BOOL fChanged= FALSE;
    int nmove;

#ifdef WIN32
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workspace, 0);
#else
    GetWindowRect (GetDesktopWindow (), &workspace);
#endif

    if (window->right > workspace.right &&
	window->left > workspace.left) {
	nmove= min (window->right - workspace.right, window->left - workspace.left);
	window->left -= nmove;
	fChanged= TRUE;
    }
    if (window->bottom > workspace.bottom &&
	window->top > workspace.top) {
	nmove= min (window->bottom - workspace.bottom, window->top - workspace.top);
	window->top -= nmove;
	fChanged= TRUE;
    }
    return fChanged;
}

/* The terminal window has just resized to specified width/height.  We
 * have to resize ourselves to allow the terminal resize.
 *
 * Args:
 * xs - width (in pixels) of the terminal child window
 * ys - height (in pixels) of the terminal child window
 */
void telnetTerminalSize(int xs, int ys)
{
    RECT window;		/* screen rectangle of application window */
    RECT client;		/* size of client rectangle */
    RECT status;		/* size/position of status bar */
    RECT newWindow;
    SIZE newSize;		/* calculate new size of application window */
    int statusHeight;		/* calculate height of status bar child */

    /* Query and calculate current sizes and positions of all players
     */
    GetWindowRect(telnetWnd, &window);
    GetClientRect(telnetWnd, &client);
    GetWindowRect(statusGetWnd(), &status);
    statusHeight = status.bottom - status.top;

    /* Work out the new size of the application window
     */
    newSize.cx = xs + window.right - window.left - client.right;
    newSize.cy = ys + window.bottom - window.top - client.bottom
	+ statusHeight;

    newWindow.top=    window.top;
    newWindow.bottom= newWindow.top + newSize.cy;
    newWindow.left=   window.left;
    newWindow.right=  newWindow.left + newSize.cx;
    rationalisePlacement (&newWindow);

    /* Now make ourselves the adjusted size
     */
    SetWindowPos(telnetWnd, HWND_TOPMOST,
		 newWindow.left, newWindow.top,
		 newSize.cx, newSize.cy,
		 SWP_NOZORDER|SWP_NOACTIVATE);

    /* if the window was Maximized, now it becomes Restored,
       but without resizing it */
    {
	WINDOWPLACEMENT wplc;

	memset (&wplc, 0, sizeof (wplc));
	wplc.length= sizeof (wplc);
	GetWindowPlacement (telnetWnd, &wplc);
	wplc.flags= 0;
	if (wplc.showCmd==SW_MAXIMIZE) {
	    wplc.showCmd= SW_RESTORE;

	    wplc.rcNormalPosition.left   = wplc.ptMaxPosition.x;
	    wplc.rcNormalPosition.right  = wplc.rcNormalPosition.left + newSize.cx;
	    wplc.rcNormalPosition.top    = wplc.ptMaxPosition.y;
	    wplc.rcNormalPosition.bottom = wplc.rcNormalPosition.top + newSize.cy;
	}
	SetWindowPlacement (telnetWnd, &wplc);
    }

    /* Move the status bar to the new position
     */
    MoveWindow(statusGetWnd(), 0, ys, xs, statusHeight, TRUE);
}

#define SizeFromRect(to,from) \
{ (to)->cx = (from)->right  - (from)->left; \
  (to)->cy = (from)->bottom - (from)->top; }

/* telnetCalculateOptimalSize
 *	extracted from 'telnetWindowPosChanging' by LZS 2012.02.17.
 *
 *      size:      IN: suggested size (brutto, as in GetWindowRect, or WM_WINDOWPOSCHANGING)
 *                     size==NULL or cx==cy==0 means use GetWindowRect
 *                OUT: reduced size 
 *      sDec:     OUT: delta values (non-negative)
 *      rWindow:  OUT: GetWindowRect (telnetWnd)
 *      faccepted OUT: TRUE if accepted==suggested
 *      fcurrent  OUT: TRUE if accepted==current
 *
 * example: (800,600) -> (797,590),(3,10),faccepted==FALSE
 *          (797,590) -> (797,590),(0, 0),faccepted==TRUE
 */
static void telnetCalculateOptimalSize (SIZE *size, SIZE *sDec,
					RECT *rWindow,
					BOOL *faccepted, BOOL *fcurrent)
{
    RECT rMyWindow;		/* current window position */
    RECT rClient;		/* current client window */
    RECT rStatus;		/* current status window */

    SIZE sPad;			/* padding around terminal window */
    SIZE sWindow;		/* size of the window */
    SIZE sClient;		/* size of the client area */
    SIZE sStatus;		/* size of status window */
    SIZE sTerm;			/* size of terminal window */

    SIZE sProposed;		/* proposed size of the window */
    SIZE sRet;			/* accepted size of the window */

    /* Determine the current dimensions of the various windows
     */
    if (!rWindow) rWindow= &rMyWindow;
    GetWindowRect(telnetWnd,  rWindow);      SizeFromRect (&sWindow, rWindow);
    GetClientRect(telnetWnd, &rClient);      SizeFromRect (&sClient, &rClient);
    GetWindowRect(statusGetWnd(), &rStatus); SizeFromRect (&sStatus, &rStatus);

    if (size && (size->cx || size->cy)) sProposed= *size;
    else				sProposed= sWindow;

    /* Calculate the padding around the terminal window
     */
    sPad.cx = sWindow.cx - sClient.cx;
    sPad.cy = sWindow.cy - sClient.cy + sStatus.cy;

    /* Work out the proposed terminal window size
     */

    sTerm.cx = sProposed.cx - sPad.cx;
    sTerm.cy = sProposed.cy - sPad.cy;

    termWindowPosChanging (&sTerm);

    sRet.cx = sTerm.cx + sPad.cx;
    sRet.cy = sTerm.cy + sPad.cy;

    if (size) {
	*size= sRet;
    }

    if (sDec) {
	sDec->cx = sProposed.cx - sRet.cx; 
	sDec->cy = sProposed.cy - sRet.cy; 
    }
    if (faccepted) {
	*faccepted= sRet.cx==sProposed.cx && sRet.cy==sProposed.cy;
    }
    if (fcurrent) {
	*fcurrent= sRet.cx==sWindow.cx && sRet.cy==sWindow.cy;
    }
}

/* The user is dragging the window frame.  Modify the resize as it is
 * happening to force a grid of one character.
 *
 * Args:
 * winPos - size that window is about to be given by Windows
 *
 * This code is the result of sweating litres of blood trying to tie
 * down all of the different ways that Win3.1 and Win95 resize a
 * window.
 */
static void telnetWindowPosChanging(WINDOWPOS* winPos)
{
    RECT window;		/* current window position */

    SIZE sSize;
    SIZE proposed;		/* proposed terminal size */

    /* We are only interested in window resizing
     */
    if (winPos->flags & (SWP_NOSIZE|SWP_HIDEWINDOW
			 |SWP_SHOWWINDOW|SWP_DRAWFRAME))
	return;

    /* Determine the current dimensions of the various windows
     */

    sSize.cx = winPos->cx;
    sSize.cy = winPos->cy;
    telnetCalculateOptimalSize (&sSize, &proposed, &window, NULL, NULL);

    /* Compare the proposed terminal size with the preferred size and
     * make adjustments to the window resize accordingly.
     */
    if (proposed.cx != 0) {
	/* Horizontal size adjusted
	 */
	if (winPos->x != window.left) {
	    /* Resizing from left - adjust winPos->x for preferred size
	     */
	    winPos->x += proposed.cx;
	}
	winPos->cx -= proposed.cx;
    }

    if (proposed.cy != 0) {
	/* Vertical size was adjusted
	 */
	if (winPos->y != window.top) {
	    /* Resizing from top - adjust winPos->y for preferred size
	     */
	    winPos->y += proposed.cy;
	}
	winPos->cy -= proposed.cy;
    }
}

/* The window has just been resized - enforce the new size on the
 * child windows.
 *
 * Args:
 * type - the type of resize that just occurred
 * cx -   the new width of the application window
 * cy -   the new height of the application window
 */
static void telnetSetWindowSize(UINT type, int cx, int cy)
{
    RECT status;		/* size / location of status bar */
    int statusHeight;		/* height of status bar */

    if (type == SIZE_MINIMIZED)
	return;

    if (type==SIZE_MAXIMIZED) {
	SIZE sReduced= {0, 0}; /* meaning: call GetWindowRect */
	SIZE sDelta;
	BOOL fcurrent;
	RECT rWindow;

	telnetCalculateOptimalSize (&sReduced, &sDelta, &rWindow, NULL, &fcurrent);
	if (! fcurrent) {
	    WINDOWPLACEMENT wplc;
            SIZE sShift = {0, 0};

	    cx -= sDelta.cx;
	    cy -= sDelta.cy;

	    wplc.length= sizeof (wplc);
	    GetWindowPlacement (telnetWnd, &wplc);

	    sShift.cx= -GetSystemMetrics (SM_CXFRAME);
	    if (sDelta.cx>0) {
		sShift.cx += sDelta.cx;
		if (sShift.cx>0) sShift.cx= 0;
	    }
	    sShift.cy= -GetSystemMetrics (SM_CYFRAME);
	    if (sDelta.cy>0) {
		sShift.cy += sDelta.cy;
		if (sShift.cy>0) sShift.cy= 0;
	    }

#if 1
	    wplc.rcNormalPosition.left   = sShift.cx;
	    wplc.rcNormalPosition.right  = wplc.rcNormalPosition.left + sReduced.cx;
	    wplc.rcNormalPosition.top    = sShift.cy;
	    wplc.rcNormalPosition.bottom = wplc.rcNormalPosition.top + sReduced.cy;

	    wplc.showCmd= SW_RESTORE;
	    SetWindowPlacement (telnetWnd, &wplc);
#else
	    sShift.cx += GetSystemMetrics (SM_CXFRAME);
	    sShift.cy += GetSystemMetrics (SM_CYFRAME);

	    ShowWindow (telnetWnd, SW_SHOWNORMAL);
	    SetWindowPos (telnetWnd, NULL,
		rWindow.left + sShift.cx, 
		rWindow.top + sShift.cy,
		sReduced.cx, sReduced.cy, SWP_SHOWWINDOW|SWP_NOSENDCHANGING);
#endif
	}
    }

    /* Move the status bar to keep it at the bottom of the window, and
     * at full width.
     */
    GetWindowRect(statusGetWnd(), &status);
    statusHeight = status.bottom - status.top;

    if (type == SIZE_RESTORED || type == SIZE_MAXIMIZED)
	termSetWindowSize(cx, cy - statusHeight);
    MoveWindow(statusGetWnd(), 0, cy - statusHeight, cx, statusHeight, TRUE);
}

/* TestEnterMenu:
   if an Left_Alt + Right_Alt sequence has been completed,
   enter the menu
   Don't call this function if we are not connected
   (becase in that case either 'F10' or 'Alt' enters the menu

Return value:
   TRUE: ignore this message, don't call any other proc
 */
BOOL TestEnterMenu (HWND wnd, const TermKey *tk)
{
#define STATE_S	0
#define STATE_A	1	/* one alt key is pressed */
#define STATE_B	2	/* both-alt pressed */
    static int state= STATE_S;
    int altcnt;

/* ignore VK_CONTROL, because Windows 'autmagically' generates 
   VK_CONTROL keypresses when right-ALT is pressed */
    if (tk->keycode==VK_CONTROL) {
	return FALSE;

/* otherwise if it is non-ALT, reset state */
    } else if (tk->keycode!=VK_MENU) {
	state= STATE_S;
	return FALSE;
    }

    altcnt= ((tk->modifiers & TMOD_LALT)!=0) +
	    ((tk->modifiers & TMOD_RALT)!=0);

    if (state==STATE_S) {
	if ((tk->flags & KF_UP)==0 && altcnt==1) state= STATE_A;
	else					 state= STATE_S;

    } else if (state==STATE_A) {
	if ((tk->flags & KF_UP) || altcnt==0) state= STATE_S;
	else if (altcnt==2) state= STATE_B;
	else		    state= STATE_A;

    } else if (state==STATE_B) {
	if ((tk->flags & KF_UP)==0 && altcnt==2) state= STATE_B;
	else if ((tk->flags & KF_UP) && altcnt==1) {
	    PostMessage (wnd, WM_SYSCOMMAND, SC_KEYMENU, 0);
	    state= STATE_S;
	} else state= STATE_S;
    }
    return FALSE;
#undef STATE_S
#undef STATE_L
#undef STATE_R
#undef STATE_B
}

/* 20150108.LZS: variable 'msg' became public
    reason: it contains important information:
	DWORD  time;	-- mesurement unit ?!
	POINT  pt;
 */
static MSG msg;			/* current message */

MSG *telnetLastMsg(void)
{
    return &msg;
}

/* Window procedure for the application window
 */
static LRESULT CALLBACK mainWndProc
	(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    HMENU menu;
    static int ignore_next_wm_char= 0;
    WORD wCmd;
    LRESULT retval;

/* Dirtiest hack ever, please someone suggest something better */
    if (message!=WM_CHAR) ignore_next_wm_char= 0;

    switch (message) {
    case WM_COMMAND: 
	wCmd= LOWORD (wparam);
DO_COMMAND:
	switch (wCmd) {
	case ID_CONNECT_NEW:
	    /* Create a new instance of dtelnet */
	    telnetSaveProfile ();
	    telnetClone (NULL, NULL);
	    break;

	case ID_CONNECT_REMOTE_SYSTEM:
	    showConnectDialog(instanceHnd, wnd);
	    break;

	case ID_CONNECT_DISCONNECT:
	    if (!socketOffline() && telnetConfirmClose()) {
		socketDisconnect();
	    }
	    break;

	case ID_CONNECT_PROXY:
	    showProxyDialog(instanceHnd, wnd);
	    break;
	case ID_CONNECT_EXIT_DISCONNECT:
	    connectToggleExitOnDisconnect();
	    break;
	case ID_EXIT:
	    telnetQueryExit();
	    break;

	case ID_CONNECT_RECONN:
	    if (socketOffline() && socketConnectedEver()) {
		connectOpenHost ();

	    } else if (socketConnected()) {
		BuffData cmdline= EmptyBuffData;

		telnetSaveProfile ();
		connectCmdLine (NULL, &cmdline);
		telnetClone (NULL, (ConstBuffData *)&cmdline);
		if (cmdline.ptr) free (cmdline.ptr);
	    }
	    break;

	case ID_EDIT_COPY:
	    termSelectCopy();
	    break;
	case ID_EDIT_PASTE:
	    termSelectPaste();
	    break;
	case ID_EDIT_AUTO_COPY:
	    termToggleAutoCopy();
	    break;

	case ID_CM_FREE:
	case ID_CM_TEXT:
	    editSetCMMode (wCmd);
	    break;

	case ID_MANAGE_FAVORITES:
	    fileTypeManage(instanceHnd, wnd);
	    break;

	case ID_ADD_TO_FAVORITES:
	    fileFavCreate(instanceHnd, wnd);
	    break;

	case ID_TERMINAL_FONT:
	    showFontDialog(instanceHnd, wnd);
	    break;

	case ID_TERMINAL_BOTTOM_ON_OUTPUT:
	    termToggleBottomOnOutput();
	    break;

	case ID_TERMINAL_CS_BLOCK:
	    termSetCursorStyle(cursorBlock);
	    break;
	case ID_TERMINAL_CS_VERTLINE:
	    termSetCursorStyle(cursorVertLine);
	    break;
	case ID_TERMINAL_CS_UNDERLINE:
	    termSetCursorStyle(cursorUnderLine);
	    break;

	case ID_TERMINAL_BACKSPACE_ALT:
	    connectSetBs2Del(!connectGetBs2DelOption());
	    break;
	case ID_TERMINAL_VT_KEY_MAP:
	    connectSetVtKeyMap(!connectGetVtKeyMapOption());
	    break;

	case ID_TERMINAL_MOUSE_PASTE_M:
	    termSetPasteButton (1); /* middle button */
	    break;
	case ID_TERMINAL_MOUSE_PASTE_R:
	    termSetPasteButton (2); /* right button */
	    break;

	case ID_TERMINAL_START_LOGGING:
	    logStart();
	    break;
	case ID_TERMINAL_STOP_LOGGING:
	    logStop();
	    break;
	case ID_TERMINAL_LOG_SESSION:
	    logToggleSession();
	    break;
	case ID_TERMINAL_LOG_PROTOCOL:
	    logToggleProtocol();
	    break;
	case ID_TERMINAL_DEFINITION:
	    terminalDefShowDialog(instanceHnd, wnd);
	    break;

	case ID_COLOR_FORE:
	case ID_COLOR_BACK:
	case ID_COLOR_EMP_FORE:
	case ID_COLOR_EMP_BACK: {
	    int rc= GetTermStdColor (wCmd-ID_COLOR_FORE, wnd);
	    if (rc==0) winInvalidateAll ();
	    break; }

	case ID_COLOR_SAVE:
	    termSaveDefAttr (NULL);
	    break;

	case ID_COLOR_LOAD:
	    termGetDefAttr ();
	    winInvalidateAll ();
	    break;

	case ID_ENABLE_LOCAL_EXECUTE:
	    emulSetExecuteFlag (! emulGetExecuteFlag ());
	    break;

	case ID_TERMINAL_PRINTPREF:
	    printingPrefDialog();
	    break;
	case ID_TERMINAL_ATTPRINT:
	    termToggleAttachedPrinter();
	    break;
	case ID_TERMINAL_ENABLE_PRINTSCR:
	    termTogglePrintScreen();
	    break;
	case ID_TERMINAL_PRINTSCREEN:
	    if (termPrintScreenEnabled()) printingPrintScreen();
	    break;

	case ID_HELP_CONTENTS: {
	    const char *hlp;

	    hlp= getHelpName ();
	    if (hlp && *hlp) {
#if defined (_WIN32)
		HtmlHelp (termGetWnd (), hlp, HH_DISPLAY_TOC, 0L);
#else
		WinHelp (termGetWnd (), hlp, HELP_INDEX, 0L);
#endif
	    }
	    break; }

	case ID_HELP_ABOUT:
	    aboutShowDialog(instanceHnd, wnd);
	    break;

	default:
	    /* handle connection history items */
	    if (wparam >= ID_CONNECT_HISTORY
		&& wparam < ID_CONNECT_HISTORY + MAX_HISTORY) {
		connectHistory(wparam - ID_CONNECT_HISTORY);
		break;
	    }
	    /* handle emulation menu items */
	    else if (wparam >= ID_MENU_EMULATE_FIRST
		&& wparam < ID_MENU_EMULATE_FIRST + emuls.num) {
		connectRememberTerm(emuls.names[wparam - ID_MENU_EMULATE_FIRST]);
		emulSetTerm(emuls.names[wparam - ID_MENU_EMULATE_FIRST]);
		statusSetTerm();
	    }
	    /* handle 'Favorites' menu items */
	    else if (wparam >= ID_FAVORITES_FIRST && wparam < ID_FAVORITES_MAX)
	    {
		fileExecFavorites(GetMenu(wnd), wparam);
	    }
	    /* handle 'CharSet' menu items */
	    else if (wparam >= ID_TERMINAL_SERVER_MIN &&
		     wparam <  ID_TERMINAL_SERVER_LIM) {
		char *charset;

		charset = dtcEC_GetName (wparam - ID_TERMINAL_SERVER_MIN + 1);
		if (charset) {
		    emulSetCharset (NULL, charset);
		    dtcEC_CheckMenuItem (charset);
		}
	    }
	    else break;
	}
	return 0;

    case WM_ERASEBKGND:
	/* No point erasing the window, it is obscured by child windows.
	 */
	return 1;
    case WM_DESTROY:
	PostQuitMessage(0);
	return 0;

    case WM_SETFOCUS:
	termSetFocus();
	return 0;
    case WM_KILLFOCUS:
	termKillFocus();
	return 0;

#ifdef WIN32
    case WM_ENTERSIZEMOVE:
	rawResizeBegin();
	break;
    case WM_EXITSIZEMOVE:
	rawResizeEnd();
	break;
#endif
    case WM_WINDOWPOSCHANGING:
	telnetWindowPosChanging((WINDOWPOS*)lparam);
	return 0;

    case WM_SIZE: {
	unsigned short cx= LOWORD(lparam);
	unsigned short cy= HIWORD(lparam);

	telnetSetWindowSize (wparam, cx, cy);
	return 0; }

    case WM_MOVE:
	recalcInitPos();
	break;

    case WM_INITMENU:
	if ((HMENU)wparam == GetMenu(wnd))
	    initMenu(wnd, (HMENU)wparam);
	return 0;

    case WM_UNICHAR:
	if (wparam != UNICODE_NOCHAR) {
	    termWmChar (wparam, lparam, DTCHAR_UNICODE);
	}
	return 1;

    case WM_CHAR:
	if (ignore_next_wm_char) ignore_next_wm_char= 0;
	else
#if defined(_WIN32)
	    termWmChar (wparam, lparam, DTCHAR_UNICODE);
#else
	    termWmChar (wparam, lparam, DTCHAR_ANSI);
#endif
	break;

    case WM_KEYDOWN:
	{
	    TermKey tk;
	    ConstBuffData bmap, *pmap;
	    int rc;
	    long lmap;

	    termFillTermKey (wparam, lparam, &tk);

	    rc= tkmGetKeyMap (TKM_DEFAULT_MAP, &tk, &bmap, &lmap);

	    TermDebugF ((0, "dtelnet.WM_KEYDOWN: key=0x%x,scan=0x%x,flags=0x%x,mods=0x%x,locks=0x%x,classe=%d"
		"\nrc=%d,bmap=(%p,%d),lmap=%d"
		, tk.keycode, tk.scancode, tk.flags
		, tk.modifiers, tk.locks, tk.classe
		, (int)rc, bmap.ptr, (int)bmap.len, (int)lmap
		));
	    if (!rc) { /* mapped */
		if (lmap) {		/* command */
		    wCmd= (WORD)lmap;
		    wparam= wCmd;
		    lparam= 0;
		    goto DO_COMMAND;
		}
		pmap= &bmap;
	    } else {
		BOOL menu= 			/* special: if we are connected */
		    socketConnected() &&	/* LeftAlt + RightAlt means enter menu */
		    TestEnterMenu (wnd, &tk);
		if (menu) {
		    ignore_next_wm_char= FALSE;
		    break;
		}
		pmap= NULL;
	    }
	    ignore_next_wm_char = termKeyDown (&tk, pmap);
	    break;
	}

    case WM_KEYUP: {
	TermKey tk;
	BOOL menu;

	if (!socketConnected()) break;

	termFillTermKey (wparam, lparam, &tk);
	menu= TestEnterMenu (wnd, &tk);
	if (menu) return 0;
	else	  break; }

    case WM_SYSKEYDOWN: {
	    TermKey tk;
	    ConstBuffData bmap, *pmap;
            int rc;
	    long lmap;

	    termFillTermKey (wparam, lparam, &tk);

	    rc= tkmGetKeyMap (TKM_DEFAULT_MAP, &tk, &bmap, &lmap);
	    if (!rc) { /* mapped */
		if (lmap) {		/* command */
		    wCmd= (WORD)lmap;
		    wparam= wCmd;
		    lparam= 0;
		    goto DO_COMMAND;
		}
		pmap= &bmap;
	    } else {
		BOOL menu= 			/* special: if we are connected */
		    socketConnected() &&	/* LeftAlt + RightAlt means enter menu */
		    TestEnterMenu (wnd, &tk);
		if (menu) {
		    ignore_next_wm_char= FALSE;
		    return 0;
		}
		pmap= NULL;
	    }
	    if (!pmap && !socketConnected()) {	/* Neither mapped key nor connected to server: */
		break;				/* Default action for F10, Alt+F4 etc */

	    } else {
		termSysKeyDown (&tk, pmap);
		return 0;
	    }
	}

    case WM_SYSKEYUP: {
	TermKey tk;

	if (!socketConnected()) break;

	termFillTermKey (wparam, lparam, &tk);
	TestEnterMenu (wnd, &tk);
	return 0; }

    case WM_SYSCHAR:
	if (!socketConnected())
	    break;
	return 0;

    case WM_CREATE:
	menu = GetMenu (wnd);
	menu = GetSubMenu (menu, MENU_TERMINAL_POSITION);
	menu = GetSubMenu (menu, MENU_SERVCHAR_POSITION);
	dtcEC_InitMenu (menu, ID_TERMINAL_SERVER_MIN);
#ifdef WIN32
	termWheelAction (message, wparam, lparam);
#endif
	termMouseAction (message, wparam, lparam, &msg);
	break;

    case WM_TIMER:
	if (socketConnected() &&
	    rawGetProtocol()==protoTelnet) {
	    rawNop();
	}
	return 0;

#ifdef WIN32
    case WM_SETTINGCHANGE:
	termWheelAction (message, wparam, lparam);
	termMouseAction (message, wparam, lparam, &msg);
	return 0;

    case WM_MOUSEWHEEL:
	termWheelAction (message, wparam, lparam);
	return 0;
#endif
    case WM_CLOSE:
	if (socketOffline() || telnetConfirmClose()) {
	    DestroyWindow (wnd);
	}
	return 0;
    }
#if defined(_WIN32)
    retval= DefWindowProcW (wnd, message, wparam, lparam);
#else
    retval= DefWindowProc (wnd, message, wparam, lparam);
#endif
    return retval;
}

/* Set the application window title.
 *
 * When we are online, the session host/port are displayed.
 * When we are logging, the name of the log file is displayed.
 */
void telnetSetTitle()
{
    char title[256], *ptitle= title;
    int dtcc;
    unsigned len;

    if (termHasTitle()) {
	ptitle= termGetTitle (&len, &dtcc);

    } else {
	if (socketConnected())
	    sprintf(title, "%s/%s", socketGetHost(), socketGetPort());
	else
	    strcpy(title, telnetAppName());

	if (logGetTitle() != NULL)
	    sprintf(title + strlen(title), " - %s", logGetTitle());

	len= strlen (title);
	dtcc= DTCHAR_ANSI;
    }

#if defined(_WIN32)
    if (len) {
	ConstBuffData bfrom;
	DynBuffer bwide= EmptyBuffer;

	bfrom.len= len;
	bfrom.ptr= ptitle;

	uconvBytesToUtf16 (&bfrom, dtcc, &bwide);
	BuffAddTermZeroes (&bwide, sizeof (wchar_t));

	SetWindowTextW (telnetWnd, (wchar_t *)bwide.ptr);

	if (bwide.maxlen) BuffCutback (&bwide, 0);
    }
#else
    SetWindowText(telnetWnd, title);
#endif
}

#if !defined(WIN32)
static WPARAM timerId;
#endif

/* Initialise this process instance
 */
static BOOL initInstance(int cmdShow)
{
    fontGetProfile();
    logGetProfile();
    proxyGetProfile();
    printingGetProfile();
    termGetProfile();

#if defined(_WIN32)
    telnetWnd = CreateWindowW (telnetWinClassW,
			       telnetAppNameW (),
			       WS_OVERLAPPEDWINDOW,
			       CW_USEDEFAULT, CW_USEDEFAULT,
			       CW_USEDEFAULT, CW_USEDEFAULT,
			       NULL, NULL, instanceHnd, NULL);
#else
    telnetWnd = CreateWindow(telnetWinClass,
			     telnetAppName(),
			     WS_OVERLAPPEDWINDOW,
			     CW_USEDEFAULT, CW_USEDEFAULT,
			     CW_USEDEFAULT, CW_USEDEFAULT,
			     NULL, NULL, instanceHnd, NULL);
#endif
    if (!telnetWnd)
	return FALSE;
    if (!statusCreateWnd(instanceHnd, telnetWnd))
	return FALSE;
    if (!termCreateWnd(instanceHnd, telnetWnd))
	return FALSE;
    if (!socketInit(instanceHnd))
	return FALSE;

    fileLoadFavorites(FILETYPE_LOAD_MENU);

    if (cmdShow != SW_MAXIMIZE) {
	initSetWindowSize();
	initSetWindowPos(telnetWnd);
    }

#if !defined(WIN32)
    {
	UINT timerinterval;

    #if !defined(WIN32)
	timerinterval= 65535U; /* sigh: 65.5 sec */
    #else
	timerinterval= 10*60*1000; /* 10 min */
    #endif
	timerId= SetTimer (telnetWnd, timerId, timerinterval, NULL);
    }
#endif

    ShowWindow(telnetWnd, cmdShow);
    UpdateWindow(telnetWnd);
    return TRUE;
}

/* Initialise the application when first instance
 */
static BOOL initApplication(void)
{
#if defined(_WIN32)
    WNDCLASSW wc;
#else
    WNDCLASS  wc;
#endif
    BOOL rc;

    if (!socketInitClass(instanceHnd))
	return FALSE;
    if (!termInitClass(instanceHnd))
	return FALSE;
    if (!statusInitClass(instanceHnd))
	return FALSE;

    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = mainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = instanceHnd;
    wc.hIcon = LoadIcon(instanceHnd, MAKEINTRESOURCE(IDR_TELNET));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

#if defined(_WIN32)
    wc.lpszMenuName =  MAKEINTRESOURCEW (IDM_TELNET_MENU);
    wc.lpszClassName = telnetWinClassW;
    rc = RegisterClassW (&wc);
#else
    wc.lpszMenuName =  MAKEINTRESOURCE (IDM_TELNET_MENU);
    wc.lpszClassName = telnetWinClass;
    rc = RegisterClass (&wc);
#endif
    return rc;
}

static TermKeyMapStringNum strn [] = {
    {"PrintScreen", ID_TERMINAL_PRINTSCREEN},
    {"NewWindow",   ID_CONNECT_NEW}
};
#define Nstrn (sizeof(strn) / sizeof(strn[0]))

static TermKeyMapStringNumS strs = {
    0, strn, Nstrn
};
TermKeyMapStringNumS *dtelnetKeyMapStrings = &strs;

static struct {
    int cmdShow;
    const char* autoHost;	/* host from command line */
    const char* autoPort;	/* port from command line */
    const char* autoTerm;	/* terminal emulation from command line */
    const char* replayFile;	/* name of replay file from command line */
    BOOL haveSession;		/* true if user asks for a preset session */
    Connect conn;
} argset = {
    SW_SHOWDEFAULT,
    NULL,
    NULL,
    NULL,
    NULL,
    FALSE,
    EmptyConnect
};

/* parseArgs:
	we might want to call function this twice, first for option -I<inifile>,
	and later for everything else
	classmask=0: execute no options
	classmask=1: execute options from class1 (-I)
	classmask=2: execute options from class2 (everything else)
	classmask=3: execute options from both classes
 */

static int parseArgs (int classmask, int argc, const char **argv, int *ret_argi)
{
    int c;
    int errVal;
    OptRet opt;
/* if you have to reorder options, change OPTION_**** too */
/*                                               1 11  1           1  1         1      1 */
/*                              12 3 4 5 6 7 8 9 0 12  3           4  5         6      7 */
static const char options [] = "BI:G:H:P:S:T:U:C:R:DX:(AnswerBack):A:(Maximize)(Kpam):(VtKeyMap)";
#define OPTION_ANSWERBACK (GETOPT_LONGOPT+13)
#define OPTION_MAXIMIZE   (GETOPT_LONGOPT+15)
#define OPTION_KPAM       (GETOPT_LONGOPT+16)
#define OPTION_VTKEYMAP   (GETOPT_LONGOPT+17)
    AttrDefaultData blankAttr;

    errVal = 0 ;  /* 'no error' now */

    getoptStart (argc, argv, options);
    while (errVal == 0 && ((c = getopt(&opt)) != EOF)) {
	if (c=='?') {
	    errVal= 1;
	    continue;

	} else {
	    int nclass;

	    if (c=='I') nclass= 0;	/* classification: class 0: option 'I' */
	    else        nclass= 1;	/*		   class 1: every other */
	    if ((classmask&(1<<nclass))==0) continue;
	}
	switch (c) {
	case 'B':
	    /* Set binary mode
	     */
	    rawEnableBinaryMode();
	    break;

	case 'A':
	    /* Set color of blank terminal window
	     */
	    parseAttrib(opt.optarg, &blankAttr);
	    termSetBlankAttr (&blankAttr, OptFromCmdLine);
	    break;

        case 'C':
	    /* Set server character set mode
	     */
	    connectSetServerCharSet (opt.optarg);
	    break;

	case 'G':
	    /* Set terminal window size (in characters)
	     */
	    termSetGeometry(opt.optarg, OptFromCmdLine);
	    break;

	case 'H':
	    /* Set host to connect to on startup
	     */
	    argset.autoHost = opt.optarg;
	    break;

	case 'P':
	    /* Set the port to connect to on startup
	     */
	    argset.autoPort = opt.optarg;
	    break;

	case 'S': /* protocol: telnet, rlogin */
	    argset.conn.protocol[0]= '\0';
	    strncat (argset.conn.protocol, opt.optarg, sizeof argset.conn.protocol -1);
	    break;

	case 'T':
	    /* Set terminal type to emulate
	     */
	    argset.autoTerm = opt.optarg;
	    break;

	case 'U':
	    /* Set username to specify when connecting to rlogin port
	     * on startup.
	     */
	    connectSetUser(opt.optarg);
	    break;

	case 'I':
	    /* Specify the name of the .INI file to use
	     */
	    src_iniFileName= OptFromCmdLine;
	    iniFileName= opt.optarg;
	    break;

	case 'R':
	    /* Specify name of the log file to replay on startup
	     */
	    argset.replayFile = opt.optarg;
	    break;

	case 'D':
            connectSetBs2Del(TRUE);
	    break;

	case 'X':
	    argset.haveSession = fileLoadSessionByName(opt.optarg, &argset.conn);
	    break;

	case OPTION_ANSWERBACK:
	    connectSetAnswerBack (opt.optarg);
	    break;

	case OPTION_MAXIMIZE: /* 20071205.LZS.*/
	    argset.cmdShow = SW_MAXIMIZE;
	    break;

	case OPTION_KPAM:
	    connectSetKeypadMode (opt.optarg);
	    break;

	case OPTION_VTKEYMAP:
	    connectSetVtKeyMap (TRUE);
	    break;

        /* error */
	case '?':
            errVal = 1 ;
        break;
	}
    }

    if (ret_argi) *ret_argi= opt.optind;
    return errVal;
}

static void doDefaultIniFile (const char *inifile, char **into, OptSource *sinto);

/* Program entry point
 */
int PASCAL WinMain(HINSTANCE instance, HINSTANCE prevInstance,
		   LPSTR cmdLine, int cmdShow)
{
    const char* argv[32];	/* command line arguments */
    int argc, argi;		/* number of command line arguments */
    int errVal;			/* current command line argument */
    AttrDefaultData blankAttr;	/* will be initialized later (defaultBlankAttr) */

    /* Standard application initialisation sequence
     */
    instanceHnd = instance;

    if (!prevInstance)
	if (!initApplication())
	    return FALSE;

    if (cmdLine && *cmdLine) {
	argc = getoptInit(cmdLine, (char **)argv, 32);
    } else {
	argc= 0;
    }
    argset.cmdShow= cmdShow;

    parseArgs (1, argc, argv, NULL); /* only option -I is handled now */

    if (src_iniFileName == OptUnset) {
	doDefaultIniFile ("dtelnet.ini", &iniFileName, &src_iniFileName);
    } else {
	/* <FIXME> We could check if the specied file exists		*/
	/* or at least it could be created,				*/
	/* but only if it contains path (one or more \backslashes),	*/
	/* otherwise it is up to Windows where to put the INI-file,	*/
	/* including nowhere (ie the registry)				*/
    }

    /* 20041118.LZS: connect.c now requires us
     *               to initialize dlgVars from defVars
     */
    connectGetDefVars (&argset.conn);
    connectLoadVars (&argset.conn, OptFromDefault);

    /* 20050125.LZS: connectGetProfile moved here from initInstance */

    connectGetProfile();

    /* Set default terminal attributes and default .INI file
     */

    defaultBlankAttr (&blankAttr);	/* white/White on black/Black */
    termSetBlankAttr (&blankAttr, OptFromDefault);
    initTermSet();

    /* 20120816.LZS: Load keymaps from dtelnet.ini */
    tkmLoadFromIniFile (TKM_DEFAULT_MAP, iniFileName, "Keymap", dtelnetKeyMapStrings);

    /* 20151017.LZS: Load EnvVars dtelnet.ini */
    EV_LoadFromIniFile (DefaultEnvVars, telnetIniFile (), NULL);

    /* Parse command line: every options but -I are handled now */
    errVal = parseArgs (2, argc, argv, &argi);

    if ( 0 == errVal ) {
	if (argset.haveSession) { /* We got a session file */
            connectLoadVars(&argset.conn, OptFromTsFile);

	} else {
	    /* Unix compatible behaviour: telnet host [port]
	     */
	    if (argi < argc)
		argset.autoHost = argv[argi++];
	    if (argi < argc)
		argset.autoPort = argv[argi++];

	    /* If host was specified on command line, set session parameters
             */
            if (argset.autoHost != NULL) {
                connectSetHost(argset.autoHost);
                connectSetPort("telnet");
            }
            if (argset.autoPort != NULL)
                connectSetPort(argset.autoPort);
            if (argset.autoTerm != NULL)
                connectSetTerm(argset.autoTerm);
        }

        /* Initialise this application instance
         */
        if (!initInstance(argset.cmdShow))
            return FALSE;

        /* Replay log file if specified
         */
        if (argset.replayFile != NULL)
	    logReplay(argset.replayFile);

        /* Show socket status in status bar, then initiate connection if
         * host specified on command line.
         */
        socketShowStatus();
        if ((argset.autoHost != NULL) || argset.haveSession)
	    connectOpenHost();

        /* We have a slightly non-standard message loop.  In all cases, we
         * want to give user interface processing precedence over the
         * network.  To achieve this, the socket IO messages are used to
         * set status indicators.  When the GUI is idle (message queue is
         * empty), we go back and check those indicators.  It is at this
	 * point that the actual network IO occurs.
         */
        for (;;) {
	    /* Get a message if one is available.  If not return
	     * immediately so we can service the socket IO messages.
	     */
	    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
	        if (msg.message == WM_QUIT)
		    break;
	    } else {
	        /* The message queue is empty.  Process any pending socket
	         * IO.  Then wait for the next message - this way we avoid
	         * spinning on the PeekMessage when there are no messages.
	         *
	         * For the sake of user convenience, do not perform socket
	         * IO when the user is performing a selection with the
	         * mouse.
	         */
	        if (!termSelectInProgress())
		    socketProcessPending();
	        if (!GetMessage(&msg, NULL, 0, 0))
		    break;
	    }

	    /* Handle the message.  First try routing the message to a
	     * registered dialog.
	     */
	    if (!dialogCheckMessage(&msg)) {
	        TranslateMessage(&msg);
	        DispatchMessage(&msg);
	    }
        }

        /* Stop logging to log file
         */
        logStop();

	/* Cleanup and save profile in the .INI file
	 */
	telnetSaveProfile();

	socketCleanup();
	connectReleaseResources();
        telnetDestroyTermProfiles(&emuls);
     }

     /* Exit the application
      */
     return ( 0 == errVal ? msg.wParam : 0 ) ;
}

void telnetSaveProfile (void)
{
    connectSaveProfile();
    fontSaveProfile();
    logSaveProfile();
    proxySaveProfile();
    printingSaveProfile();
    termSaveProfile();
}

static void doDefaultIniFile (const char *inifile, char **into, OptSource *sinto)
{
    const char *confdir= NULL;   /* eg C:\Home\Zsiga\Dtelnet\			*/
    const char *exedir=  NULL;	 /* eg C:\Bin\					*/
    char *confdir_inifile= NULL; /* eg C:\Home\Zsiga\Dtelnet\dtelnet.ini	*/
    char *exedir_inifile=  NULL; /* eg C:\Bin\dtelnet.ini			*/
    BOOL fConfDir= FALSE;
    BOOL fConfDir_IniFile= FALSE;
    BOOL fExeDir= FALSE;
    BOOL fExeDir_IniFile= FALSE;

    confdir= getConfDir ();
    fConfDir= confdir!=NULL && *confdir!='\0';

    if (fConfDir) {
	confdir_inifile= fileInDir (confdir, inifile);		/* remember xfree! */
	if (access (confdir_inifile, 0)==0) fConfDir_IniFile= TRUE;
    }

    if (!fConfDir_IniFile) {
	exedir= getExePath ();
	fExeDir= exedir!=NULL && *exedir!='\0';

	if (fExeDir) {
	    exedir_inifile= fileInDir (exedir, inifile);	/* remember xfree */
	    if (access (exedir_inifile, 0)==0) fExeDir_IniFile= TRUE;
	}
    }

    if (!fConfDir_IniFile && fConfDir && fExeDir_IniFile) {
	BOOL fcopied= CopyFile (exedir_inifile, confdir_inifile, TRUE);

	if (fcopied) {
	    fConfDir_IniFile= TRUE;
	    MessageBoxF (NULL, "Ini-file copied", MB_ICONINFORMATION | MB_OK,
		"Ini-file '%.128s' copied into '%.128s'.\n"
		"From now on, the latter will be used.",
		exedir_inifile, confdir_inifile);
	}
    }

    if (fConfDir_IniFile ||
	(fConfDir && !fExeDir_IniFile)) {
	*sinto= OptFromDefault;
	*into= confdir_inifile;
	confdir_inifile= NULL;		/* don't call xfree */

    } else {
	*sinto= OptFromDefault;
	*into= exedir_inifile;
	exedir_inifile= NULL;		/* don't call xfree */
    }

/* RETURN: */
    if (confdir_inifile) xfree (confdir_inifile);
    if (exedir_inifile)  xfree (exedir_inifile);
}
