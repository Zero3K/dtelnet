/* utils.c
 * Copyright (c) 1997 David Cole
 *
 * Miscellaneous utilities
 */
#include "preset.h"

#include <windows.h>
#include <ctype.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>

#include "utils.h"
#include "dtelnet.h"
#include "term.h"
#include "log.h"

size_t strlcat(char* dst, const char* src, size_t size)
{
    size_t origlen= strlen (dst);

    if (origlen<size) {
	strncat(dst, src, size-origlen);
    }
    return strlen(dst);
}

size_t strlcpy(char* dst, const char* src, size_t size)
{
    strncpy(dst, src, size);
    return strlen(dst);
}

char *stpcpy(char *dest, const char *from)
{
    size_t len;

    len= strlen(from);
    memcpy(dest, from, len+1);
    return dest+len;
}

size_t strnlen (const char *s, size_t maxlen)
{
    const char *p, *plim;

    if (!s || maxlen<1) return 0;

    p= s-1;
    plim= s+maxlen;
    while (++p<plim && *p);

    return (size_t)(p-s);
}

/* find the last occurance */
void *memrchr(const void *s, int c, size_t n)
{
    const char *b;
    const char *e;

    if (n==0) return NULL;
    b= s;
    e= b+n;

    while (--e >= b) {
	if (*e==(char)c) return (void *)e;
    }
    return NULL;
}

/*
 * Get next token from string *stringp, where tokens are possibly-empty
 * strings separated by characters from delim.  
 *
 * Writes NULs into the string at *stringp to end tokens.
 * delim need not remain constant from call to call.
 * On return, *stringp points past the last NUL written (if there might
 * be further tokens), or is NULL (if there are definitely no more tokens).
 *
 * If *stringp is NULL, strsep returns NULL.
 */
char* strsep(char** stringp, const char* delim)
{
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;

    if ((s = *stringp) == NULL)
        return (NULL);
    for (tok = s;;) {
        c = *s++;
        spanp = delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *stringp = s;
                return (tok);
            }
        } while (sc != 0);
    }
	/* NOTREACHED */
}

/* Allocate some memory - abort program on failure
 *
 * Args:
 * size - the amount of memory to allocate
 */
void* xmalloc(int size)
{
    void* ret = malloc(size);
    if (ret == NULL) {
	char msg[64];
	sprintf (msg, "malloc(%d) failed\n", size);

	MessageBox(termGetWnd(), msg, telnetAppName(),
		   MB_APPLMODAL | MB_ICONHAND | MB_OK);
	if (size!=0) telnetExit();
    }
    return ret;
}

/* Resize some previously allocated memory - abort program on failure
 *
 * Args:
 * ptr -  point to memory previously allocated
 * size - the amount of memory to allocate
 */
void* xrealloc(void* ptr, int size)
{
    void* ret = realloc(ptr, size);
    if (ret == NULL && size != 0) {
	MessageBox(termGetWnd(), "Out of memory", telnetAppName(),
		   MB_APPLMODAL | MB_ICONHAND | MB_OK);
	telnetExit();
    }
    return ret;
}

/* Free some previously allocated memory - abort program on failure
 *
 * Args:
 * ptr -  point to memory previously allocated
 */
void xfree(void* ptr)
{
    if (ptr == NULL) {
        MessageBox(termGetWnd(), "xfree: NULL pointer", telnetAppName(),
		   MB_APPLMODAL | MB_ICONHAND | MB_OK);
	        telnetExit();
    }
    free(ptr);
}

/* Copy a string using xmalloc - abort program on failure
 *
 * Args:
 * str -  point to string to be copied
 */
char* xstrdup(const char *str)
{
    int len = strlen(str) + 1;
    char* cp;

    if (len == 0) {
        MessageBox(termGetWnd(), "xstrdup: NULL pointer", telnetAppName(),
            MB_APPLMODAL | MB_ICONHAND | MB_OK);
        telnetExit();
    }

    cp = xmalloc(len);
    strlcpy(cp, str, len);
    return cp;
}

HINSTANCE instanceHnd= NULL;

static char *exeName= NULL;
static char *exePath= NULL;
static char *confDir= NULL;
static char *hlpName= NULL;

const char *getHelpName (void)
{
#if defined(_WIN32)
    static const char helpfilename[] = "dtelnet.chm";
#else
    static const char helpfilename[] = "dtelnet.hlp";
#endif
    if (!hlpName) {
        size_t plen, len;
	char *p;
	int rc;

	if (!exePath) getExePath ();
	plen= strlen (exePath);
	if (plen>0 && exePath[plen-1]=='\\') --plen;
	len= plen + 1 + sizeof (helpfilename);
	p= xmalloc (len+1);
	sprintf (p, "%.*s\\%s", (int)plen, exePath, helpfilename);
	rc= access (p, 0);
	if (rc) {
	    xfree (p);
	    hlpName= "";
	    goto RETURN;
	}
	hlpName= p;
    }
RETURN:
    return hlpName;
}

const char *getExePath (void)
{
    if (!exePath) {
	const char *p;
	size_t len;

	if (!exeName) getExeName();
	p= exeName + strlen (exeName);
	while (--p >= exeName && *p!='\\'&& *p!=':');
	len= (size_t)(p-exeName+1);
	exePath= xmalloc (len+1);
	memcpy (exePath, exeName, len);
	exePath[len]= '\0';
    }
    return exePath;
}

const char *getExeName(void)
{
    if (!exeName) {
	char buff [_MAX_PATH+1];
	size_t len;

	len = GetModuleFileName (instanceHnd, buff, _MAX_PATH);
	exeName= xmalloc (len+1);
	memcpy (exeName, buff, len);
	exeName[len]= '\0';
    }
    return exeName;
}

BOOL hasConfDir (void)
{
    BOOL retval;

    if (!confDir) {
	getConfDir ();
    }
    retval= *confDir!='\0';
    return retval;
}

const char *getConfDir(void)
{
    if (!confDir) {
	const char *envp;
	int rc;
	char *dirp= NULL;
	size_t len;

	envp= getenv ("HOME");
	if (!envp || !*envp) {
	    envp= getenv ("USERPROFILE");
	}
	if (!envp || !*envp) {
	    goto HOMELESS;
	}

	rc= access (envp, 0);
	if (rc) goto HOMELESS;

	len= strlen (envp);
	if (envp[len-1]=='\\') --len;
	dirp= xmalloc (len+1+13+1);
	len= sprintf (dirp, "%.*s\\Dtelnet", (int)len, envp);
	
	rc= access (dirp, 0);
	if (rc) {
	    mkdir (dirp);
	    rc= access (dirp, 0);
	}
	if (rc) goto HOMELESS;

	dirp[len]= '\\';
	dirp[len+1]= '\0';
	confDir= dirp;
	goto RETURN;

HOMELESS:
	if (dirp) xfree (dirp);
	confDir= "";
	goto RETURN;
    }

RETURN:
    return confDir;
}

void makeIniFileName (char *into, const char *fname)
{
    char tmp1[_MAX_PATH], tmp2[_MAX_PATH];
    const char *p1= NULL, *p2= NULL, *retp= NULL;
    int acc1= -1, acc2= -1;

    if (hasConfDir()) {
	p1= tmp1;
	sprintf (tmp1, "%s%s", getConfDir(), fname);
	acc1= access (p1, 0);
    }

    if (acc1!=0) {
	p2= tmp2;
	sprintf (tmp2, "%s%s", getExePath(), fname);
	acc2= access (p2, 0);
    }
    if (acc1==0 ||
	(acc1!=0 && acc2!=0 && p1!=NULL)) {
	retp= p1;

    } else if (p2) {
	retp= p2;
    }

    if (retp) strcpy (into, retp);
    else      strcpy (into, fname);
}

char *fileInDir (const char *dirname, const char *filename)
{
    size_t dlen= 0;
    size_t flen= 0;
    char *pathname= NULL;
    char *p;

    if (dirname) {
	dlen= strlen (dirname);
	if (dlen && dirname[dlen-1] == '\\') --dlen;
    }
    if (filename) {
	flen= strlen (filename);
    }

    pathname= xmalloc (dlen + 1 + flen + 1);
    p= pathname;

    if (dlen) {
	memcpy (p, dirname, dlen);
	p += dlen;
    }
    if (dlen && flen) {
	*p++ = '\\';
    }
    if (flen) {
	memcpy (p, filename, flen);
	p += flen;
    }
    if (p==pathname) {		/* empty -- this shouldn't happen */
	*p++= '.';
    }
    *p= '\0';

    return pathname;
}

void telnetClone (const ConstBuffData *sesname, const ConstBuffData *otheropts)
{
    const char *exename, *ininame;
    size_t reqlen;
    char *cmd, *p;
    DWORD rc;

    exename= getExeName ();
    ininame= telnetIniFile ();

    reqlen= strlen (exename) + strlen (ininame) + 16;

    if (sesname   && sesname->len)   reqlen += 6 + sesname->len;
    if (otheropts && otheropts->len) reqlen += 1 + otheropts->len;

    cmd= xmalloc (reqlen);
    p= cmd;

    p += sprintf (p, "\"%s\" /I'%s'", exename, ininame);

    if (sesname && sesname->len) {
	p += sprintf (p, " /X '%.*s'", (int)sesname->len, sesname->ptr);
    }

    if (otheropts && otheropts->len) {
	p += sprintf (p, " %.*s", (int)otheropts->len, otheropts->ptr);
    }

/*  MessageBox (NULL, cmd, "WinExec", MB_OK); -- debug */

    rc= WinExec (cmd, SW_SHOW);
    if (rc<32) {
	MessageBoxF (NULL, "WinExec error", MB_OK,
	    "WinExec returned %d\n"
	    "cmd=%s", (int)rc, cmd);
    }
    xfree (cmd);
}

/* Split a string into a number of fields using delimiter
 *
 * Args:
 * str -       string to be split in place
 * delim -     delimiter to split fields by
 * fields -    array to receive pointers to fields
 * maxFields - number of fields that will fit in array
 */
int split(char* str, char delim, char** fields, int maxFields)
{
    int num;			/* iterate over fields */

    for (num = 0; num < maxFields;) {
	fields[num++] = str;
	str = strchr(str, delim);
	if (str == NULL)
	    break;
	*str++ = '\0';
    }

    return num;
}

#ifdef USE_ASSERT
void assertFail(char* str, char* file, int line)
{
    char where[256];

    logStop();
    sprintf(where, "In %s, line %d", file, line);
    MessageBox(termGetWnd(), str, where, MB_APPLMODAL | MB_ICONHAND | MB_OK);
}
#endif

#ifdef USE_ODS
void ods(char* fmt, ...)
{
    char msg[512];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    OutputDebugString(msg);
}
#endif

BOOL isNonBlank(char* str)
{
    while (*str != '\0')
	if (isalnum(*str++))
	    return TRUE;
    return FALSE;
}

/* Utility functions for dealing with escape codes
 *
 * copied from termdef.c of Mark Melvin
 */

int escape (const char *from, int len, char* to, int maxlen)
{
    Buffer bInto;
    ConstBuffData bFrom;

    if (to && maxlen) {
	bInto.ptr= to;
	bInto.maxlen= maxlen;
    } else {
	return 0;
    }
    bInto.len= 0;

    if (from) {
	bFrom.ptr= from;
	bFrom.len= len;
    } else {
	bFrom.ptr= "";
	bFrom.len= 0;
    }

    bescape (0, &bFrom, &bInto, NULL);

    return (int)bInto.len;
}

#define StoreTwo(c1,c2)		\
    {				\
	if (q+1 >= qlim) {	\
	    leave= -1;		\
	} else {		\
	    *q++= (c1);		\
	    *q++= (c2);		\
	}			\
    }
#define StoreFour(c1,c2,c3,c4)	\
    {				\
	if (q+3 >= qlim) {	\
	    leave= -1;		\
	} else {		\
	    *q++= (c1);		\
	    *q++= (c2);		\
	    *q++= (c3);		\
	    *q++= (c4);		\
	}			\
    }

int bescape (int opt, const ConstBuffData *from, Buffer *into, BuffData *rest)
{
    char tohex[16] = "0123456789ABCDEF";
    const char *p, *plim;
    char *q, *qlim;
    int c;
    int leave;
    int rc;

    p= from->ptr;
    plim= p + from->len;
    q= into->ptr + into->len;
    qlim= into->ptr + into->maxlen;

    for (--p, leave= 0; !leave && ++p<plim && q<qlim;) {
	c = *(unsigned char *)p;
	if      (c=='"')    StoreTwo ('\\', '"')
	else if (c=='\r')   StoreTwo ('\\', 'r')
	else if (c=='\n')   StoreTwo ('\\', 'n')
	else if (c=='\x1b') StoreTwo ('\\', 'e')
	else if ((c >= 0x20 && c < 0x7F) ||
		   (c >  0xA0 && c < 0xFF)) {
	    *q++ = (char)c;
	} else 		    StoreFour ('\\', 'x', tohex[(c & 0xF0)>>4], tohex[c & 0x0F]);
    }
    if (q<qlim)*q = '\0';   /* null-terminate result */
    into->len= (size_t)(q - into->ptr);

    if (leave) rc= leave;
    else if (q>=qlim) rc= -1;	/* too long */
    else rc= 0;

    if (rest) {
	rest->ptr= (char *)p;
	rest->len= (size_t)(plim-p);
    }

    return rc;
}

int fromhex (char hex)
{
   if ((hex>='0') && (hex<='9'))     return hex-'0';
   else if((hex>='A') && (hex<='F')) return hex-'A'+10;
   else if((hex>='a') && (hex<='f')) return hex-'a'+10;
   else
   return 0;
}

int unescape (const char* from, int len, char* to, int maxlen)
{
    ConstBuffData bf;
    Buffer bt;

    bf.ptr= from;
    bf.len= len;
    bt.ptr= to;
    bt.len= 0;
    bt.maxlen= maxlen;

    bunescape (0, &bf, &bt, NULL);
    return bt.len;
}

int bunescape (int opt, const ConstBuffData *from, Buffer *into, BuffData *rest)
{
    const char *p, *plim;
    char *q, *qlim;
    BOOL in_slash;
    int in_hex, code;
    int leave, rc;

    p = from->ptr;
    plim= p + from->len;
    q = into->ptr + into->len;
    qlim= into->ptr + into->maxlen;
    in_slash = FALSE;
    in_hex = 0;
    code   = 0;

    for (--p, leave= 0; !leave && ++p<plim && q<=qlim;) {
	if (q==qlim) { /* 'into' is full. Alloc mode? */
	    if (opt&4) {
		BuffSecure (into, 1);
		q=    into->ptr + into->len;
		qlim= into->ptr + into->maxlen;
	    } else {
		leave= -1;
		continue;
	    }
	}

	if (in_hex==2) {
	    code = 16*fromhex (*p);
	    in_hex = 1;

	} else if (in_hex==1) {
	    code += fromhex (*p);
	    *q++ = (char)code;
	    in_hex = 0;

	} else if (!in_slash) {
	    if (*p=='\\')                 in_slash = TRUE;
	    else if (*p=='\'' && (opt&1)) leave= 1;
	    else if (*p=='\"' && (opt&2)) leave= 2;
	    else 			  *q++ = *p;

	} else { /* in_slash */
	    switch (*p){
	    case 'n':  *q++ = '\n'; break;
	    case 'r':  *q++ = '\r'; break;
	    case '\\': *q++ = '\\'; break;
	    case 't':  *q++ = '\t'; break;
	    case 'b':  *q++ = '\b'; break;
	    case 'e':  *q++ = '\x1b'; break;
	    case '\'': *q++ = '\''; break;
	    case '\"': *q++ = '\"'; break;
	    case '0':  *q++ = '\0'; break;
	    case 'x':  in_hex = 2;  break;
	    default:   *q++ = *p;
	    }
	    in_slash = FALSE;
	}
    }
    if (leave) {
	rc= leave;	/* 1/2/-1 = single/double quote reached/'into' is full */
    } else if (q>=qlim) {
	rc= -1;		/* -1: couldn't fit */
    } else {
	rc= 0;		/* cool! */
    }

    if (q<qlim) *q='\0';
    into->len = (size_t)(q - into->ptr);
    if (rest) {
	rest->ptr= (char *)p;
	rest->len= (size_t)(plim - p);
    }

    return rc;
}

BOOL WritePrivateProfileBoolean (const char *inifilename, const char *section, const char *key, BOOL val)
{
    BOOL success;

    success= WritePrivateProfileString (section, key, val? "TRUE": "FALSE", inifilename);

    return success;
}

BOOL GetPrivateProfileBoolean (const char *inifilename, const char *section, const char *key, BOOL defval)
{
    char buff [32];
const char NOTSET[]= "*NOTSET";
    BOOL retval;

    GetPrivateProfileString (section, key, NOTSET, buff, sizeof (buff), inifilename);
    if (strcmp (buff, NOTSET)==0) {
	retval= defval;
    } else {
	retval= strcmp (buff, "TRUE")==0 || strcmp (buff, "1")==0;
    }
    return retval;
}

#ifndef WIN32
/* MyGetPrivateProfileSection: not in Win16 */
size_t GetPrivateProfileSection
	(const char far *sectionname, char far *buff, size_t sectmaxsize, const char far *filename)
{
    FILE *f= NULL;
    char linebuff [2048];
    char *p= buff;
    const char *plim= buff + sectmaxsize-1;
    int fcopy= 0, fleave= 0, ffull= 0;
    ConstBuffData bd, bsect;

    if (sectmaxsize<=2) {
	memset (buff, 0, sectmaxsize);
	return 0;
    }

    bsect.ptr= sectionname;
    bsect.len= strlen (bsect.ptr);

    f= fopen (filename, "rb");
    if (!f) {
	buff[0]= '\0';
	buff[1]= '\0';
	return 0;
    }

    while (!fleave && !ffull && fgets (linebuff, sizeof (linebuff), f)) {
printf ("Debug %s",linebuff);
	bd.ptr= linebuff;
	bd.len= strlen (bd.ptr);
	if (bd.len>0 && bd.ptr[bd.len-1]=='\n') {
	    --bd.len;
	    if (bd.len>0 && bd.ptr[bd.len-1]=='\r') {
		--bd.len;
	    }
	}
	while (bd.len>0 && isspace (*bd.ptr)) {
	    ++bd.ptr;
	    --bd.len;
	}
	while (bd.len>0 && isspace (bd.ptr[bd.len-1])) {
	    --bd.len;
	}
printf ("Debug %.*s\n", (int)bd.len, bd.ptr);
	if (bd.len==0 || bd.ptr[0]==';') continue;
	if (bd.ptr[0]=='[') {
	    --bd.len;
	    ++bd.ptr;
	    if (bd.len>0 && bd.ptr[bd.len-1]==']') {
		--bd.len;
	    }
	    if (BuffDataEqI (&bd, &bsect)) {
		fcopy= 1;
	    } else if (fcopy) {
		fleave= 1;
	    }
	    continue;
	}
	if (fcopy) {
	    size_t cpylen;

	    if (p == plim-1) *p++= '\0';
	    if (p >= plim) {
		ffull= 1;
		continue;
	    }
	    cpylen= bd.len;
	    if (cpylen > (size_t)(plim-p-1)) {
		cpylen= (size_t)(plim-p-1);
		ffull= 1;
	    }
	    memcpy (p, bd.ptr, cpylen);
	    p += cpylen;
	    *p++ = '\0';
	}
    }

RETURN:
    if (ffull) *p-- = '\0';
    else       *p++ = '\0';
    if (!f) fclose (f);
    return (size_t)(p - buff);
}

BOOL CopyFile (const char *from, const char *to, BOOL fFailIfExist)
{
    FILE *fin=  NULL;
    FILE *fout= NULL;
    int   hout= -1;
    BOOL fOk= FALSE;
    int omode;
    int c;

    fin= fopen (from, "rb");
    if (!fin) goto RETURN;

    omode= O_WRONLY | O_BINARY | O_CREAT;
    if (fFailIfExist) omode |= O_EXCL;
    hout= open (to, omode, S_IREAD | S_IWRITE);
    if (hout==EOF) goto RETURN;
    fout= fdopen (hout, "wb");
    if (!fout) goto RETURN;
    hout= EOF;

    while ((c= fgetc (fin)) != EOF)
	fputc (c, fout);
    fOk= TRUE;

RETURN:
    if (fin)       fclose (fin);
    if (hout!=EOF) close  (hout);
    if (fout)      fclose (fout);
    return fOk;
}
#endif

static int MessageBoxV (HWND hWnd, const char *caption, unsigned type, const char *fmt, va_list ap)
{
    char tmp [512];

    vsprintf (tmp, fmt, ap);
    return MessageBox (hWnd, tmp, caption, type);
}

int MessageBoxF (HWND hWnd, const char *caption, unsigned type, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start (ap, fmt);
    rc= MessageBoxV (hWnd, caption, type, fmt, ap);
    va_end (ap);

    return rc;
}
