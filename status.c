/* status.c
 * Copyright (c) 1997 David Cole
 *
 * Control and manage the status bar
 */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "platform.h"
#include "term.h"
#include "emul.h"
#include "utils.h"
#include "status.h"

static char statusWinClass[] = "DStatusWClass";	/* statusbar window class name */
static int pad;			/* padding around the text in the bar */
static int aveCharWidth;	/* average width of font in status bar */
static HWND statusWnd;		/* status window handle */
static HFONT statusFont;	/* font to use */
static char statusMsg[200];	/* current status message */

#define SRN_Message	0	/* window area used to show status messages */
#define SRN_KeyLock	1	/* KeyPad: NumLock,CapsLock */
#define SRN_KeyPadMode	2	/* KeyPad: Num/Appl */
#define SRN_TermType	3	/* window area used to show terminal type */
#define SRN_WinSize	4	/* window area used to show terminal size */
#define SRN_Elements	5

typedef struct StatusDataType {
    RECT rects[SRN_Elements];	/* the parts of the status line, see SRN_* */
    const char *strings[SRN_Elements];
} StatusDataType;

#define PANE_PAD 2

/* Return the handle of the status bar
 */
HWND statusGetWnd (void)
{
    return statusWnd;
}

/* Format and display a status message
 *
 * Args:
 * fmt - printf format string
 * ... - arguments to format string
 */
void statusSetMsg(const char* fmt, ...)
{    
    va_list ap;

    va_start(ap, fmt);
    vsprintf(statusMsg, fmt, ap);
    va_end(ap);
    if (statusWnd)
	InvalidateRect(statusWnd, NULL, TRUE);
}

/* Tell the status bar to redisplay the terminal type
 */
void statusSetTerm (void)
{
    statusInvalidate();
}

static void statusCalculatePartSize (HDC dc, const char *str, RECT *rest, RECT *into)
{
    SIZE width;
    unsigned wreq, wtot;

    GetTextExtentPoint(dc, str, strlen(str), &width);
    wreq = width.cx + 2 * (pad + 2) +  2 * PANE_PAD;
    wtot = rest->right - rest->left;
    if (wreq > wtot) wreq= wtot;

    *into = *rest;
    into->left = into->right - wreq;
    rest->right = into->left;
}

void statusInvalidate (void)
{
    InvalidateRect (statusWnd, NULL, TRUE);
}

/* The status bar has been resized.  Recalculate all of the drawing
 * areas.
 */
static void statusCalculate (HDC dc, StatusDataType *stadata)
{
    RECT rect;			/* status bar window rectangle */
    int i;

    if (statusWnd == 0)
	return;

    GetClientRect(statusWnd, &rect);
    rect.top += PANE_PAD;

    /* Recalculate the positions of the panes
     */

    for (i=SRN_Elements; --i >= 1;) {
	if (stadata->strings[i] && *stadata->strings[i]) {
	    statusCalculatePartSize (dc, stadata->strings[i], &rect, &stadata->rects[i]);
	} else {
	    memset (&stadata->rects[i], 0, sizeof stadata->rects[i]);
	}
    }

    stadata->rects[SRN_Message] =  rect;
}

/* Redraw a single pane
 */
static void statusDrawPane(HDC dc, LPRECT rect, const char *paneText, HPEN pens[2])
{
    RECT rc;

    SetRect(&rc, rect->left, rect->top, rect->right-1, rect->bottom-1);
    SelectObject(dc, pens[0]);

    MoveTo(dc, rc.left, rc.bottom-1);
    LineTo(dc, rc.left, rc.top);
    LineTo(dc, rc.right, rc.top);
    SelectObject(dc, pens[1]);
    MoveTo(dc, rc.right, rc.top);
    LineTo(dc, rc.right, rc.bottom);
    LineTo(dc, rc.left-1, rc.bottom);

    SetRect(&rc, rc.left + 1 + PANE_PAD, rc.top+1, rc.right-1, rc.bottom-1);
    DrawText(dc, paneText, strlen(paneText), &rc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
}

/* Redraw the status bar
 */
static void statusPaint(void)
{
    PAINTSTRUCT ps;
    HDC dc;
    HFONT oldFont;
    char buffKeyLock[40];
    char buffKeyPad[40];
    char buffWinSize[40];
    StatusDataType stadatav, *stadata= &stadatav;
    HPEN oldPen;
    HPEN pens[2];
    int i;

    /* Initialise device context for repainting status bar
     */
    dc = BeginPaint(statusWnd, &ps);

    SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
    SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
    oldFont = (HFONT)SelectObject(dc, statusFont);

    pens[0]= CreatePen (PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
    pens[1]= CreatePen (PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
    oldPen = (HPEN) SelectObject(dc, pens[0]);

    memset (stadata, 0, sizeof *stadata);
    stadata->strings[SRN_Message]=    statusMsg;
    stadata->strings[SRN_KeyLock]=    buffKeyLock;
    stadata->strings[SRN_KeyPadMode]= buffKeyPad;
    stadata->strings[SRN_TermType]=   emulGetTerm()->name;
    stadata->strings[SRN_WinSize]=    buffWinSize;

    termKeyLockStatus    (buffKeyLock);
    termKeypadModeStatus (NULL, buffKeyPad);
    termFormatSizeStatus (buffWinSize);

    statusCalculate (dc, stadata);

    for (i=SRN_Elements; --i >= 0;) {
	if (stadata->rects[i].left < stadata->rects[i].right &&
	    RectVisible (dc, &stadata->rects[i])) {
	    statusDrawPane (dc, &stadata->rects[i], stadata->strings[i], pens);
	}
    }

    /* Clean up the device context
     */
    SelectObject(dc, oldPen);
    DeleteObject(pens[0]);
    DeleteObject(pens[1]);
    SelectObject(dc, oldFont);
    EndPaint(statusWnd, &ps);
}

/* Window procedure for the status bar
 */
static LPARAM CALLBACK statusWndProc
	(HWND wnd, UINT message, WPARAM wparam, LPARAM lparam)
{                                   
    switch (message) {
    case WM_PAINT:
	statusPaint();
	return 0;

    case WM_SIZE:
	InvalidateRect(statusWnd, NULL, TRUE);
	return 0;

    case WM_DESTROY:
	DeleteObject(statusFont);
	statusWnd = NULL;
	return 0;
    }

    return DefWindowProc(wnd, message, wparam, lparam);
}

/* Register the status bar window class
 */
BOOL statusInitClass(HINSTANCE instance)
{
    WNDCLASS wc;

    wc.style = 0;
    wc.lpfnWndProc = statusWndProc;
    wc.cbClsExtra = wc.cbWndExtra = 0;
    wc.hInstance = instance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszMenuName =  NULL;
    wc.lpszClassName = statusWinClass;
    return RegisterClass(&wc);
}

/* Create the status bar window
 *
 * Args:
 * instance - application instance handle
 * parent -   parent window (application window)
 */
BOOL statusCreateWnd(HINSTANCE instance, HWND parent)
{
    HDC dc;			/* calculate height via font metrics */
    TEXTMETRIC tm;
    LOGFONT lf;
    HFONT oldFont;
    RECT parentClient;		/* client rectangle of parent window */
    SIZE winSize;		/* calculate size of status bar */

    /* Work out the height of the status bar by querying the font that
     * will be used to draw in it
     */
    dc = GetDC(parent);

    /* Create the font that will be used to draw in the status bar
     */
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight = -MulDiv(8, GetDeviceCaps(dc, LOGPIXELSY), 72);
    lf.lfWeight = FW_NORMAL;
    lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
    strcpy(lf.lfFaceName, "MS Sans Serif");
    statusFont = CreateFontIndirect(&lf);

    oldFont = (HFONT)SelectObject(dc, statusFont);
    GetTextMetrics(dc, &tm);
    SelectObject(dc, oldFont);

    ReleaseDC(parent, dc);

    /* Build some padding around the various elements of the status bar
     */
    pad = tm.tmExternalLeading + 1;
    aveCharWidth = tm.tmAveCharWidth;

    /* Create the status bar at the bottom of the parent window.
     */
    GetClientRect(parent, &parentClient);
    winSize.cx = parentClient.right;
    winSize.cy = (tm.tmHeight + 2 * pad) + (PANE_PAD * 2) + pad;
    statusWnd = CreateWindow(statusWinClass, "", WS_CHILD,
	  0, parentClient.bottom-winSize.cy, winSize.cx, winSize.cy,
	  parent, NULL, instance, NULL);
    if (statusWnd == NULL)
	return FALSE;

    /* Display the status bar
     */
    ShowWindow(statusWnd, SW_SHOW);
    UpdateWindow(statusWnd);
    return TRUE;
}
