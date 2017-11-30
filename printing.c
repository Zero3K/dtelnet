/* printing.c
Dtelnet printing facility
provides XTERM/VT100 attached printer emulation

   - initiated 2001-05-31 Mark Melvin 

*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "resource.h"
#include "dialog.h"
#include "term.h"
#include "dtelnet.h"
#include "printing.h"
#include "utils.h"

#define PRINTERNAMELEN 128
#define PRINTBUFFERSIZE 4096

/* These are the print buffering variables */
/* zero initialized by the compiler */
static struct {
    Buffer buff;
    int currline;
} print;

static char *getPrintingBuff (void)
{
    if (!print.buff.ptr) {
	print.buff.ptr= xmalloc (PRINTBUFFERSIZE);
	print.buff.maxlen= PRINTBUFFERSIZE;
	print.buff.len= 0;
    }
    return print.buff.ptr;
}

/* preferences */
static int marginTop = 0;
static int marginBottom = 0;
static int marginLeft = 0;
static int marginRight = 0;
static char printerName[PRINTERNAMELEN];

/* These are the dimensions of the paper */
static int numrows;
static int numcols;

static int charWidth;
static int charHeight;

static BOOL pageBreak = FALSE;

/* The controlling variable. Every entry points must be checked against this. */
static BOOL printingActive = FALSE;

/* The printer's resource */
static HDC printDC;

/* .INI file Strings */
static const char printingStr[] = "Printing";
static const char printerStr[] = "Printer";
static const char marginLeftStr[] = "Margin Left";
static const char marginTopStr[] = "Margin Top";
static const char marginRightStr[] = "Margin Rigth";
static const char marginBottomStr[] = "Margin Bottom";

/**************************** UI functions ***************************/

static HWND dlgPrinting = NULL;
static HWND dlgPreferences = NULL;

/* Dialog procedure for the print dialog
 */
static DIALOGRET CALLBACK printingDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_INITDIALOG:
	dialogRegister(dlg);
	return TRUE;
    case WM_COMMAND:
	switch (LOWORD(wparam)) {
	case IDCANCEL:
		/* User pressed Cancel, abort printing */
		AbortDoc(printDC);
		printingActive = FALSE;		
		EndDialog(dlg, IDCANCEL);
		break;
	case IDOK:
		EndDialog(dlg, IDOK);
		dialogUnRegister(dlg);
	}
	break;
    }
    return FALSE;
}

static void printingShowDialog(void)
{
	dlgPrinting = CreateDialog(telnetGetInstance(), MAKEINTRESOURCE(IDD_PRINTING_DIALOG),
		telnetGetWnd(), printingDlgProc);
}

static void printingCloseDialog(void)
{
	if (dlgPrinting) {
		SendMessage(dlgPrinting, WM_COMMAND, IDOK, 0L);
	}
}

/******************* preferences *********************/

static void prefEnumPrinters(HWND comboWnd)
{
#ifdef WIN32
    PRINTER_INFO_1 *pi;
    DWORD i, count, needed, flags;
    flags = PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS;
    EnumPrinters(flags, NULL, 1, NULL, 0, &needed, &count);
    pi = (PRINTER_INFO_1*) xmalloc(needed);
    EnumPrinters(flags, NULL, 1, (BYTE*) pi, needed, &needed, &count);
    for ( i = 0 ; i < count ; i++ ) {
	SendMessage(comboWnd, CB_ADDSTRING, 0, (LPARAM) pi[i].pName);
    }
    free(pi);
#else
    char szPrinter[80];
    GetProfileString ("windows", "device", ",,,", szPrinter, sizeof(szPrinter));
    if (strtok(szPrinter, ",") != NULL) {
        SendMessage(comboWnd, CB_ADDSTRING, 0, (LPARAM) szPrinter);
    }
#endif
    SendMessage(comboWnd, CB_SELECTSTRING, 0, (LPARAM) printerName);
}

static DIALOGRET CALLBACK prefDlgProc(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
	dlgPreferences = dlg;
	dialogRegister(dlg);
	SetDlgItemInt(dlg, IDC_PRINTPREF_MLEFT, marginLeft, FALSE);
	SetDlgItemInt(dlg, IDC_PRINTPREF_MTOP, marginTop, FALSE);
	SetDlgItemInt(dlg, IDC_PRINTPREF_MRIGHT, marginRight, FALSE);
	SetDlgItemInt(dlg, IDC_PRINTPREF_MBOTTOM, marginBottom, FALSE);
	prefEnumPrinters(GetDlgItem(dlg, IDC_PRINTPREF_PRINTERS));
	return TRUE;
    case WM_COMMAND:
	switch (LOWORD(wparam))
	{
	case IDOK:
	    marginLeft = GetDlgItemInt(dlg, IDC_PRINTPREF_MLEFT, NULL, FALSE);
	    marginTop = GetDlgItemInt(dlg, IDC_PRINTPREF_MTOP, NULL, FALSE);
	    marginRight = GetDlgItemInt(dlg, IDC_PRINTPREF_MRIGHT, NULL, FALSE);
	    marginBottom = GetDlgItemInt(dlg, IDC_PRINTPREF_MBOTTOM, NULL, FALSE);
	    /* if any margin is higher than MARGIN_MAX, truncate it silently
	     */
#define MARGIN_MAX 1000
	    marginLeft = min(marginLeft, MARGIN_MAX);
	    marginTop = min(marginTop, MARGIN_MAX);
	    marginRight = min(marginRight, MARGIN_MAX);
	    marginBottom = min(marginBottom, MARGIN_MAX);
	    GetDlgItemText(dlg, IDC_PRINTPREF_PRINTERS, printerName, sizeof(printerName));
	case IDCANCEL:
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    dlgPreferences = NULL;
	    return TRUE;
	}
	break;
    }
    return FALSE;
}

void printingPrefDialog(void)
{
    if (!dlgPreferences) {
	CreateDialog(
	    telnetGetInstance(), 
	    MAKEINTRESOURCE(IDD_PRINTING_PREF),
	    telnetGetWnd(),
	    prefDlgProc
	);
    }
}

/************************* Internal Functions ************************/


/* Starting a new page */
static void printingNewPage(void)
{
	print.currline = 0;
	StartPage(printDC);
	pageBreak = FALSE;
}

/* End of page */
static void printingEndPage(void)
{
	EndPage(printDC);
}

/*
Starting a print job:
   - get the default printer from win.ini
   - init the printer DC
   - calculate output sizes from the DC
*/
static BOOL printingNewJob(void)
{
    DOCINFO docInfo;
    TEXTMETRIC tm;

#ifdef WIN32
    printDC = CreateDC ("WINSPOOL", printerName, NULL, NULL);
#else
    static char szPrinter[80];
    char *szDevice, *szDriver, *szOutput;

    GetProfileString ("windows", "device", ",,,", szPrinter, 80);
    if (NULL != (szDevice = strtok(szPrinter, ",")) &&
        NULL != (szDriver = strtok(NULL, ",")) &&
        NULL != (szOutput = strtok(NULL, ","))) {
		printDC = CreateDC (szDriver, szDevice, szOutput, NULL);
    }
#endif
    if (!printDC) {
	MessageBox(telnetGetWnd(), "Printer not found.", telnetAppName(), MB_OK | MB_ICONSTOP);
	return FALSE;
    }

    docInfo.cbSize = sizeof(DOCINFO);
    docInfo.lpszDocName = "DTelnet Session";
    docInfo.lpszOutput = NULL;

    GetTextMetrics(printDC, &tm);
    charWidth = tm.tmAveCharWidth;
    charHeight = (tm.tmHeight+tm.tmExternalLeading);
    numcols = (GetDeviceCaps(printDC, HORZRES)-(marginLeft+marginRight)) 
	    / charWidth;
    numrows = (GetDeviceCaps(printDC, VERTRES)-(marginTop+marginBottom))
	    / charHeight;

    printingShowDialog();
    if (StartDoc(printDC, &docInfo) <= 0) {
	DeleteDC(printDC);
	printingCloseDialog(); 
	MessageBox(telnetGetWnd(), "Printer is inaccesible.", telnetAppName(), MB_OK|MB_ICONSTOP);
	return FALSE;
    }
    printingNewPage();

    return TRUE;
}

/* Terminate the printing job */
static void printingEndJob(void)
{
	printingEndPage();
	EndDoc(printDC);
	DeleteDC(printDC);
	printingCloseDialog();
}

/* Render a line */
static void printingRenderLine (const char* startpos, int linelength)
{
	if (pageBreak){
		printingEndPage();
		printingNewPage();
	}
	TextOut(printDC, marginLeft, marginTop+(print.currline * charHeight), startpos, linelength);
	print.currline++;
	if (print.currline > numrows) {
		pageBreak = TRUE;
	}
}

/* Split buffer into lines and send to renderer */
static void printingFlush (void)
{
	const char *from;
	size_t rest;

	rest = print.buff.len;
	from = print.buff.ptr;

        /* Split lines to fit in numcols */
	while (rest > 0) {
		size_t length = (rest <= (size_t)numcols) ? rest : (size_t)numcols;

		printingRenderLine (from, length);
		from += length;
		rest -= length;
	}

	print.buff.len= 0;
}

/********************* Exported Functions ***********************/

/* Inject a character into buffer */
void printingAddChar(char c)
{
	switch (c) {
        case '\n':
                printingFlush();
                break;

        case '\t':
                /* change tabs into spaces, 8 is hard-coded tab-size for now */
		do {
			printingAddChar(' ');
		} while (print.buff.len < (size_t)numcols && print.buff.len % 8 != 0);
		break;

        case 12:
                /* do a page break */
                printingFlush();
                pageBreak = TRUE;
                break;

        default:
		getPrintingBuff ();

		if (print.buff.len < print.buff.maxlen) {
		    print.buff.ptr [print.buff.len++] = c;
		}
                /* force newline if buffer exhausted */
		if (print.buff.len == print.buff.maxlen) {
		    printingFlush();
		}
		break;
	}
}

/* Start printing on attached printer */
void printingStart()
{
	if (!printingActive)
	{
		printingActive = printingNewJob();
	}
}

/* Stop printing */
void printingStop()
{
	if (printingActive)
	{
		printingFlush();
		printingEndJob();
		printingActive = FALSE;
	}
}

BOOL printingIsActive(void)
{
	return printingActive;
}
	
void printingGetProfile(void)
{
    marginLeft = GetPrivateProfileInt(
	printingStr, marginLeftStr, marginLeft, telnetIniFile());
    marginTop = GetPrivateProfileInt(
	printingStr, marginTopStr, marginTop, telnetIniFile());
    marginRight = GetPrivateProfileInt(
	printingStr, marginRightStr, marginRight, telnetIniFile());
    marginBottom = GetPrivateProfileInt(
	printingStr, marginBottomStr, marginBottom, telnetIniFile());
    GetPrivateProfileString(
	printingStr, printerStr, "", printerName, sizeof(printerName), telnetIniFile());
}

void printingSaveProfile (void)
{
    char str[24];
    sprintf(str, "%d", marginLeft);
    WritePrivateProfileString(printingStr, marginLeftStr, str, telnetIniFile());
    sprintf(str, "%d", marginTop);
    WritePrivateProfileString(printingStr, marginTopStr, str, telnetIniFile());
    sprintf(str, "%d", marginRight);
    WritePrivateProfileString(printingStr, marginRightStr, str, telnetIniFile());
    sprintf(str, "%d", marginBottom);
    WritePrivateProfileString(printingStr, marginBottomStr, str, telnetIniFile());
    WritePrivateProfileString(printingStr, printerStr, printerName, telnetIniFile());
}

/*** functions to support Screen Dump operations ***/

static void printingAddLine (const Line *line)
{
    int cs;

    cs = DTCHAR_OEM; /* OEM - but why? */
    print.buff.len= 0;
    linesGetChars (LGC_MODE_NARROW,
		   line, (Column)0, line->len,
		   &print.buff, &cs);
    printingFlush();
}

void printingPrintScreen()
{
    int row;
    int idx=term.topVisibleLine;

    if (printingActive) {
	MessageBox(telnetGetWnd(), "Printer busy.", telnetAppName(), MB_OK|MB_ICONEXCLAMATION);
	return;
    }

    if (printingNewJob())
    {
        // Print all visible lines of the terminal window.
        for (row = 0 ; row < TERM_MAX_Y && row < term.winSize.cy &&
	    	       idx < term.linebuff.used; 
	     row++, idx++) {
            printingAddLine (term.linebuff.list[idx]);
        }
	printingEndJob();
    }
}
