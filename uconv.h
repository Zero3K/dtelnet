/* uconv.h */

#ifndef UCONV_H
#define UCONV_H

#include "preset.h"
#include <stddef.h>
#include <windows.h>
#include "platform.h"

/* returns 0/1/2: invalid / BMP / supplemental */
size_t uconvUnicodeToUtf16 (uint16_t *toptr, uint32_t charCode);

/* wlen: 1/2: BMP / supplemental
   returns 0/-1: ok/error 
 */
int uconvUtf16ToUnicode (uint32_t *into, unsigned wlen, const uint16_t wtmp[]);

/* returns length=1-4, or 0 for error */
size_t uconvUnicodeToUtf8 (uint8_t to[], uint32_t from);

/* dtcc: DTCHAR_ASCII/ANSI/OEM/XTERM_GRAPHICS/UNICODE
   returns length in bytes */
size_t uconvCharToServer (uint32_t from, int dtcc, void *to, const struct Emul *emul);

/* like the previous one, but for a byte-sequence
 * if the conversion fails for some reason, 'rest' will contain the unconverted part
 */
int uconvBytesToServer (const ConstBuffData *from, int dtcc, DynBuffer *to, const struct Emul *emul,
    ConstBuffData *rest);

/* uconvBytesToUtf16: from->len is interpreted as 'number of bytes' */
int uconvBytesToUtf16 (const ConstBuffData *from, int dtcc, DynBuffer *to);

/* this rutins decodes emul->fAnsi into DTCHAR_ANSI / DTCHAR_OEM / DTCHAR_UTF8 */
int uconvGetEmulDtcc (const struct Emul *emul);

typedef struct Utf8StreamContext {
    int state;			/* 0: empty */
    int nutf8act;		/* number of characters read */
    int nutf8tot;		/* length of the sequence (based on the first byte) */
    uint32_t unicode;	/* the unicode represented by the sequence */
} Utf8StreamContext;

#define Empty_Utf8StreamContext {0, 0, 0, 0}
#define Reset_Utf8StreamContext(pctx) ((pctx)->state= 0)

/* this rutin changes paremeter 'from'; returns unicode in '*punic'
 * you should call this function repeatedly while the return value is TRUE;
 * when it runs out of data, it returns FALSE (and sets 'from->len' to zero)
 */
BOOL Utf8StreamContextAdd (Utf8StreamContext *ctx, ConstBuffData *from, uint32_t *punic);

typedef struct Utf16StreamContext {
    int state;			/* 0: empty */
    uint16_t first_half;	/* if we are inside a surrogate pair */
} Utf16StreamContext;

#define Empty_Utf16StreamContext {0, 0}
#define Reset_Utf16StreamContext(pctx) ((pctx)->state= 0)

/* this rutin changes paremeter 'from'; returns unicode in '*punic'
 * you should call this function repeatedly while the return value is TRUE;
 * when it runs out of data, it returns FALSE (and sets 'from->len' to zero)
 */
BOOL Utf16StreamContextAdd (Utf16StreamContext *ctx, ConstBuffData *from, uint32_t *punic);

#endif
