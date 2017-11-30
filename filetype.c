/* filetype.c -- Handling connection files
	 20011009 (Mark Melvin)
*/

#include "preset.h"
#include <stdlib.h>
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <string.h>
#include <io.h>

#ifndef _WIN32
#ifdef USE_DIRECT_H
    #include <direct.h>
#else
    #include <dir.h>
    #include <dirent.h>
#endif
#endif

#include <stdio.h>

#include "platform.h"
#include "utils.h"
#include "dtelnet.h"
#include "emul.h"
#include "socket.h"
#include "connect.h"
#include "term.h"
#include "raw.h"
#include "dialog.h"
#include "filetype.h"
#include "dtchars.h"
#include "termkey.h"
#include "envvars.h"

/* <HELPME> prefix underline required or not? */
#if !defined strcasecmp
#define strcasecmp  stricmp
#define strncasecmp strnicmp
#endif

/* Section header inside the session file */
#define SECTION "Telnet Shortcut"

typedef struct ExtData {
    size_t n;
    const char **str;
} ExtData;

#if defined(_WIN32)
    static const char *extlist [] = 
	{"dtelnet_ses", "ts"};
    #define MAXEXTLEN 12
#else
    static const char *extlist [] = 
	{"ts"};
    #define MAXEXTLEN 2
#endif

static const ExtData extdata = 
{
    sizeof extlist / sizeof extlist[0],
    extlist
};

#define DESCRIPTION "DTelnet Shortcut"
/* TYPE_REG_LEN is the length of TYPE_REG, these two must be kept in sync */
#define TYPE_REG "DTelnetShortcut"
#define TYPE_REG_LEN 15

static HWND manageDlg = NULL;	    /* manage dialog handle */
static BOOL haveEditDlg = FALSE;
static Connect editVars;
static char dlgTitle[_MAX_PATH];
static BOOL newFile;
static char saveName[_MAX_PATH];

extern struct Protocols protocols[];
extern unsigned int numProtocols;

static size_t favSearch (const ConstBuffData *favname, size_t n);

static BOOL buildPathName (Buffer *into, int indx)
{
    size_t reqlen;
    const char *dirname;
    const FavoriteRec *favp;
    BOOL retval;

    if (indx<0 || (size_t)indx>favorites.n) {
	retval= FALSE;
	goto RETURN;
    }
    favp= &favorites.p[indx];

    dirname= favp->dir==FAVDIR_CONF? getConfDir(): getExePath();

    reqlen= strlen (dirname) +
	    strlen (favp->name) +
	    1 + MAXEXTLEN;
    BuffSecX (into, reqlen);

    into->len= sprintf (into->ptr + into->len, "%s%s.%s",
	dirname, favp->name, extdata.str[favp->extno]);
    retval= TRUE;

RETURN:
    return retval;
}

/* fill the path of favorites file */
static BOOL expandFavPathB (Buffer *into, int indx)
{
    BOOL retval;

    into->len= 0;
    if (!favorites.loaded) {
	favLoadAll (FALSE);
    }

    retval= buildPathName (into, indx);

    return retval;
}

/* fill the path of favorites file */
static BOOL expandFavPath(char *fullname, int indx)
{
    BOOL retval;
    Buffer bfname= EmptyBuffer;

    retval= expandFavPathB (&bfname, indx);
    if (bfname.len) {
	memcpy (fullname, bfname.ptr, bfname.len); /* What about buffer overload?! */
    }
    fullname[bfname.len]= '\0';

    if (bfname.ptr) xfree (bfname.ptr);
    return retval;
}

static const char *getFavName(int indx)
{
    const FavoriteRec *favp;
    const char *retval;

    if (!favorites.loaded) {
	favLoadAll (FALSE);
    }
    if (indx<0 || (size_t)indx>favorites.n) {
	retval= NULL;
	goto RETURN;
    }
    favp= &favorites.p[indx];
    retval= favp->name;

RETURN:
    return retval;
}

static char *tsfileName= NULL;
/* getTsfileName: returns the full name of the last loaded, if any */
const char *getTsfileName (void)
{
    return tsfileName;
}

static BOOL fileLoadSessionCore (const char* fname, Connect* conn);

/* something.ts.dtelnet_ses.ts -> something */
static void StripOurExtensions (ConstBuffData *fn)
{
    const char *lastbs;
    int leave;

    lastbs=  memrchr (fn->ptr, '\\', fn->len);

    for (leave= 0; !leave; ) {
	const char *dot;
	int found;
	ConstBuffData ext;
	unsigned i;

	dot= memrchr (fn->ptr, '.',  fn->len);
	if (!dot || (lastbs && dot<lastbs)) {
	    leave= 1;
	    continue;
	}

	ext.ptr= dot+1;
	ext.len= (size_t)(fn->ptr + fn->len - ext.ptr);

	for (i= 0, found= 0; !found  && i<extdata.n; ++i) {
	    if (ext.len == strlen (extdata.str[i]) &&
		strncasecmp (ext.ptr, extdata.str[i], ext.len)==0) {
		found= 1;
		fn->len= (size_t)(dot - fn->ptr);
	    }
	}
	if (!found) leave= 1;
    }
}

/* Load a shortcut file into Connect structure */
BOOL fileLoadSessionByName (const char* filename, Connect* conn)
{
    int rc;
    BOOL retval;
    ConstBuffData cbfilename;
    Buffer bfilename= EmptyBuffer;

    rc= access (filename, 0);
    if (rc==0) {
	retval= fileLoadSessionCore (filename, conn);
	goto RETURN;
    }

    cbfilename.ptr= filename;
    cbfilename.len= strlen (cbfilename.ptr);

    StripOurExtensions (&cbfilename);

#if 0
/* perhaps we should add our '.ts' extension */
    if (! BuffDataEndI (&cbfilename, &DotFileType)) {
	Buffer bfilename= EmptyBuffer;

	BuffAppend (&bfilename, &cbfilename);
	BuffAppend (&bfilename, &DotFileType);
	BuffAddTermZero (&bfilename);
	
	rc= access (bfilename.ptr, 0);
	if (rc==0) {
	    retval= fileLoadSessionCore (bfilename.ptr, conn);
	    goto RETURN;
	}
    }
#endif

/* Ok, we didn't find it in with the 'direct method' */
    if (strchr(filename, '\\')) {
	/* it had a pathname-part, so we won't do favSearch */
	retval= FALSE;
	goto RETURN;
    }

    { /* search favorites */
	int indx;

	if (!favorites.loaded) favLoadAll (FALSE);

    /* remove extension */
	cbfilename.ptr= filename;
	cbfilename.len= strlen (cbfilename.ptr);

#if 0
        if (BuffDataEndI (&cbfilename, &DotFileType)) {
	    cbfilename.len -= DotFileType.len;
	    if (cbfilename.len==0) return FALSE;
	}
#endif

	indx= favSearch (&cbfilename, favorites.n);
	if (indx==-1) {
	    retval= FALSE;
	    goto RETURN;
	}
	bfilename.len= 0;
	expandFavPathB (&bfilename, indx);
	retval= fileLoadSessionCore (bfilename.ptr, conn);
    }

RETURN:
    if (bfilename.ptr) xfree (bfilename.ptr);
    return retval;
}

/* Load a shortcut file into Connect structure */
static BOOL fileLoadSessionByIndex (int indx, Connect* conn)
{
    BOOL retval;
    Buffer bfilename= EmptyBuffer;

    retval= expandFavPathB (&bfilename, indx);
    if (! retval) goto RETURN;

    retval= fileLoadSessionCore (bfilename.ptr, conn);
RETURN:
    if (bfilename.ptr) free (bfilename.ptr);
    return retval;
}

static BOOL fileLoadSessionCore (const char* fname, Connect* conn)
{
    int rc;
    char tmp[256];
    size_t len;
    Buffer bfullname= EmptyBuffer;
    const char *fullpath;

    rc= access (fname, 0);
    if (rc) {
	Buffer message= EmptyBuffer;
	size_t reqlen;

	reqlen= strlen (fname) + 32;
	BuffSecX (&message, reqlen);
	message.len= sprintf (message.ptr, "Error opening file %s", fname);
	MessageBox(NULL, message.ptr, "DTelnet", MB_OK | MB_ICONEXCLAMATION);

	if (message.ptr) xfree (message.ptr);
	return FALSE;
    }

    if (strchr (fname, '\\')) { /* ok, there is 'PATH' in filename */
	fullpath= fname;

    } else {			/* no 'PATH' in filename, add some */
	size_t reqlen;

	reqlen= strlen (fname) + 8;
	BuffSecX (&bfullname, reqlen);
	bfullname.len= sprintf (bfullname.ptr, ".\\%s", fname);
	fullpath= bfullname.ptr;
    }

    /* Load variables from file with ini-file format */
    GetPrivateProfileString(SECTION, "Host", "", conn->host, HOST_NAME_LEN, fullpath);
    GetPrivateProfileString(SECTION, "Port", "", conn->port, PORT_NAME_LEN, fullpath);
    GetPrivateProfileString(SECTION, "Protocol", "none", conn->protocol, PROTOCOL_NAME_LEN, fullpath);
    GetPrivateProfileString(SECTION, "Term", "", conn->term, TERM_NAME_LEN, fullpath);
    GetPrivateProfileString(SECTION, "User", "", conn->user, USER_NAME_LEN, fullpath);
    GetPrivateProfileString(SECTION, "Charset", "", conn->charset, CHARSET_NAME_LEN, fullpath);

    GetPrivateProfileString(SECTION, "AnswerBack", "*NOTSET", tmp, sizeof (tmp),
			    fullpath);
    if (strcmp (tmp, "*NOTSET")!=0) {
	conn->abacklen =
	    (short)unescape (tmp, strlen (tmp),
			     conn->answerback, sizeof (conn->answerback));
    } /* else keep original setting */

    conn->locexec = GetPrivateProfileBoolean (fullpath, SECTION, "LocalExec", FALSE);
    conn->bs2Del  = GetPrivateProfileBoolean (fullpath, SECTION, "BS2Del",    FALSE);
    conn->vtkeymap= GetPrivateProfileBoolean (fullpath, SECTION, "VtKeyMap",  FALSE);

    GetPrivateProfileString(SECTION, "Kpam", "", tmp, sizeof (tmp),
			    fullpath);
    connectSetKeypadModeTo (conn, tmp);

/* keymap: first load section "[Keymap]" from "dtelent.ini" then from "foo.ts" 
   the latter can overwrite the former */
    tkmReleaseMap (TKM_DEFAULT_MAP);
    tkmLoadFromIniFile (TKM_DEFAULT_MAP, telnetIniFile (), TKM_DEFAULT_SECTION, dtelnetKeyMapStrings);
    tkmLoadFromIniFile (TKM_DEFAULT_MAP, fullpath, TKM_DEFAULT_SECTION, dtelnetKeyMapStrings);

/* keymap: first load section "[EnvVars]" from "dtelent.ini" then from "foo.ts"
   the latter can overwrite the former */
    EV_Clear (DefaultEnvVars);
    EV_LoadFromIniFile (DefaultEnvVars, telnetIniFile (), NULL);
    EV_LoadFromIniFile (DefaultEnvVars, fullpath, NULL);

    len= strlen (fullpath);
    tsfileName= realloc (tsfileName, len+1);
    strcpy (tsfileName, fullpath);

    if (bfullname.ptr) free (bfullname.ptr);
    return TRUE;
}

/* Save current session's parameter */
static void fileSaveSession(const Connect *conn, char* filename)
/* filename is the full path */
{
   const char *p;
   char tmpbuff [16];

   WritePrivateProfileString(SECTION, "Host", conn->host, filename);
   WritePrivateProfileString(SECTION, "Port", conn->port, filename);
   WritePrivateProfileString(SECTION, "Protocol", conn->protocol, filename);
   WritePrivateProfileString(SECTION, "Term", conn->term, filename);
   WritePrivateProfileString(SECTION, "User", conn->user, filename);
   WritePrivateProfileString(SECTION, "Charset", conn->charset, filename);
   WritePrivateProfileBoolean(filename, SECTION, "BS2Del",    conn->bs2Del);
   WritePrivateProfileBoolean(filename, SECTION, "LocalExec", conn->locexec);
   WritePrivateProfileBoolean(filename, SECTION, "VtKeyMap",  conn->vtkeymap);

   p= connectCodeKpamValue (conn, tmpbuff);
   if (!p) p= "";
   WritePrivateProfileString(SECTION, "Kpam", p, filename);
}

/* Associate our file type with dtelnet */
static void fileTypeRegister(void)
{
    HKEY hkDtelnet;
    char exePath[_MAX_PATH];
    char value[_MAX_PATH];
    char extension [1+MAXEXTLEN+1];

    GetModuleFileName(telnetGetInstance(), exePath, _MAX_PATH);

    if (RegCreateKey(HKEY_CLASSES_ROOT, TYPE_REG, &hkDtelnet)
        == ERROR_SUCCESS)
    {
	long rc;

#ifdef WIN32
        /* Win32-specific keys, we can set description and icons. */
        RegSetValue(HKEY_CLASSES_ROOT, TYPE_REG, REG_SZ, "DTelnet Shortcut", 16);
        strcpy(value, exePath);
	strlcat(value, ",1", _MAX_PATH);
	RegSetValue(hkDtelnet, "DefaultIcon", REG_SZ, value, strlen(value));
#endif

#ifdef WIN32
	/* Insert double-quote marks to handle names like "Program Files" */
	if (strlen(exePath)+2 < _MAX_PATH)
	{
	    sprintf(value, "\"%s\"", exePath);
	}
#else
        strcpy(value, exePath);
#endif
	strlcat(value, " /X \"%1\"", _MAX_PATH);
	RegSetValue(hkDtelnet, "shell\\open\\command", REG_SZ, value, strlen(value));
	RegCloseKey(hkDtelnet);

	/* Link the extension to the file type */
	sprintf (extension, ".%s", extdata.str[0]);
	rc= RegSetValue(HKEY_CLASSES_ROOT, extension, REG_SZ, TYPE_REG, TYPE_REG_LEN);
	if (rc) {
	    MessageBoxF (NULL, "RegSetValue", MB_OK,
		"RegSetValue(\"%s\") returned %d", extension, rc);
	}

#ifdef WIN32
	/* Notify the system that it's been updated */
        SendMessage(HWND_BROADCAST, WM_SETTINGCHANGE, 0, 0);
#endif
    }
}

#if 0
/* Compare filename to our extension */
static BOOL fileCheckType(const char* filename)
{
    const char* extpart;
    BOOL retval;

    /* pointer arithmetic, should take us to right before extension part */
    extpart= filename + (strlen(filename)-EXTLEN);
    retval= stricmp(extpart, FILETYPE)==0;
    return retval;
}
#endif

/* enable delete/modify buttons if the listbox have selection
 */
static void fileManageEnableCtls(void)
{
    BOOL bEnable;
    bEnable = (SendDlgItemMessage(manageDlg, ID_FAV_LISTBOX, LB_GETCURSEL, 0, 0) != LB_ERR);
    EnableWindow(GetDlgItem(manageDlg, ID_FAV_DEL), bEnable);
    EnableWindow(GetDlgItem(manageDlg, ID_FAV_MOD), bEnable);
    if (!bEnable) {
	SendDlgItemMessage(manageDlg, ID_FAV_INFO, WM_SETTEXT, 0, (LPARAM)"No selection");
    }
}

void fileExecFavorites(HMENU favmenu, WPARAM item)
{
    Buffer pathb= EmptyBuffer;
    char *cmd= NULL;
    int indx;

    if (item-ID_FAVORITES_FIRST >= favorites.n) {	/* ?! */
	goto RETURN;
    }
    indx= item - ID_FAVORITES_FIRST;

    if (!buildPathName (&pathb, indx)) goto RETURN;

    if (socketOffline()) {
	/* Not connected, we load the session in current window */
	Connect conn;

	connectGetDefVars (&conn);
	if (fileLoadSessionCore (pathb.ptr, &conn)) {
	    connectLoadVars(&conn, OptFromTsFile);
	    connectOpenHost();
	}

    } else {
	telnetSaveProfile ();
	telnetClone ((ConstBuffData *)&pathb, NULL);
    }

RETURN:
    if (pathb.ptr) free (pathb.ptr);
    if (cmd) free (cmd);
}

static void writeFilters (char *filters)
{
    unsigned i;
    char *q= filters;

    for (i=0;i<extdata.n; ++i) {
	if (i==0) q += 1 + sprintf (q, "DTelnet Shortcuts");
	else      q += 1 + sprintf (q, "Old DTelnet Shortcuts");
	q += 1 + sprintf (q, "*.%s", extdata.str[i]);
    }
    *q++ = '\0';
}

/* Dialog procedure for the Connect dialog
 */
static DIALOGRET CALLBACK fileEditDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    	/* Flag that we have created the dialog and register the
         * dialog window handle.
    	 */
    	haveEditDlg = TRUE;
	dialogRegister(dlg);
    	/* Initialise the dialog controls
    	 */
	if (*dlgTitle) {
	    SendMessage(dlg, WM_SETTEXT, 0, (LPARAM)dlgTitle);
	}
	connectDlgSetVars(dlg, &editVars);
	return TRUE;

    case WM_COMMAND:
    	switch (LOWORD(wparam)) {
	case IDOK:
    	    /* Read all dialog controls into the current session
	     */
	    connectDlgGetVars(dlg, &editVars);
	    if (newFile) {
		const char *dtelnetdir;
	        OPENFILENAME ofn;
		BOOL fHasFileName;
		char filters[256];

		/* Get a new filename */
	    	memset(&ofn, 0, sizeof(OPENFILENAME));
		ofn.lStructSize = sizeof(OPENFILENAME);
		ofn.hwndOwner = dlg;
		writeFilters (filters);
		ofn.lpstrFilter = filters;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = saveName;
		ofn.lpstrFile[0]= '\0';
		ofn.lpstrDefExt = extdata.str[0];
		ofn.nMaxFile = _MAX_PATH;
		dtelnetdir= hasConfDir()? getConfDir(): getExePath();
		ofn.lpstrInitialDir = dtelnetdir;
		ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
		fHasFileName= GetSaveFileName(&ofn);
		if (!fHasFileName) {
		    return TRUE;
		}
	    } else {
		if (MessageBox(dlg, "Do you want to save changes?", "Confirm changes", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) {
		    return TRUE;
		}
	    }
	    /* save to full name */
	    fileSaveSession(&editVars, saveName);

	    /* refresh list */
	    favLoadAll (TRUE);
	    fileLoadFavorites (FILETYPE_LOAD_MENU);

	    if (manageDlg){
		/* parent is Manage Dialog */
		SendMessage(GetParent(dlg), WM_COMMAND, ID_FAV_REFRESH, 0);
	    }
	    /* fall - through  to destroy window */
	case IDCANCEL:
	    /* Destroy the dialog and unregister the dialog handle.
	     */
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    haveEditDlg = FALSE;
	    return TRUE;
    
    	case IDC_PROTOCOL:
#ifdef WIN32
	    if (HIWORD(wparam) != CBN_SELCHANGE)
    		break;
#else
    	    if (HIWORD(lparam) != CBN_SELCHANGE)
		break;
#endif
	    connectDlgOnProtocolChange(dlg);
	    break;
    
    	case IDC_PORT:
#ifdef WIN32
    	    if (HIWORD(wparam) != EN_CHANGE)
		break;
#else
    	    if (HIWORD(lparam) != EN_CHANGE)
		break;
#endif
	    connectDlgOnPortChange(dlg);
    	    break;
    
    	case IDC_HOSTNAME:
#ifdef WIN32
    	    if (HIWORD(wparam) != EN_CHANGE)
    		    break;
#else
    	    if (HIWORD(lparam) != EN_CHANGE)
    		    break;
#endif
	    connectDlgOnHostChange(dlg);
    	    break;
    	}
    }
    return FALSE;
}

void fileFavCreate(HINSTANCE instance, HWND wnd)
{
    if (!haveEditDlg){
	newFile = TRUE;
	strcpy(dlgTitle, "New connection Profile");
	strcpy(saveName, "");
	if (socketConnected()) {
	    connectGetVars(&editVars);
	}
	else {
	    connectGetDefVars(&editVars);
	}
	CreateDialog(instance, MAKEINTRESOURCE(IDD_CONNECT_DIALOG),
		     wnd, fileEditDlgProc);
    }
}

static void fileFavDelete(HWND wnd, int indx)
{
    Buffer delName= EmptyBuffer;
    Buffer prompt=  EmptyBuffer;
    size_t reqlen;

    expandFavPathB (&delName, indx);

    reqlen= 32 + delName.len;
    BuffSecX (&prompt, reqlen);
    prompt.len= sprintf (prompt.ptr, "Delete %.*s?",
	(int)delName.len, delName.ptr);

    if (MessageBox (wnd, prompt.ptr, "Confirm delete",
		    MB_OKCANCEL | MB_ICONQUESTION)==IDOK) {
	unlink (delName.ptr);
	SendMessage (wnd, WM_COMMAND, ID_FAV_REFRESH, 0);
    }

    if (prompt.ptr) xfree (prompt.ptr);
    if (delName.ptr) xfree (delName.ptr);
}

static void fileFavModify(HINSTANCE instance, HWND wnd, int indx)
{
    if (!haveEditDlg){
	const char *name;

	newFile = FALSE;
	strcpy(dlgTitle, "Editing ");
	name= getFavName (indx);
	strlcat(dlgTitle, name, _MAX_PATH);

	expandFavPath(saveName, indx);
	fileLoadSessionByIndex(indx, &editVars);
	CreateDialog(instance, MAKEINTRESOURCE(IDD_CONNECT_DIALOG),
		     wnd, fileEditDlgProc);
    }
}

static DIALOGRET CALLBACK fileManageDlgProc
	(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
    Connect entry;
    int sel;
    char info[256];

    switch (message) {
    case WM_INITDIALOG:
        /* Flag that we have created the dialog, and register the
         * dialog window handle with the dialog handling code.
         */
	manageDlg = dlg;
	dialogRegister(dlg);

	fileLoadFavorites(FILETYPE_LOAD_MANAGE);
	return TRUE;

    case WM_COMMAND:
	switch (LOWORD (wparam)) {
	case ID_FAV_LISTBOX:
#ifdef WIN32
	    switch (HIWORD (wparam))
#else
	    switch (HIWORD (lparam))
#endif
	    {
	    case LBN_SELCHANGE:
		/* Display connection info as string */
		sel = (int)SendDlgItemMessage(dlg, ID_FAV_LISTBOX, LB_GETCURSEL, 0, 0);
		if (sel != LB_ERR) {
		     if (fileLoadSessionByIndex (sel, &entry)) {
			sprintf(info, "Host: %s/%s (%s) %s %s",
			    entry.host, entry.port, entry.protocol, entry.term,
			    entry.charset);
		        SendDlgItemMessage(dlg, ID_FAV_INFO, WM_SETTEXT, 0, (LPARAM)info);
		     } else {
			 SendDlgItemMessage(dlg, ID_FAV_LISTBOX, LB_SETCURSEL, -1, 0);
		     }
                }
		fileManageEnableCtls();
                break;

            case LBN_DBLCLK:
                /* Modify current entry */
		sel = (int)SendDlgItemMessage(dlg, ID_FAV_LISTBOX, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) {
		    fileFavModify(telnetGetInstance(), dlg, sel);
                }
                break;
            }
            break;

        case ID_FAV_DEL:
	    sel = (int)SendDlgItemMessage(dlg, ID_FAV_LISTBOX, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR) {
		fileFavDelete(dlg, sel);
            }
            break;

        case ID_FAV_MOD:
            sel = (int)SendDlgItemMessage(dlg, ID_FAV_LISTBOX, LB_GETCURSEL, 0, 0);
	    if (sel != LB_ERR) {
		fileFavModify(telnetGetInstance(), dlg, sel);
	    }
	    break;

	case ID_FAV_NEW:
	    fileFavCreate(telnetGetInstance(), dlg);
	    break;

	case ID_FAV_REFRESH:
	    favLoadAll (TRUE);
	    fileLoadFavorites(FILETYPE_LOAD_MENU | FILETYPE_LOAD_MANAGE);
	    break;

	case ID_FAV_REGASSOC: {
	    char msg [256];

	    sprintf (msg, "Do you want to associate \"%s\" with DTelnet?",
		extdata.str[0]);
	    if (MessageBox(dlg, msg, "Register Association",
		MB_ICONQUESTION | MB_OKCANCEL) == IDOK) {
		fileTypeRegister();
	    }
	    break; }

	case IDOK:
	    /* Destroy the dialog and unregister the dialog handle
	     * with the dialog handling code.
	     */
	    DestroyWindow(dlg);
	    dialogUnRegister(dlg);
	    manageDlg = NULL;
	    return TRUE;
	}
        break;
    case WM_CLOSE:
        DestroyWindow(dlg);
        dialogUnRegister(dlg);
	manageDlg = NULL;
        return TRUE;
    }
    return FALSE;

}

/* Pop up a manage favorites dialog */
void fileTypeManage(HINSTANCE instance, HWND wnd)
{
    if (!manageDlg)
	CreateDialog(instance, MAKEINTRESOURCE(IDD_MANAGE_FILETYPE),
                     wnd, fileManageDlgProc);
}

FavoriteData favorites = EmptyFavoriteData;

void favClear (void)
{
    size_t i;

    for (i=0; i<favorites.n; ++i) {
	if (favorites.p[i].name) {
	    free (favorites.p[i].name);
	    favorites.p[i].name= NULL;
	}
    }
    favorites.n= 0;
}

static int favSearchCmp (const void *pneedle, const void *pfrec)
{
    int cmp;
    ConstBuffData *needle= (ConstBuffData *)pneedle;
    const FavoriteRec *frec= (FavoriteRec *)pfrec;
    ConstBuffData fname;
    
    fname.ptr= frec->name;
    fname.len= strlen (fname.ptr);

    cmp= BuffDataCmpI (needle, &fname);
    return cmp;
}

static int favSortCmp (const void *rec1, const void *rec2)
{
    int cmp;

    cmp= stricmp (((FavoriteRec *)rec1)->name, ((FavoriteRec *)rec2)->name);
    return cmp;
}

static size_t favSearch (const ConstBuffData *favname, size_t n)
{
    FavoriteRec *found;

    if (n==(size_t)-1) n= favorites.n;
    if (n==0) return (size_t)-1;

    found= bsearch (favname, favorites.p, n, sizeof (FavoriteRec), favSearchCmp);
    if (!found) return (size_t)-1;
    else return (size_t)(((char *)found - (char *)favorites.p))/
	sizeof (FavoriteRec);
}

static void favLoadDirRutin (const char *directory, int dir,
	int extno)
{
    size_t nbefore= favorites.n;
    FavoriteRec *fp;
    ConstBuffData favname;
    const char *ext;
    char dotext[1+MAXEXTLEN+1];	/* ".ts" or ".dtelnet_ses" */
    ConstBuffData dotextb;

#if defined(_WIN32)
    char mask [_MAX_PATH];
    HANDLE hfind;
    WIN32_FIND_DATA wfd;
#else
    DIR *dhandle= NULL;
    struct dirent *item;
#endif

    ext= extdata.str[extno];
    dotextb.ptr= dotext;
    dotextb.len= sprintf (dotext, ".%s", ext);

#if defined(_WIN32)
    sprintf (mask, "%s*.%s", directory, ext);
    hfind = FindFirstFile(mask, &wfd);
    if (hfind == INVALID_HANDLE_VALUE) goto RETURN;
    do {
	favname.ptr= wfd.cFileName;
#else
    dhandle= opendir (directory);
    if (!dhandle) goto RETURN;
    while ((item= readdir (dhandle))!=NULL) {
	favname.ptr= item->d_name;
#endif
	favname.len= strlen (favname.ptr);

	/* check/cut extension */
	if (favname.len > dotextb.len &&
	    BuffDataEndI (&favname, &dotextb)) {
	    favname.len -= dotextb.len;
	} else {
	    continue;
	}

	/* check duplicate */
	if (nbefore && favSearch (&favname, nbefore)!=(size_t)-1) continue;

	if (favorites.n==favorites.nmax) {
	    if (favorites.nmax==0) favorites.nmax= 16;
	    else favorites.nmax *= 2;
	    favorites.p= xrealloc (favorites.p, favorites.nmax * sizeof (FavoriteRec));
	}
	fp= &favorites.p[favorites.n++];
	fp->dir= dir;
	fp->name= xmalloc (favname.len+1);
	memcpy (fp->name, favname.ptr, favname.len);
	fp->name[favname.len]= '\0';
	fp->extno= extno;

#if defined(_WIN32)
    } while (FindNextFile (hfind, &wfd));
    FindClose(hfind);
#else
    }
    closedir (dhandle);
#endif
    if (favorites.n>1) {
	qsort (favorites.p, favorites.n, sizeof (FavoriteRec), favSortCmp);
    }
RETURN:
    return;
}

static void favLoadDirCore (const char *directory, int dir)
{
    unsigned i;

    for (i= 0; i<extdata.n; ++i) {
	favLoadDirRutin (directory, dir, i);
    }
}

void favLoadDir (int dir)
{
    const char *directory= NULL;

    if (dir==FAVDIR_CONF) {
	if (hasConfDir()) directory= getConfDir ();

    } else if (dir==FAVDIR_EXE) {
        directory= getExePath();
    }

    if (directory) {
	favLoadDirCore (directory, dir);
    }
}

void favLoadAll (BOOL force) 	/* force==TRUE: even if loaded==FALSE (calls favClear) */
{
    if (favorites.loaded && !force) return;
    favClear ();
    favLoadDir (FAVDIR_CONF);
    favLoadDir (FAVDIR_EXE);
    favorites.loaded= TRUE;
}

/*
 * flag FILETYPE_LOAD_MENU update favorites menu
 * flag FILETYPE_LOAD_MANAGE update manage favorites listbox
 */
void fileLoadFavorites (UINT flags)
{
    int item;
    HMENU favmenu;
    HWND listbox;
    size_t i;
				    
    favLoadAll (FALSE);

    if (flags & FILETYPE_LOAD_MENU) {
	item = ID_FAVORITES_FIRST;
	favmenu = GetSubMenu(GetMenu(telnetGetWnd()),2);

    /* Clear the menu fisrt */
	while (GetMenuItemCount(favmenu) > 3){
	    DeleteMenu(favmenu, 3, MF_BYPOSITION);
	}
    }
    if (flags & FILETYPE_LOAD_MANAGE) {
	listbox = GetDlgItem(manageDlg, ID_FAV_LISTBOX);

	/* Clear the listbox */
	SendMessage(listbox, LB_RESETCONTENT, 0, 0);
	fileManageEnableCtls();
    }

    for (i=0; i<favorites.n; ++i) {
	if (flags & FILETYPE_LOAD_MENU) {
	    AppendMenu(favmenu, MF_STRING, item, favorites.p[i].name);
	    item++;
	}
	if (flags & FILETYPE_LOAD_MANAGE) {
	    SendMessage(listbox, LB_ADDSTRING, 0, (LPARAM)favorites.p[i].name);
	}
    }

    if (flags & FILETYPE_LOAD_MENU) {
	DrawMenuBar(telnetGetWnd());
    }
}
