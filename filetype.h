/*
filetype.h -- Handling connection files


*/

#ifndef __filetype_h
#define __filetype_h

#include "connect.h"

/* Load a session file into connection structure */
BOOL fileLoadSessionByName (const char* filename, Connect* conn);

/* getTsfileName: returns the full name of the last loaded, if any */
const char *getTsfileName (void);

#define FILETYPE_LOAD_MENU	0x01
#define FILETYPE_LOAD_MANAGE	0x02

typedef struct FavoriteRec {
    int dir;	/* FAVDIR_*** */
    char *name;	/* malloced! Without extension! */
    int extno;	/* always 0 for Win16; 0/1 for Win32+ */
} FavoriteRec;

#define FAVDIR_CONF	0
#define FAVDIR_EXE	1

typedef struct FavoriteData {
    BOOL loaded;
    size_t n;
    size_t nmax;
    FavoriteRec *p;
} FavoriteData;

#define EmptyFavoriteData {FALSE, 0, 0, NULL}

extern FavoriteData favorites;

void favClear (void);
void favLoadDir (int dir);	/* FAVDIR_??? */
void favLoadAll (BOOL force); 	/* force==TRUE: even if loaded==FALSE (calls favClear) */

/* Scan dtelnet directory for files and create favorites menu items */
void fileLoadFavorites(UINT flags);
/* Execute the session file specified by menu item */
void fileExecFavorites(HMENU favmenu, WPARAM item);

/* Manage session files in directory */
void fileTypeManage(HINSTANCE instance, HWND wnd);
/* Create a session file in directory */
void fileFavCreate(HINSTANCE instance, HWND wnd);

#endif
