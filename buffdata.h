/* buffdata.h */
 
#ifndef BUFFDATA_H
#define BUFFDATA_H

#include <stddef.h>

typedef struct ConstBuffData {
    const char *ptr;
    size_t len;
} ConstBuffData;

typedef struct BuffData {
    char *ptr;
    size_t len;
} BuffData;

#define EmptyBuffData {NULL, 0}
 
typedef struct Buffer {
    char *ptr;
    size_t len;
    size_t maxlen;
} Buffer;

#define EmptyBuffer {NULL, 0, 0}

/* type 'DynBuffer' as function-parameter means:
   the function will (re)alloc this buffer,
   so it cannot be a static buffer
 */
#define DynBuffer Buffer

#define StaticConstBuffData(Name,Sname,Value) \
    static const char Sname [] = Value; \
    static const ConstBuffData Name = { (char *)Sname, sizeof (Sname)-1}

#define StaticConstBD(Name,Value) StaticConstBuffData(Name,N##Name,Value)

int BuffDataEq	 (const ConstBuffData *a, const ConstBuffData *b);
int BuffDataEqI (const ConstBuffData *a, const ConstBuffData *b);

int BuffDataCmp	 (const ConstBuffData *a, const ConstBuffData *b);
int BuffDataCmpI (const ConstBuffData *a, const ConstBuffData *b);

/* BuffDataCmp[I] returns -1/0/1 for LT/EQ/GT */
/* warning: BuffDataCmpI uses strncasecmp, so does not work well
   if '\0' character found */

int BuffDataEnd  (const ConstBuffData *data, const ConstBuffData *end);
int BuffDataEndI (const ConstBuffData *data, const ConstBuffData *end);
/* returns 1/0 for yes/no; 'I' means case-insensitive (calls strcasecmp)
   e.g: BuffDataEndI ("TEJ", "ej") returns 1 (yes)
 */

void BuffCopyZ (Buffer *into, const ConstBuffData *from);
/* adds terminating zero */
void BuffDupZ  (BuffData *into, const ConstBuffData *from);
/* adds terminating zero */
void BuffDupS  (BuffData *into, const char *from);
/* adds terminating zero */

void BuffSecure (Buffer *into, size_t minimum);
#define BuffSecX BuffSecure

void BuffAppend (Buffer *into, const ConstBuffData *from);
/* BuffAppend is the same as BuffInsertAt (into,into->len,from) */
void BuffAppendLP (Buffer *into, size_t len, const void *p);
void BuffAppendS (Buffer *into, const char *s);
/* for convenience */

void BuffAddTermZero (DynBuffer *b);
/* Adds a terminating zero, without changing 'b->len' */
void BuffAddTermZeroes (DynBuffer *b, unsigned nzero);
/* Adds 1-4 terminating zeroes, without changing 'b->len',
   eg: BuffAddTermZeroes (into, sizeof (wchar_t));
       BuffAddTermZeroes (into, sizeof (uchar32_t));
 */

void BuffCutback (Buffer *b, size_t newmaxlen);

#endif
