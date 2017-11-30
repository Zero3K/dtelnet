/* about.c
 * Copyright (C) 1998 David Cole
 *
 * Implement the About dialog
 */
#include "preset.h"

#include <stdio.h>
#include <windows.h>

#include "utils.h"
#include "platform.h"
#include "resource.h"
#include "about.h"
#include "dtelnet.h"
#include "dialog.h"
#include "filetype.h"

static BOOL haveDialog;		/* have we created the About dialog? */

/* Dialog procedure for the About dialog
 */
static DIALOGRET CALLBACK aboutDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_INITDIALOG: {
	char tmp [4096], *p= tmp;
	const char *tsfile;
	char setname [_MAX_PATH];

	p += sprintf (p, "Executable:\n  %s\nIni-file:\n  %s",
	    getExeName(), telnetIniFile());

	makeIniFileName (setname, "dtelnet.set");
	p += sprintf (p, "\nSet-file:\n  %s", setname);

	tsfile= getTsfileName ();
	if (tsfile && *tsfile) {
	    p += sprintf (p, "\nSession-file:\n  %s", tsfile);
	}
	if (hasConfDir ()) {
	    p += sprintf (p, "\nConfig:\n  %s", getConfDir ());
	} else {
	    p += sprintf (p, "\nHOME:\n  Unset or invalid");
	}
	SetDlgItemText (dlg, IDC_ABOUT_INFO, tmp);

	/* Flag that we have created the dialog, and register the
	 * dialog window handle with the dialog handling code.
	 */
	haveDialog = TRUE;
	dialogRegister(dlg);
	return TRUE; }

    case WM_SIZE: {
	PixelRect rDlg, rInfo;
	PixelSize sDlg, sInfo, sInfoNew;
	HWND hInfo;

	GetWindowRect (dlg, (RECT *)&rDlg);
	sDlg.cx = rDlg.right - rDlg.left;
	sDlg.cy = rDlg.bottom- rDlg.top;

	hInfo= GetDlgItem (dlg, IDC_ABOUT_INFO);
	if (!hInfo) return FALSE;

	GetWindowRect (hInfo, (RECT *)&rInfo);
	sInfo.cx = rInfo.right - rInfo.left;
	sInfo.cy = rInfo.bottom- rInfo.top;

	sInfoNew.cx = sDlg.cx - 2*(rInfo.left - rDlg.left); /* width of dialog - margins */
	sInfoNew.cy = sInfo.cy;				    /* unchanged */

	if (sInfo.cx>=64 && sInfo.cy>=32) {
	    SetWindowPos (hInfo, NULL, 0, 0, (int)sInfoNew.cx, (int)sInfoNew.cy,
		SWP_NOMOVE | SWP_NOZORDER);
	    InvalidateRect (hInfo, NULL, TRUE);
	}
	return FALSE;
    }

    case WM_COMMAND:
	switch (LOWORD (wparam)) {
	case IDOK:
	case IDCANCEL:
	    /* Destroy the dialog and unregister the dialog handle
	     * with the dialog handling code.
	     */
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    haveDialog = FALSE;
	    return TRUE;
	}
	break;
    }
    return FALSE;
}

/* Called when the user invokes the About dialog
 */
void aboutShowDialog(HINSTANCE instance, HWND wnd)
{
    if (!haveDialog)
	CreateDialog(instance, MAKEINTRESOURCE(IDD_ABOUT_DIALOG),
		     wnd, aboutDlgProc);
}

