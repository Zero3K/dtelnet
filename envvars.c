/* envvars.c */

#include <stdio.h>
#include <string.h>

struct EnvVariable;
struct ConstBuffData;

#define BABITED_OBJECT	struct EnvVariable
#define BABIT_KEY	struct ConstBuffData
#include "babit.h"
#include "harap.h"

#include "utils.h"
#include "buffdata.h"
#include "envvars.h"

struct EnvVars {
    BABIT_ROOT tree;
    unsigned nelements;
};

typedef struct EnvVariable {
    ConstBuffData name;
    ConstBuffData value;
    BabitConnector bc;
} EnvVariable;

static int cmpfun (const EnvVariable *e, const EnvVariable *f, BABIT_EXTRAPAR unused);
static int fndfun (const ConstBuffData *bk, const EnvVariable *p, BABIT_EXTRAPAR unused);

static const BabitControlBlock bcb = {
    0,
    offsetof (EnvVariable, bc),
    (BabitCmpFun *)cmpfun,
    (BabitFndFun *)fndfun,
    0
};

int EV_Init (EnvVars **evp)
{
    *evp= xmalloc (sizeof **evp);
    (*evp)->tree= 0;
    (*evp)->nelements= 0;
    return 0;
}

int EV_Destroy (EnvVars *evp)
{
    EV_Clear (evp);
    xfree (evp);
    return 0;
}

static EnvVars EV_Default = {(BABIT_ROOT)0, 0};

EnvVars *DefaultEnvVars= &EV_Default;

static void EV_ClearCB (EnvVariable *obj, BABIT_EXTRAPAR unused)
{
    if (obj->name.ptr)  xfree ((char *)obj->name.ptr);
    if (obj->value.ptr) xfree ((char *)obj->value.ptr);
    xfree (obj);
}

int EV_Clear (EnvVars *evp)
{
    if (!evp) evp= &EV_Default;

    if (evp->tree) {
	BabitDestroy (&evp->tree, EV_ClearCB, 0, &bcb);
	evp->tree= 0;
    }
    evp->nelements= 0;
    return 0;
}

int EV_Add (EnvVars *evp, const char *name, const char *value, int freplace)
{
    int rc;
    ConstBuffData bn, bv;

    bn.ptr= name;
    bn.len= name? strlen (bn.ptr): 0;
    bv.ptr= value;
    bv.len= value? strlen (value): 0;
    rc= EV_AddB (evp, &bn, &bv, freplace);
    return rc;
}

int EV_AddB (EnvVars *evp, const ConstBuffData *name, const ConstBuffData *value, int freplace)
{
    int rc;
    EnvVariable *oldp, *newp;

    oldp= BabitFindU (&evp->tree, name, &bcb, 0);
    if (oldp) {
	if (!freplace) {
	    rc= 1;
	    goto RETURN;
	}
	if (BuffDataEq (&oldp->value, value)) {
	    rc= 0;
	    goto RETURN;
	}
    }

    newp= xmalloc (sizeof *newp);
    if (oldp) newp->name= oldp->name;
    else      BuffDupZ ((BuffData *)&newp->name, name);
    BuffDupZ ((BuffData *)&newp->value, value);

    if (oldp) BabitReplaceU (&evp->tree, oldp, newp, &bcb, 0);
    else      BabitInsertU  (&evp->tree, newp, &bcb, 0);

    if (oldp) {
	xfree ((char *)oldp->value.ptr);
	xfree (oldp);
    } else {
	++evp->nelements;
    }
    rc= 0;

RETURN:
    return rc;
}

int EV_Del (EnvVars *evp, const char *key)
{
    int rc;
    ConstBuffData bk;

    bk.ptr= key;
    bk.len= key? strlen (key): 0;
    rc= EV_DelB (evp, &bk);
    return rc;
}

int EV_DelB (EnvVars *evp, const ConstBuffData *bn)
{
    int rc;
    EnvVariable *p;

    p= BabitFindU (&evp->tree, bn, &bcb, 0);
    if (!p) {
	rc= 1;
	goto RETURN;
    }

    BabitDelete (&evp->tree, p, &bcb);
    if (evp->nelements) --evp->nelements;
    rc= 0;

RETURN:
    return rc;
}

static int cmpfun (const EnvVariable *e, const EnvVariable *f, BABIT_EXTRAPAR unused)
{
    int cmp;

    cmp= BuffDataCmp (&e->name, &f->name);
    return cmp;
}

static int fndfun (const ConstBuffData *bk, const EnvVariable *p, BABIT_EXTRAPAR unused)
{
    int cmp;

    cmp= BuffDataCmp (bk, &p->name);
    return cmp;
}

int EV_IsEmpty (const EnvVars *evp)
{
    int isempty;

    isempty= !evp->tree || evp->nelements==0;
    return isempty;
}

int EV_Get (const EnvVars *from, int select, const char *ref,
	const char **pname, const char **pvalue)
{
    int rc= -1;
    int fun= -1; /* 0/1 = BabitGetMin, BabitGetNext */
    int op= 0;
    const char *name= NULL;
    const char *value= NULL;
    const EnvVariable *p;
    ConstBuffData br;

    switch (select) {
    case EVGET_FIRST: fun= 0; op= BABIT_GET_MIN; break;
    case EVGET_LT:    fun= 1; op= BABIT_FIND_GT; break;
    case EVGET_LE:    fun= 1; op= BABIT_FIND_GE; break;
    case EVGET_EQ:    fun= 1; op= BABIT_FIND_EQ; break;
    case EVGET_GE:    fun= 1; op= BABIT_FIND_LE; break;
    case EVGET_GT:    fun= 1; op= BABIT_FIND_LT; break;
    case EVGET_LAST:  fun= 0; op= BABIT_GET_MAX; break;
    }

    switch (fun) {
    case 0:
	p= BabitGetMin (&from->tree, op, &bcb);
	break;

    case 1:
	br.ptr= ref;
	br.len= ref? strlen (ref): 0;
	p= BabitFindEx (&from->tree, &br, op, 0, &bcb);
	break;

    default:
	rc= -1;
	goto RETURN;
    }

    if (p==NULL) {
	rc= 1;		/* not found */
    } else {
	name=  p->name.ptr;
	value= p->value.ptr;
	rc= 0;
    }

RETURN:
    if (pname)  *pname=  name;
    if (pvalue) *pvalue= value;
    return rc;
}

int EV_LoadFromIniFile (EnvVars *into, const char *filename, const char *sectionname)
{
    char *buff= NULL;
    unsigned sectmaxsize= 32767, size;
    const char *p, *plim, *q;
    int rc;

    if (!into) into= DefaultEnvVars;
    if (!sectionname) sectionname= "EnvVars";

    buff= xmalloc (sectmaxsize);
    memset (buff, '#', sectmaxsize);

    size= GetPrivateProfileSection (sectionname, buff, sectmaxsize, filename);
    printf ("GetPrivateProfileSection(%s,%s) returned %d\n", sectionname, filename, (int)size);
    if (size==0) { /* No 'EnvVars' section */
	rc= 0;
	goto RETURN;
    }
    if (size==sectmaxsize-2) {
	fprintf (stderr, "Error loading section '%s' from file '%s' (maxsize=%ld)\n",
		sectionname, filename, (long)sectmaxsize);
	rc= -1;
	goto RETURN;
    }

    p= buff;
    plim= buff + size;
    while (p<plim) {
	q= memchr (p, '\0', (size_t)(plim-p));
	if (!q) break;
	printf ("%.*s\n", (int)(q-p), p);
	EV_AddFromIniStr (into, p);
	p= q+1;
    }

    rc= 0;

RETURN:
    if (buff) xfree (buff);
    return rc;
}

int EV_AddFromIniStr (EnvVars *vars, const char *str)
{
    int rc;
    ConstBuffData b, b1, b2;

    if (vars==NULL) vars= DefaultEnvVars;

    if (!str || !*str) {
	rc= -1;
	goto RETURN;
    }

    b.ptr= str;
    b.len= strlen (str);
    rc= harapr (&b, (BuffData *)&b1, (BuffData *)&b2, '=');
    if (rc) {
	rc= -1;
	goto RETURN;		/* No '=' found */
    }

    printf ("\"%.*s\" <> \"%.*s\"\n",
	(int)b1.len, b1.ptr, (int)b2.len, b2.ptr);

    if (b2.len==0) {
	rc= EV_DelB (vars, &b1);
	printf ("EV_DelB(%.*s) returned %d\n", (int)b1.len, b1.ptr, rc);
	goto RETURN;
    }

    if (b2.ptr[0]=='"' && b2.len>=2 && b2.ptr[b2.len-1]=='"') {
	Buffer bunesc;

	b2.ptr += 1;
	b2.len -= 2;
    /* unescape (in place) */
	bunesc.ptr= (char *)b2.ptr;
	bunesc.len= 0;
	bunesc.maxlen= b2.len;
	bunescape (2, &b2, &bunesc, NULL);	/* stop at unescaped "double" quote */
	b2.ptr= bunesc.ptr;
	b2.len= bunesc.len;
    }

/* <FIXME>: unescape */
    EV_AddB (vars, &b1, &b2, 1);

    rc= 0;

RETURN:
    return rc;
}
