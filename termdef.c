/*
  Customizable definitions for terminal type variations.
  The definitions are stored in an INI-file called 'DTELNET.SET'
  and can be edited from within dtelnet.

  -- Mark Melvin
*/
#include <windows.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "dialog.h"
#include "dtelnet.h"
#include "emul.h"
#include "term.h"
#include "termdef.h"
#include "utils.h"

extern EmulNames emuls;

static char termsetFileName[_MAX_PATH + 1];	/* INI file to use */

/* Determine the name of the application .INI file
 */
static void makeTermSetFileName(void)
{
    makeIniFileName (termsetFileName, "dtelnet.set");
}

static BOOL haveTerminalDefDlg;

static char *mescape (const char *from)
{
    static char esc[32];

    escape (from, strlen (from), esc, sizeof (esc));
    return esc;
}

static void munescape (const char *from, char *to)
{
    unescape (from, strlen (from), to, KEYCODE_LEN);
}

/* Show the Emul structure onto the dialog items */
static void terminalDefinitionDlgShow(HWND dlg, const Emul *editEmul)
{
    char nrFkey[10];
    int escStyle;
    int i;

    SetDlgItemText(dlg, IDC_TOSERVER, editEmul->servname);

    SetDlgItemText(dlg, IDC_KPGUP, mescape(editEmul->keyPageUp));
    SetDlgItemText(dlg, IDC_KPGDN, mescape(editEmul->keyPageDown));
    SetDlgItemText(dlg, IDC_KHOME, mescape(editEmul->keyHome));
    SetDlgItemText(dlg, IDC_KEND,  mescape(editEmul->keyEnd));
    SetDlgItemText(dlg, IDC_KINS,  mescape(editEmul->keyInsert));
    SetDlgItemText(dlg, IDC_KDEL,  mescape(editEmul->keyDelete));
    SetDlgItemText(dlg, IDC_KBS,   mescape(editEmul->keyBackSpace));
    SetDlgItemText(dlg, IDC_KABS,  mescape(editEmul->keyAltBackSpace));
    SetDlgItemText(dlg, IDC_KBTAB, mescape(editEmul->keyBacktab));

    sprintf(nrFkey, "%d", editEmul->nMaxFkey);
    SetDlgItemText(dlg, IDC_NFFUNCKEYS, nrFkey);

    for (i=0; i<24; ++i) {
	char buff [512];

	escape (editEmul->keyFn[i].ptr, editEmul->keyFn[i].len, buff, sizeof (buff));
	SetDlgItemText (dlg, IDC_KEYF1+i, buff);
    }

    CheckDlgButton(dlg, IDC_CKMOUSE, (editEmul->flags & ETF_MOUSE_EVENT)? 1 : 0);

    escStyle = editEmul->flags & ETF_ESC_PARO;
    CheckDlgButton(dlg, IDC_CKPARO, (escStyle ? 1 : 0));
    if (escStyle)
    {
        CheckRadioButton(dlg, IDC_RPARO_VT320, IDC_RPARO_LINUX,
            ((escStyle==ETF_ESC_PARO_VT320)?IDC_RPARO_VT320:IDC_RPARO_LINUX));
    }
}

/* Save the (modified) dialog items into Emul */
static void terminalDefinitionDlgSave(HWND dlg, Emul *editEmul)
{
    char fkey[KEYCODE_LEN];
    char buff[256];
    char buff2[256];
    ConstBuffData cbd;
    int i, nfnkey;
    size_t len;

    len= GetDlgItemText(dlg, IDC_TOSERVER, buff, sizeof (buff));
    if (len) {
        strncpy (editEmul->servname, buff, sizeof (editEmul->servname));
    } else {
        strcpy (editEmul->servname, editEmul->name);
    }

#define GETKEYCODE(dlgitemid,keyvar) \
GetDlgItemText(dlg, dlgitemid, fkey, KEYCODE_LEN); \
munescape(fkey, keyvar);

#define GETFUNKEYCODE(dlgitemid,pkeyvar) \
	len= GetDlgItemText(dlg, dlgitemid, buff, sizeof (buff)); \
	cbd.len= unescape(buff, len, buff2, sizeof (buff2)); \
	cbd.ptr= buff2; \
	BuffCopyZ (pkeyvar, &cbd);

    GETKEYCODE(IDC_KPGUP, editEmul->keyPageUp)
    GETKEYCODE(IDC_KPGDN, editEmul->keyPageDown)
    GETKEYCODE(IDC_KHOME, editEmul->keyHome)
    GETKEYCODE(IDC_KEND, editEmul->keyEnd)
    GETKEYCODE(IDC_KINS, editEmul->keyInsert)
    GETKEYCODE(IDC_KDEL, editEmul->keyDelete)
    GETKEYCODE(IDC_KBS, editEmul->keyBackSpace)
    GETKEYCODE(IDC_KABS, editEmul->keyAltBackSpace)
    GETKEYCODE(IDC_KBTAB, editEmul->keyBacktab)

    editEmul->nMaxFkey = GetDlgItemInt(dlg, IDC_NFFUNCKEYS, NULL, FALSE);
    if (editEmul->nMaxFkey > MAX_F_KEY) {
	editEmul->nMaxFkey = MAX_F_KEY;
    }

    nfnkey= editEmul->nMaxFkey;
    if (nfnkey>24) nfnkey= 24;

    for (i=0; i<nfnkey; ++i) {
	GETFUNKEYCODE(IDC_KEYF1+i, &editEmul->keyFn[i])
    }

    editEmul->flags = 0;
    if(IsDlgButtonChecked(dlg, IDC_CKMOUSE)){
	editEmul->flags |= ETF_MOUSE_EVENT;
    }
    if(IsDlgButtonChecked(dlg, IDC_CKPARO)){
	if (IsDlgButtonChecked(dlg, IDC_RPARO_LINUX)) {
            editEmul->flags |= ETF_ESC_PARO_LINUX;
        }
        else if (IsDlgButtonChecked(dlg, IDC_RPARO_VT320)) {
            editEmul->flags |= ETF_ESC_PARO_VT320;
        }
    }
}

static void terminalDefinitionDlgEnableEdit(HWND dlg, BOOL isEnabled)
{
    int i;
    int tmpIsEnabled;

    EnableWindow(GetDlgItem(dlg, IDC_TOSERVER), isEnabled);

    EnableWindow(GetDlgItem(dlg, IDC_KPGUP), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KPGDN), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KHOME), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KEND), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KINS), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KDEL), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KBS), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KABS), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_KBTAB), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_CKMOUSE), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_CKPARO), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_RPARO_VT320), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_RPARO_LINUX), isEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_NFFUNCKEYS), isEnabled);

    for (i=0; i<24; ++i) {
	EnableWindow(GetDlgItem(dlg, IDC_KEYF1+i), isEnabled);
    }

    tmpIsEnabled= isEnabled && IsDlgButtonChecked(dlg, IDC_CKPARO);
 
    EnableWindow(GetDlgItem(dlg, IDC_RPARO_LINUX), tmpIsEnabled);
    EnableWindow(GetDlgItem(dlg, IDC_RPARO_VT320), tmpIsEnabled);
}

static DIALOGRET CALLBACK termnameDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    static char* name;
    switch (message){
    case WM_INITDIALOG:
        name = (char*)lparam;
	SetFocus(GetDlgItem(dlg, IDC_TERMNAME)); 
	return FALSE;
    case WM_COMMAND:
        switch LOWORD(wparam){
        case IDOK:
            GetDlgItemText(dlg, IDC_TERMNAME, name, TERM_NAME_LEN);
            EndDialog(dlg, IDOK);
            break;
	case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
        }
        return TRUE;
    }
    return FALSE;
}

static DIALOGRET CALLBACK terminalDefinitionDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
static Emul editEmul;
static char name[TERM_NAME_LEN];
static BOOL editing;
    unsigned int idx;

    switch (message) {
    case WM_INITDIALOG:
	/* Flag that we have created the dialog, and register the
	 * dialog window handle with the dialog handling code.
	 */
	haveTerminalDefDlg = TRUE;
	dialogRegister(dlg);

	memset (&editEmul, 0, sizeof (editEmul));
	memset (name, 0, sizeof (name));
	editing= FALSE;

        telnetEnumTermProfiles(&emuls);
	for (idx = 0; idx<emuls.num; idx++)
        {
	    SendDlgItemMessage(dlg, IDC_COMBOBOX1, CB_ADDSTRING, 0,
			      (LPARAM)emuls.names[idx]);
	}
        SetDlgItemText(dlg, IDC_BUTTON1, "Add New");
        EnableWindow(GetDlgItem(dlg, IDC_BUTTON2), FALSE);
        terminalDefinitionDlgEnableEdit(dlg, FALSE);
	return TRUE;

    case WM_COMMAND:
	switch (LOWORD (wparam)) {
        case IDC_COMBOBOX1:
            /* When user select a terminal name, load definition
               and update the dialog entries accordingly.
             */
#ifdef WIN32
	    if (HIWORD(wparam) != CBN_SELCHANGE)
		break;
#else
	    if (HIWORD(lparam) != CBN_SELCHANGE)
		break;
#endif
            GetDlgItemText(dlg, IDC_COMBOBOX1, name, sizeof(name));
            if (strlen(name)!=0)
            {
            	telnetGetTermProfile(&editEmul, name);
                terminalDefinitionDlgShow(dlg, &editEmul);
                SetDlgItemText(dlg, IDC_BUTTON1, "Modify");
	    }

            break;
        case IDC_CKPARO:
            /* If checked, the radiobutton linux or vt320 becomes
               available, otherwise disabled.
             */
#ifdef WIN32
	    if (HIWORD(wparam) != BN_CLICKED)
		break;
#else
	    if (HIWORD(lparam) != BN_CLICKED)
		break;
#endif
            if (IsWindowEnabled(GetDlgItem(dlg, IDC_CKPARO)) &&
                IsDlgButtonChecked(dlg, IDC_CKPARO))
            {
                EnableWindow(GetDlgItem(dlg, IDC_RPARO_LINUX), TRUE);
		EnableWindow(GetDlgItem(dlg, IDC_RPARO_VT320), TRUE);
            }
            else
            {
                EnableWindow(GetDlgItem(dlg, IDC_RPARO_LINUX), FALSE);
                EnableWindow(GetDlgItem(dlg, IDC_RPARO_VT320), FALSE);
            }
            break;

        case IDC_BUTTON1:
            /* Add or Modify, depending on profile name selected
	       (or lack thereof). */
            if (strlen(name)==0) /* this is new, so ask for name */
            {
                int dlgResult = DialogBoxParam(telnetGetInstance(),
                    MAKEINTRESOURCE(IDD_TERMNAME), dlg, termnameDlgProc,
                    (LPARAM)name);
                if ((dlgResult != IDOK)||(strlen(name)==0)){
		    /* no name given, exit */
                    return TRUE;
                }
            }
	    idx= (unsigned)SendDlgItemMessage(dlg, IDC_COMBOBOX1, CB_ADDSTRING, 0, (LPARAM)name);
	    if (idx==(unsigned)LB_ERR) break;
	    SendDlgItemMessage(dlg, IDC_COMBOBOX1, CB_SETCURSEL, idx, 0);
	    SetDlgItemText(dlg, IDC_TOSERVER, name);
            terminalDefinitionDlgEnableEdit(dlg, TRUE);
            EnableWindow(GetDlgItem(dlg, IDC_BUTTON2), TRUE);
            editing = TRUE;
            break;

        case IDC_BUTTON2:
            /* Save */
            terminalDefinitionDlgSave(dlg, &editEmul);
	    telnetSaveTermProfile(&editEmul, name);

	    /* Destroy the dialog and unregister the dialog handle
	     * with the dialog handling code.
	     */
	    goto LEAVE_DIALOG;

	case IDOK:
	case IDCANCEL:
            if (editing)
            {
		char tmp[512];

		sprintf (tmp, "Save changes of '%s'?", name);
                switch (MessageBox(dlg, tmp, "Terminal Definitions",
                   MB_YESNOCANCEL|MB_ICONQUESTION))
                {
                    case IDCANCEL:
                        return TRUE;
		    case IDYES:
                        terminalDefinitionDlgSave(dlg, &editEmul);
                        telnetSaveTermProfile(&editEmul, name);
                }
            }
	    /* Destroy the dialog and unregister the dialog handle
	     * with the dialog handling code.
	     */
LEAVE_DIALOG:
	    emulRelBuff (&editEmul);
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    haveTerminalDefDlg = FALSE;
	    return TRUE;
	}
	break;
    }

    return FALSE;
}

void terminalDefShowDialog(HINSTANCE instance, HWND wnd)
{
    if (!haveTerminalDefDlg)
	CreateDialog(instance, MAKEINTRESOURCE(IDD_TERMINAL_DEFINITION),
		     wnd, terminalDefinitionDlgProc);
}

#ifndef WIN32
/* Win16 doesn't provide enumerating INI sections, so we create one.
  --(melvin)
 */
void GetPrivateProfileSectionNames(char* buffer, int size, char *fname)
{
    char line[4096];
    char *last;         /* pointer to buffer tail */
    FILE *f = fopen (fname, "r");
    last = buffer;

    while ((!feof(f)) && (last<(buffer+size)))
    {
        int len;
    	fgets(line, 4095, f);                  /* read lines from file */
        len = strlen(line) - 1;                /* don't count line end */
        if ((last+len) > (buffer+size)) break; /* guard against overflow */

	/* if line looks like [section] */
        if ((len > 2)&&(line[0]=='[')&&(line[len-1]==']'))
        {
            /* append it into buffer (without the brackets)*/
            memcpy(last, line+1, len-2);
            last[len-2] = '\0';   /* null-terminate each name*/
            last += (len-1);
        }
    }
    last[0] = '\0';  /* extra null for end */
    fclose(f);
};
#endif

void telnetEnumTermProfiles(EmulNames *emuls)
{
    static char namesbuffer[4096];
    static BOOL namesSet = FALSE;
    static int nTermNames = 0;
    static char **pTermNames = NULL;
    char* c;

    if (!namesSet)
    {
	GetPrivateProfileSectionNames(namesbuffer, sizeof(namesbuffer),
	   termsetFileName);
	namesSet = TRUE;

	nTermNames = 0;
	pTermNames = (char **)calloc(1024, sizeof(char*));
	c = namesbuffer;
	while (*c)
	{
	    pTermNames [nTermNames] = c;
	    nTermNames++;
	    while (*c++);
	}
	pTermNames = xrealloc(pTermNames, nTermNames * sizeof(char*));
    }
    emuls->num = nTermNames;
    emuls->names = pTermNames;
}

void telnetDestroyTermProfiles(EmulNames *emuls)
{
    ASSERT(emuls);
    free(emuls->names);
}

/* Fills Emul structure with default (linux) terminal settings
 */
void telnetCreateDefaultTermProfile(Emul* emul)
{
    int fkey;

    strcpy(emul->name, "linux");
    strcpy(emul->servname, emul->name);
    emul->flags = ETF_ESC_PARO_LINUX | ETF_SHCTRL_RXVT;
    strcpy(emul->keyPageUp, "\x1b[5~");
    strcpy(emul->keyPageDown, "\x1b[6~");
    strcpy(emul->keyHome, "\x1b[1~");
    strcpy(emul->keyEnd, "\x1b[4~");
    strcpy(emul->keyInsert, "\x1b[2~");
    strcpy(emul->keyDelete, "\x1b[3~");
    strcpy(emul->keyBacktab, "\x1b[Z");
    strcpy(emul->keyBackSpace, "\x7f");
    strcpy(emul->keyAltBackSpace, "\x08");
    strcpy(emul->keyNum5, "\x1b[G");
    emul->nMaxFkey = 20;
    for (fkey=0; fkey < emul->nMaxFkey; fkey++)
    {
	char fkeycode[KEYCODE_LEN];
	size_t len;
	ConstBuffData cb;

	/* generate linux fkey codes */
	if (fkey<5){
	    len= sprintf(fkeycode, "\x1b[[%c", 'A'+fkey);
	}
	else {
	    len= sprintf(fkeycode, "\x1b[%d~", (fkey<14)?(12+fkey):(13+fkey));
	}
	/*generate fkey names */
	cb.ptr= fkeycode;
	cb.len= len;
	BuffCopyZ (&emul->keyFn[fkey], &cb);
    }
}

static void telnetClearTermProfile (Emul* emul, const char *name)
{
    int i;

    strncpy (emul->name, name, TERM_NAME_LEN);
    strncpy (emul->servname, name, TERM_NAME_LEN);
    emul->flags= 0;

/* 20140114.LZS: from now on, 'rxvt' is the default,
 *	'xterm' is the special'
 */
    if (stricmp (name, "xterm")==0 ||
	strnicmp(name, "xterm-", 6)==0) {
	emul->flags |= ETF_SHCTRL_XTERM_NEW;

    } else if (stricmp (name, "konsole")==0 ||
	       strnicmp(name, "konsole-", 8)==0) {
	emul->flags |= ETF_SHCTRL_KONSOLE;

    } else {
	emul->flags |= ETF_SHCTRL_RXVT;
    }

    emul->keyPageUp[0]=	      '\0';
    emul->keyPageDown[0]=     '\0';
    emul->keyHome[0]=         '\0';
    emul->keyEnd[0]=          '\0';
    emul->keyInsert[0]=       '\0';
    emul->keyDelete[0]=       '\0';
    emul->keyBacktab[0]=      '\0';
    emul->keyBackSpace[0]=    '\0';
    emul->keyAltBackSpace[0]= '\0';
    emul->keyNum5[0]=         '\0';
    emul->nMaxFkey= 0;
    for (i=0; i<MAX_F_KEY; ++i) {
	emul->keyFn[i].len= 0;
    }
}

/* used to be a macro */

static int GET_TERM_KEYDEF (const char *termname,
	const char *keyname, char keyvar[KEYCODE_LEN])
{
static const char unset[]= "*UNSET";
    char buffer [512];

    buffer[0]= '\0';
    GetPrivateProfileString (termname, keyname, unset,
	buffer, sizeof(buffer), termsetFileName);
    if (strcmp (buffer, unset)==0) return 1;

    munescape (buffer, keyvar);
    return 0;
}

/* used to be a macro */

static int GET_TERM_FUNKEYDEF (const char *termname,
	const char *keyname, Buffer *pkeyvar)
{
static const char unset[]= "*UNSET";
    char buffer [512];
    char buff2[512];
    size_t len;
    ConstBuffData cbd;

    buffer[0]= '\0';
    len= GetPrivateProfileString (termname, keyname, unset,
	buffer, sizeof (buffer), termsetFileName);
    if (strcmp (buffer, unset)==0) return 1;

    cbd.len= unescape (buffer, len, buff2, sizeof (buff2));
    cbd.ptr= buff2; \
    BuffCopyZ (pkeyvar, &cbd);	/* this appends a '\0' too */
    return 0;
}

/* Fills Emul structure with terminal settings with name
 */
static void telnetGetTermProfileCore (Emul* emul, const char *name)
{
    char buffer[512];
    int fkey;
    static const char unset [] = "*UNSET";

/* you can create infinite loop with 'CopyFrom',
 * but don't blame me if you really do
 */
    GetPrivateProfileString (name, "CopyFrom", unset,
	buffer, sizeof (buffer), termsetFileName);
    if (strcmp (buffer, unset)!=0) {
	telnetGetTermProfileCore (emul, buffer);
    }

    GetPrivateProfileString(name, "SendToServer", unset,
	buffer, sizeof (buffer), termsetFileName);
    if (strcmp (buffer, unset)!=0) {
	strncpy (emul->servname, buffer, TERM_NAME_LEN);
    }

    GetPrivateProfileString (name, "CharsetSupport", unset,
	buffer, sizeof (buffer), termsetFileName);
    if (strcmp (buffer, unset)!=0) {
	emul->flags &= ~ETF_ESC_PARO;
	if      (stricmp (buffer, "linux")==0) emul->flags |= ETF_ESC_PARO_LINUX;
	else if (stricmp (buffer, "vt320")==0) emul->flags |= ETF_ESC_PARO_VT320;
    }

    GetPrivateProfileString(name, "SupportMouse", unset,
	buffer, sizeof (buffer), termsetFileName);
    if (strcmp (buffer, unset)!=0) {
	emul->flags &= ~ETF_MOUSE_EVENT;
	if (stricmp (buffer, "yes")==0) emul->flags |= ETF_MOUSE_EVENT;
    }

    GET_TERM_KEYDEF (name, "kpgup", emul->keyPageUp);
    GET_TERM_KEYDEF (name, "kpgdn", emul->keyPageDown);
    GET_TERM_KEYDEF (name, "khome", emul->keyHome);
    GET_TERM_KEYDEF (name, "kend", emul->keyEnd);
    GET_TERM_KEYDEF (name, "kins", emul->keyInsert);
    GET_TERM_KEYDEF (name, "kdel", emul->keyDelete);
    GET_TERM_KEYDEF (name, "kbtab", emul->keyBacktab);
    GET_TERM_KEYDEF (name, "kbs", emul->keyBackSpace);
    GET_TERM_KEYDEF (name, "kabs", emul->keyAltBackSpace);
    GET_TERM_KEYDEF (name, "knum5", emul->keyNum5);

    emul->nMaxFkey = GetPrivateProfileInt (name, "nfunc",
	emul->nMaxFkey, termsetFileName);
    if (emul->nMaxFkey > MAX_F_KEY) {
	emul->nMaxFkey = MAX_F_KEY;
    }

    for (fkey=0; fkey < emul->nMaxFkey; fkey++) {
	char fkeyname[6];

	/*generate fkey names */
	sprintf(fkeyname, "kf%d", fkey+1);
	GET_TERM_FUNKEYDEF (name, fkeyname, &emul->keyFn[fkey]);
    }

    GetPrivateProfileString (name, "acsc", unset,
	buffer, sizeof (buffer), termsetFileName);
    if (strcmp (buffer, unset)!=0) {
	char tmpacsc [2*256+1];
	int tal;

	if (stricmp (buffer, "vt100")==0) {
	    const char vt100graph []= "+,-.0`afghijklmnopqrstuvwxyz{|}~";
	    const char *p;
	    char *q= tmpacsc;

	    for (p= vt100graph; *p; ++p) {
		*q++ = *p;
		*q++ = *p;
	    }
	    tal= q-tmpacsc;
	} else {
	    tal = unescape (buffer, strlen (buffer), tmpacsc, sizeof (tmpacsc));
	}
	ctabConst (emul->ctGraphConv, 0);
	ctabModif (emul->ctGraphConv, tmpacsc, tal/2, 1);
    }

    GetPrivateProfileString (name, "ShCtrlMagic", unset,
	buffer, sizeof (buffer), termsetFileName);
    if (strcmp (buffer, unset)!=0) {
	emul->flags &= ~ETF_SHCTRL_MASK;
	if      (stricmp (buffer, "rxvt")==0)      emul->flags |= ETF_SHCTRL_RXVT;
	else if (stricmp (buffer, "xterm-new")==0) emul->flags |= ETF_SHCTRL_XTERM_NEW;
	else if (stricmp (buffer, "konsole")==0)   emul->flags |= ETF_SHCTRL_KONSOLE;
    }
}

void telnetGetTermProfile (Emul* emul, const char *name)
{
    telnetClearTermProfile (emul, name);
    telnetGetTermProfileCore (emul, name);
}

/* Saves emul terminal settings with name.
 */
void telnetSaveTermProfile(const Emul* emul, char *name)
{
    int fkey;
    char strnkey[5];
    char ebuff [512];
    int escPar0;

    WritePrivateProfileString(name, "SendToServer", emul->servname, termsetFileName);

    escPar0 = emul->flags & ETF_ESC_PARO;
    WritePrivateProfileString(name, "CharsetSupport",
       (escPar0==ETF_ESC_PARO_LINUX ? "linux" :
       (escPar0==ETF_ESC_PARO_VT320 ? "vt320" : "")),
       termsetFileName);

    WritePrivateProfileString(name, "SupportMouse",
       ((emul->flags & ETF_MOUSE_EVENT) ? "yes" : "no"),
       termsetFileName);

    WritePrivateProfileString (name, "ShCtrlMagic",
       (emul->flags & ETF_SHCTRL_MASK)==ETF_SHCTRL_RXVT?      "rxvt":
       (emul->flags & ETF_SHCTRL_MASK)==ETF_SHCTRL_XTERM_NEW? "xterm-new":
       (emul->flags & ETF_SHCTRL_MASK)==ETF_SHCTRL_KONSOLE?   "konsole":
	"no",
	termsetFileName);

#define WRITE_TERM_KEYDEF(keyname, keyvar) \
    WritePrivateProfileString(name, keyname, mescape(keyvar), termsetFileName)

#define WRITE_TERM_FUNKEYDEF(keyname, pkeyvar) \
    escape ((pkeyvar)->ptr, (pkeyvar)->len, ebuff, sizeof (ebuff)); \
    WritePrivateProfileString(name, keyname, ebuff, termsetFileName)

    WRITE_TERM_KEYDEF("kpgup", emul->keyPageUp);
    WRITE_TERM_KEYDEF("kpgdn", emul->keyPageDown);
    WRITE_TERM_KEYDEF("khome", emul->keyHome);
    WRITE_TERM_KEYDEF("kend", emul->keyEnd);
    WRITE_TERM_KEYDEF("kins", emul->keyInsert);
    WRITE_TERM_KEYDEF("kdel", emul->keyDelete);
    WRITE_TERM_KEYDEF("kbtab", emul->keyBacktab);
    WRITE_TERM_KEYDEF("kbs", emul->keyBackSpace);
    WRITE_TERM_KEYDEF("kabs", emul->keyAltBackSpace);
    WRITE_TERM_KEYDEF("knum5", emul->keyNum5);

    wsprintf(strnkey, "%d", emul->nMaxFkey);
    WritePrivateProfileString(name, "nfunc", strnkey, termsetFileName);
    for (fkey=0; fkey<(emul->nMaxFkey); fkey++)
    {
	char fkeyname[6];
	/*generate fkey names */
	sprintf(fkeyname, "kf%d", fkey+1);
	WRITE_TERM_FUNKEYDEF(fkeyname, &emul->keyFn[fkey]);
    }
}

void initTermSet(void)
{ 
    makeTermSetFileName();
   /* Check if dtelnet.set. exists */
    if(access(termsetFileName, 0)!=0)
    { /* file not exist, create an emergency one */
        MessageBox(NULL, "DTelnet.set file not found, creating new.", "DTelnet", MB_OK);
        telnetCreateDefaultTermProfile(emulGetTerm());
        telnetSaveTermProfile(emulGetTerm(), "linux");
        if(access(termsetFileName, 0)!=0)
        { /* Still not exist ??  We have a problem */
            MessageBox(NULL, "DTelnet.set cannot be created.", "DTelnet Error", MB_OK);
        }
    }
    emulSetTerm("linux");
}
