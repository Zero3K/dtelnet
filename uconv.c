/* uconv.c */

#include "preset.h"
#include <windows.h>

#include "platform.h"
#include "ctab.h"
#include "emul.h"
#include "graphmap.h"

#include "uconv.h"

#define DefaultUnicode 0x2592	 /* unicode medium shade */
#define DefaultAscii   '?'

static const uint32_t SP1_min= 0xd800u;
static const uint32_t SP1_max= 0xdbffu;
static const uint32_t SP2_min= 0xdc00u;
static const uint32_t SP2_max= 0xdfffu;
static const uint32_t SMP_min= 0x010000u;
static const uint32_t SMP_max= 0x10ffffu;

/* returns 0/1/2: invalid / BMP / supplemental */
size_t uconvUnicodeToUtf16 (uint16_t *toptr, uint32_t charCode)
{
    uint16_t *q= toptr;

    if ((charCode<=SP1_min) ||
	(charCode> SP2_max && charCode<SMP_min)) {
	*q++ = (uint16_t)charCode;

    } else if (charCode>=SMP_min && charCode<=SMP_max) {
	uint32_t tmp= charCode - SMP_min;
	*q++ = (uint16_t)(SP1_min + tmp/1024);
	*q++ = (uint16_t)(SP2_min + tmp%1024);

    } else {
	*q++ = DefaultUnicode;
    }
    return (size_t)(q - toptr);
}

int uconvUtf16ToUnicode (uint32_t *into, unsigned wlen, const uint16_t wtmp[])
{
    int rc;

    switch (wlen) {
    case 1: {
	unsigned w;

	w= wtmp[0];
	if (w>=SP1_min && w<=SP2_max) goto ERR;
	*into= w;
	rc= 0;
	break; }

    case 2: {
	unsigned w1, w2;

	w1= wtmp[0];
	w2= wtmp[1];
	if (!(w1>=SP1_min && w1<=SP1_max &&
	      w2>=SP2_min && w2<=SP2_max)) {
	    goto ERR;
	}
	*into= SMP_min + (w1-SP1_min)*0x0400 + (w2-SP2_min);
	rc= 0;
	break; }

    default: ERR:
	*into = DefaultUnicode;
	rc= -1;
    }
    return rc;
}

static uint32_t Utf_Max1 = 0x0080;
static uint32_t Utf_Max2 = 0x07ff;
static uint32_t Utf_Max3 = 0xffff;

size_t uconvUnicodeToUtf8 (uint8_t to[], uint32_t from)
{
    uint8_t *q= to;

    if ((from>=SP1_min && from<=SP1_max) ||
	from>SMP_max) {
	/* ?! */

    } else if (from<Utf_Max1) {
	*q++ = (uint8_t)from;

    } else if (from<Utf_Max2) {
	*q++ = (uint8_t)(0xc0 | (from>>6));
	*q++ = (uint8_t)(0x80 | (from&0x3f));

    } else if (from<Utf_Max3) {
	*q++ = (uint8_t)(0xe0 | (from>>12));
	*q++ = (uint8_t)(0x80 | ((from>>6)&0x3f));
	*q++ = (uint8_t)(0x80 | (from&0x3f));

    } else {
	*q++ = (uint8_t)(0xf0 | (from>>18));
	*q++ = (uint8_t)(0x80 | ((from>>12)&0x3f));
	*q++ = (uint8_t)(0x80 | ((from>> 6)&0x3f));
	*q++ = (uint8_t)(0x80 | (from&0x3f));
    }

    return q-to;
}

/* this rutins decodes emul->fAnsi into DTCHAR_ANSI / DTCHAR_OEM / DTCHAR_UTF8 */
int uconvGetEmulDtcc (const struct Emul *emul)
{
    int server_dtcc;

    switch (emul->fAnsi) {
    case 0:  server_dtcc= DTCHAR_OEM;  break;
    case 1:  server_dtcc= DTCHAR_ANSI; break;
    default: server_dtcc= DTCHAR_UTF8; break;
    }
    return server_dtcc;
}

/* returns length in bytes */
size_t uconvCharToServer (uint32_t from, int dtcc, void *to, const struct Emul *emul)
{
    size_t len= 0;
static const char substitue = '?';
    uint16_t wtmp [2];	/* it can be a surrogate pair */
    unsigned wlen;	/* 0/1/2: invalid/BMP/supplemental */

    if (dtcc==DTCHAR_RAW_BINARY) {	/* such as 6-byte mouse-sequence */
	*(unsigned char *)to = (unsigned char)from;
	len= 1;

    } else if (emul->fAnsi==2) {/* to utf8 */
	uint32_t uchar;

	if (dtcc==DTCHAR_ASCII   ||
	    dtcc==DTCHAR_UNICODE ||
	    dtcc==DTCHAR_ISO88591) {
	    uchar= from;

	} else if(dtcc==DTCHAR_XTERM_GRAPH) {
	    uchar= GraphMapWide ((unsigned char)from);

	} else {
	    unsigned char chtmp= (unsigned char)from;
	    int fromcp;
	    uint16_t wtmp [2];	/* it can be a surrogate pair */
	    unsigned wlen;	/* 0/1/2: invalid/BMP/supplemental */
	    int rc;

	    if (dtcc==DTCHAR_OEM) fromcp= CP_OEMCP;
	    else		  fromcp= CP_ACP;

	    wlen= MultiByteToWideChar (fromcp, 0, (void *)&chtmp, 1, wtmp, 2);
	    if (!wlen) goto RETURN;

	    rc= uconvUtf16ToUnicode (&uchar, wlen, wtmp);
	    if (rc) goto RETURN;
	}
	len= uconvUnicodeToUtf8 (to, uchar);

    } else {	/* to single byte */
	if (dtcc==DTCHAR_ISO88591) {		/* <FIXME> */
	    *(unsigned char *)to = (unsigned char)from;
	    len= 1;

	} else if (dtcc==DTCHAR_UNICODE) {	/* from Unicode to ANSI/OEM */
	    unsigned char chtmp;
	    unsigned tocp;
	    const CodeTable *ct;

	    wlen= uconvUnicodeToUtf16 (wtmp, from);
	    if (!wlen) goto RETURN;

	    if (emul->fAnsi==1) {
		tocp= CP_ACP;
		ct= &emul->ctAnsiToServer;
	    } else {
		tocp= CP_OEMCP;
		ct= &emul->ctOemToServer;
	    }

	    len= WideCharToMultiByte (tocp, 0, wtmp, wlen,
		    (char *)&chtmp, 1, &substitue, NULL);
	    if (!len) goto RETURN;

	    *(unsigned char *)to = (*ct)[chtmp];
	    len= 1;

	} else if (dtcc==DTCHAR_XTERM_GRAPH) {
	    if (emul->fAnsi==1) *(unsigned char *)to = GraphMapAnsi ((unsigned char)from);
	    else		*(unsigned char *)to = GraphMapOem  ((unsigned char)from);
	    len= 1;
		
	} else {
	    if (dtcc==DTCHAR_OEM) *(unsigned char *)to = emul->ctOemToServer  [(unsigned char)from];
	    else		  *(unsigned char *)to = emul->ctAnsiToServer [(unsigned char)from];
	    len= 1;
	}
    }

RETURN:
    return len;
}

/* like the previous one, but for a byte-sequence
 * if the conversion fails for some reason, 'rest' will contain the unconverted part
 */
int uconvBytesToServer (const ConstBuffData *from, int dtcc, DynBuffer *to, const struct Emul *emul,
    ConstBuffData *rest)
{
    int rc;
    int dtcc2;	/* if dtcc==DTCHAR_UTF* then dtcc2==DTCHAR_UNICODE */
    ConstBuffData brest= *from;

    if ((dtcc==DTCHAR_UTF8  && emul->fAnsi==2) || /* from UTF8  to UTF8 */
	(dtcc==DTCHAR_ASCII && emul->fAnsi==2) || /* from ASCII to UTF8 */
	(dtcc==DTCHAR_RAW_BINARY)) {		 /* binary, don't convert */
	BuffAppend (to, from);
	rc= 0;
	goto RETURN;
    }

    if (dtcc==DTCHAR_UTF8 || dtcc==DTCHAR_UTF16) dtcc2= DTCHAR_UNICODE;
    else					 dtcc2= dtcc;

    for (rc= 0; rc==0 && brest.len!=0;) {
	uint32_t c;

	if (dtcc==DTCHAR_UTF32) {
	    c= *(uint32_t *)brest.ptr;
	    brest.ptr += sizeof (uint32_t);
	    brest.len -= sizeof (uint32_t);

	} else if (dtcc==DTCHAR_UTF16) {
	    c= *(uint16_t *)brest.ptr;

	    if (c<SP1_min || c>SP2_max) {
		brest.ptr += sizeof (uint16_t);
		brest.len -= sizeof (uint16_t);
	    } else {
		Utf16StreamContext ctx= Empty_Utf16StreamContext;
		BOOL found;

		found= Utf16StreamContextAdd (&ctx, &brest, &c);

		if (!found) {
		    rc= -1;
		    continue;
		}
	    }

	} else if (dtcc==DTCHAR_UTF8) {
	    c= (unsigned char)brest.ptr[0];

	    if (c==(c&0x7f)) {	/* 00..7f: single byte */
		++brest.ptr;
		--brest.len;

	    } else {		/* the hard part: get the first utf8-sequence */
		Utf8StreamContext ctx= Empty_Utf8StreamContext;
		BOOL found;

		found= Utf8StreamContextAdd (&ctx, &brest, &c);

		if (!found) {
		    rc= -1;
		    continue;
		}
	    }

	} else {	/* single byte */
	    c= *(unsigned char *)brest.ptr;
	    ++brest.ptr;
	    --brest.len;
	}

	BuffSecure (to, 4); /* maximum length for an utf8 sequence */
	to->len += uconvCharToServer (c, dtcc2, to->ptr + to->len, emul);
    }

RETURN:
    if (rest) *rest= brest;
    return rc;
}

BOOL Utf8StreamContextAdd (Utf8StreamContext *ctx, ConstBuffData *from, uint32_t *punic)
{
    const char *p, *plim;
    BOOL found= FALSE;

    p= from->ptr;
    plim= p + from->len;

    while (!found && p<plim) {
	unsigned c= (unsigned char)*p++;
	BOOL fcontbyte;

	if (c<=0x7f) {
	    if (ctx->state == 0) {
		*punic= c;

	    } else { /* ?! */
		--p;	/* undo: we ignore it for now, we report the invalid UTF8 sequence */
		ctx->state= 0;
		*punic= DefaultUnicode;
	    }
	    found= TRUE;
	    continue;
	}

	fcontbyte= (c&0xc0)==0x80;
	if (fcontbyte) {
	    if (ctx->state == 0) { /* ?! */
		*punic= DefaultUnicode;
		found= TRUE;
		continue;

	    } else {
		ctx->unicode = (ctx->unicode<<6) | (c&0x3f);
		++ctx->nutf8act;
		if (ctx->nutf8act == ctx->nutf8tot) {
		    BOOL fRangeError;

		    fRangeError=
			(ctx->nutf8tot==2 && ctx->unicode < 0x80)  ||
			(ctx->nutf8tot==3 && ctx->unicode < 0x800) ||
			(ctx->nutf8tot==4 && (ctx->unicode < 0x10000UL || ctx->unicode > 0x10ffffUL));
		    if (fRangeError) {
			*punic= DefaultUnicode;
		    } else {
			*punic= ctx->unicode;
		    }

		    found= TRUE;
		    ctx->state= 0;
		}
		continue;
	    }
	}

	if      ((c&0xe0)==0xc0) ctx->nutf8tot= 2, ctx->unicode= c&0x1f;
	else if ((c&0xf0)==0xe0) ctx->nutf8tot= 3, ctx->unicode= c&0x0f;
	else if ((c&0xf8)==0xf0) ctx->nutf8tot= 4, ctx->unicode= c&0x07;
	else {
	    *punic= DefaultUnicode;
	    found= TRUE;
	    continue;
	}
	/* it still might be invalid eg c0,c1
	   that will be detected at the last byte (fRangeChange) */
	ctx->nutf8act= 1;
	ctx->state= 1;
    }

    from->ptr= p;
    from->len= plim - p;

    return found;
}

/* this rutin changes paremeter 'from'; returns unicode in '*punic'
 * you should call this function repeatedly while the return value is TRUE;
 * when it runs out of data, it returns FALSE (and sets 'from->len' to zero)
 */
BOOL Utf16StreamContextAdd (Utf16StreamContext *ctx, ConstBuffData *from, uint32_t *punic)
{
    uint16_t *p, *plim;
    uint32_t unic= DefaultUnicode;
    BOOL found= FALSE;

    p= (uint16_t *)from->ptr;
    plim= (uint16_t *)(from->ptr + from->len);

    for (--p; ++p<plim && !found;) {
	uint16_t w= *p;

	if (ctx->state==0) {
	    if (w < SP1_min || w > SP2_max) {
		unic= (uint32_t)w;
		found= TRUE;

	    } else if (w >= SP1_min && w < SP2_min) {
		ctx->state= 1;
		ctx->first_half= w;

	    } else {
		unic= DefaultUnicode;
		ctx->state= 0;
		found= TRUE;
	    }
	} else {
	    if (w >= SP2_min && w <= SP2_max) {
		unic= SMP_min + ((ctx->first_half - SP1_min)<<10) + (w - SP2_min);
		ctx->state= 0;
		found= TRUE;

	    } else {
		--p;	/* step back */
		unic= DefaultUnicode;
		ctx->state= 0;
		found= TRUE;
	    }
	}
    }

    from->ptr= (char *)p;
    from->len= (char *)plim - (char *)p;

    if (punic) *punic= unic;
    return found;
}

/* uconvBytesToUtf16: from->len is interpreted as 'number of bytes' */
int uconvBytesToUtf16 (const ConstBuffData *from, int dtcc, DynBuffer *to)
{
    if (!from || from->len==0) {
	return 0;

    } else if (dtcc==DTCHAR_UTF16) { /* don't convert */
	BuffAppend (to, from);
	return 0;

    } else if (dtcc==DTCHAR_ASCII || dtcc==DTCHAR_ISO88591 ||
	       dtcc==DTCHAR_ANSI  || dtcc==DTCHAR_OEM) {
	int cpin;
	unsigned maxwlen, retwlen;

	switch (dtcc) {
	case DTCHAR_ASCII:    cpin= 20127;    break; /* 20127 = us-ascii */
	case DTCHAR_ANSI:     cpin= CP_ACP;   break; /* ANSI (context-dependent) */
	case DTCHAR_OEM:      cpin= CP_OEMCP; break; /* OEM (context-dependent) */
	case DTCHAR_ISO88591: cpin= 28591;    break; /* 28591 = ISO-8859-1 */
	}

	maxwlen= from->len;		/* we are in BMP: 2byte/char */
	BuffSecure (to, 2*maxwlen);

	retwlen= MultiByteToWideChar (cpin, 0
		, (void *)from->ptr, from->len		/* from: ptr, len    */
		, (wchar_t *)to->ptr, maxwlen);		/* to:   ptr, maxlen */
	to->len += 2*retwlen;
	return 0;

    } else if (dtcc==DTCHAR_UTF32) {
	const uint32_t *p= (uint32_t *)from->ptr;
	const uint32_t *plim= (uint32_t *)((char *)from->ptr + from->len);

	for (--p; ++p<plim;) {
	    unsigned wcharlen; /* 0/1/2 number of wchar's: bad/BMP/supplemental */

	    BuffSecure (to, 4);
	    wcharlen= uconvUnicodeToUtf16 ((uint16_t *)(to->ptr + to->len), *p);
	    to->len += 2*wcharlen;
	}

    } else if (dtcc==DTCHAR_XTERM_GRAPH) {
	const char *p=    from->ptr;
	const char *plim= from->ptr + from->len;

	BuffSecure (to, 2*(plim-p));

	for (--p; ++p<plim;) {
	    *(uint16_t *)(to->ptr + to->len) = GraphMapWide ((unsigned char)*p);
	    to->len += sizeof (uint16_t);
	}

    } else if (dtcc==DTCHAR_UTF8) {
	Utf8StreamContext ctx= Empty_Utf8StreamContext;
	ConstBuffData brest= *from;
	uint32_t c;

	while (Utf8StreamContextAdd (&ctx, &brest, &c)) {
	    unsigned wlen;

	    BuffSecure (to, 4);
	    wlen= uconvUnicodeToUtf16 ((uint16_t *)(to->ptr + to->len), c);
	    to->len += 2*wlen;
	}

    } else {
	return -1;
    }

    return 0;
}
