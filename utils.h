/* utils.h
 * Copyright (c) 1997 David Cole
 *
 * Miscellaneous utilities
 */
#ifndef __utils_h
#define __utils_h

#include "preset.h"

#include <stddef.h>
#include <windows.h>

#include "buffdata.h"

#define USE_ASSERT

/* Convert a character to ^character */
#define CTRL(c) ((c) & 0x1f)

/* Determine the number of elements in an array */
#define numElem(a) (sizeof(a)/sizeof(a[0]))

/* Concatenate strings, return combined length */
size_t strlcat(char* dst, const char* src, size_t siz);
/* Copy a string, return copied length */
size_t strlcpy(char* dst, const char* src, size_t siz);
/* Separate a string into tokens, like strtok */
char* strsep(char** stringp, const char* delim);

/* stpcpy: len=strlen(from) + memcpy(dest,from,len+1) + return dest+len */
char *stpcpy(char *dest, const char *from);

size_t strnlen (const char *s, size_t maxlen);

/* find the last occurance */
void *memrchr(const void *s, int c, size_t n);

/* Allocate some memory - abort program on failure */
void* xmalloc(int size);
/* Resize some previously allocated memory - abort program on failure */
void* xrealloc(void* ptr, int size);
/* Frees memory allocated using xmalloc or xrealloc. */
void xfree(void* ptr);
/* Allocates memory using xmalloc, and copies the string into that memory. */
char* xstrdup(const char *str);

extern HINSTANCE instanceHnd;	/* application instance handle */
/* WinMain / main should fill this!!! */

/* fileInDir: combine dirname and filename
   result should be 'xfree'-ed
 */
char *fileInDir (const char *dirname, const char *filename);

/* getExeName: returns C:\SOME\PATH\DTELNET.EXE */
const char *getExeName(void);
/* getExePath: returns C:\SOME\PATH\ (if not empty, the last character is '\') */
const char *getExePath(void);

/* get %HOME%\dtelnet (or %USERPROFILE%\dtelnet) directory (if any) */
BOOL hasConfDir (void);
const char *getConfDir(void);	/* returns empty string ("") if there's no such directory */

/* get help-file path (in the directory of the executable */
const char *getHelpName (void);

/* makeIniFileName example:
   char buff [_MAX_PATH];
   makeIniFileName (buff, "dtelnet.set") */
void makeIniFileName (char *into, const char *fname);

/* telnetClone:
   Exec another instance, with or without a session-filename (option /X),
   and other opts (from connectCmdLine, for example)
   Most likely, you shold call telnetSaveProfile beforehand,
   to store the actual state into the ini-file
 */
void telnetClone (const ConstBuffData *sesname, const ConstBuffData *otheropts);

/* Split a string into a number of fields using delimiter */
int split(char* str, char delim, char** fields, int maxFields);

/* Check if a string is non-blank */
BOOL isNonBlank(char* str);

#ifdef USE_ASSERT
void assertFail(char* str, char* file, int line);
#define ASSERT(exp) { if (!(exp)) assertFail(#exp, __FILE__, __LINE__); }
#else
#define ASSERT(exp)
#endif

#ifdef USE_ODS
void ods(char* fmt, ...);
#endif

extern int  fromhex(char hex);
extern int  escape (const char *from, int len, char* to, int maxlen);
extern int  unescape (const char* from, int len, char* to, int maxlen);

extern int  bescape (int opt, const ConstBuffData *from, Buffer *into, BuffData *rest);
/* opt: set to zero
   from: input
   into: output (don't forget to set into->len to 0!)
   rest: the part that couldn't fit
*/

extern int  bunescape (int opt, const ConstBuffData *from, Buffer *into, BuffData *rest);
/* opt= 0: default
   opt= 1: stop at unescaped 'single' quote
   opt= 2: stop at unescaped "double" quote
   opt= 3: stop at either unescaped 'single' or unescaped "double" quote
   opt= 4: output is a DynBuffer -- it can resized
   from: input
   into: output (don't forget to set into->len to 0!)
   rest: the part that couldn't fit / the part after the error
   return:
	 0: all converted
	-1: conversion stopped: couldn't fit (see rest)
	 1: unescaped 'single' quote reached (see rest)
	 2: unescaped "double" quote reached (see rest)
*/

BOOL WritePrivateProfileBoolean (const char *inifilename, const char *section, const char *key, BOOL val);
BOOL GetPrivateProfileBoolean (const char *inifilename, const char *section, const char *key, BOOL defval);

#ifndef WIN32
/* MyGetPrivateProfileSection: not in Win16 */
size_t GetPrivateProfileSection
	(const char *sectionname, char *buff, size_t sectmaxsize, const char *filename);

BOOL CopyFile (const char *from, const char *to, BOOL fFailIfExist);
#endif

/* where an option came from (in the order of precedence) */
/* 20140328.LZS: Migrated here from term.h */
typedef enum OptSource {
    OptUnset= -1,	/* unset */
    OptFromDefault,	/* lowest priority */
    OptFromIniFile,
    OptFromTsFile,
    OptFromCmdLine,
    OptFromDialog	/* highest priority */
} OptSource;

int MessageBoxF (HWND hWnd, const char *caption, unsigned type, const char *fmt, ...);

#endif
