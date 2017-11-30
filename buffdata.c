/* buffdata.c */

#include <stddef.h>
#include <string.h>

#include "buffdata.h"

/* <HELPME> prefix underline required or not? */
#if !defined strcasecmp
#define strcasecmp  stricmp
#define strncasecmp strnicmp
#endif

void *xmalloc  (size_t size);
void *xrealloc (void *ptr, size_t size);
void  xfree(void* ptr);

int BuffDataCmp (const ConstBuffData *a, const ConstBuffData *b)
{
    size_t cmplen;
    int def, cmp;

    if (a->len > b->len)      def = 1,	cmplen = b->len;
    else if (a->len < b->len) def = -1, cmplen = a->len;
    else		      def = 0,	cmplen = a->len;

    if (cmplen == 0) cmp = 0;
    else	     cmp = memcmp (a->ptr, b->ptr, cmplen);

    if (cmp==0) cmp = def;
    return cmp;
}

int BuffDataCmpI (const ConstBuffData *a, const ConstBuffData *b)
{
    size_t cmplen;
    int def, cmp;

    if (a->len > b->len)      def = 1,	cmplen = b->len;
    else if (a->len < b->len) def = -1, cmplen = a->len;
    else		      def = 0,	cmplen = a->len;

    if (cmplen == 0) cmp = 0;
    else	     cmp = strncasecmp (a->ptr, b->ptr, cmplen);

    if (cmp==0) cmp = def;
    return cmp;
}

int BuffDataEq (const ConstBuffData *a, const ConstBuffData *b)
{
    return
	a->len == b->len &&
	memcmp (a->ptr, b->ptr, a->len)==0;
}

int BuffDataEqI (const ConstBuffData *a, const ConstBuffData *b)
{
    return
	a->len == b->len &&
	strncasecmp (a->ptr, b->ptr, a->len)==0;
}

void BuffSecure (Buffer *into, size_t minimum)
{
    size_t freesize, reqsize;

    freesize= into->maxlen - into->len;
    if (freesize >= minimum) return;

    reqsize = minimum - freesize;
    if (reqsize < into->maxlen) reqsize= into->maxlen; /* aggressive;) */
    into->maxlen += reqsize;
    into->ptr = xrealloc (into->ptr, into->maxlen);
}

void BuffCopyZ (Buffer *into, const ConstBuffData *from)
{
    into->len= 0;

    if (into->maxlen < from->len+1) BuffSecure (into, from->len+1);
    if (from->len) memcpy (into->ptr, from->ptr, from->len);
    into->ptr[from->len] = '\0';
    into->len = from->len;
}

void BuffDupZ (BuffData *into, const ConstBuffData *from)
{
    into->ptr= xmalloc (from->len + 1);
    into->len = from->len;
    if (from->len) memcpy (into->ptr, from->ptr, from->len);
    into->ptr [from->len] = '\0';
}

void BuffDupS (BuffData *into, const char *from)
{
    ConstBuffData tmp;

    tmp.len= from? strlen (from): 0;
    tmp.ptr= from;
    BuffDupZ (into, &tmp);
}

int BuffDataEnd (const ConstBuffData *data, const ConstBuffData *end)
{
    if (data->len < end->len) return 0; /* not match */
    return memcmp (data->ptr + data->len - end->len,
		   end->ptr, end->len) == 0;
}

int BuffDataEndI (const ConstBuffData *data, const ConstBuffData *end)
{
    if (data->len < end->len) return 0;
    return strncasecmp (data->ptr + data->len - end->len,
		   end->ptr, end->len) == 0;
}

void BuffAppend (Buffer *into, const ConstBuffData *from)
{
    size_t sumlen;

    if (! from->len) return;
    sumlen = into->len + from->len;
    if (sumlen > into->maxlen) BuffSecure (into, from->len);

    memcpy (into->ptr + into->len, from->ptr, from->len);
    into->len += from->len;
}

void BuffAppendLP (Buffer *into, size_t len, const void *p)
{
    ConstBuffData from;

    if (len) {
	from.len= len;
	from.ptr= (const char *)p;
	BuffAppend (into, &from);
    }
}

void BuffAppendS (Buffer *into, const char *s)
{
    ConstBuffData from;

    if (s && *s) {
	from.len= strlen (s);
	from.ptr= s;
	BuffAppend (into, &from);
    }
}

void BuffAddTermZero (DynBuffer *b)
{
    if (b->maxlen < b->len+1) BuffSecure (b, 1);
    b->ptr[b->len] = '\0';
}

void BuffAddTermZeroes (DynBuffer *b, unsigned nzero)
{
    if (!nzero) return; /* very funny */
    if (b->maxlen < b->len+nzero) BuffSecX (b, (b->len+nzero) - b->maxlen);
    memset (b->ptr + b->len, 0, nzero);
}

void BuffCutback (Buffer *b, size_t newmaxlen)
{
    if (b->maxlen==newmaxlen) return;

    b->maxlen = newmaxlen;
    if (b->len > newmaxlen) b->len= newmaxlen;

    if (newmaxlen==0) {
	xfree (b->ptr);
	b->ptr= NULL;

    } else {
	b->ptr = xrealloc (b->ptr, newmaxlen);
    }
}
