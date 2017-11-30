/* babit.c */

#include <stdio.h>
#include <stdlib.h>

#define BABITED_OBJECT struct Blob
#define BABIT_KEY      void

struct Blob;

#include "babit.h"

typedef struct Blob {
    int unusedfld;
} Blob;

#define MkConn(obj,bcb) ((BabitConnector *)\
                         ((char *)&(obj->unusedfld) + bcb->offset))

#define IsNullPtr(obj,from)  ((from)==0)
#define StoreNullPtr(obj,to) (to)=0

#ifdef INTEL_8086_HUGE_LONG
  #define LoadPtr(obj,from)    (Blob *)((char huge *)(obj) + (from))
  #define StorePtr(obj,to,ptr) (to)=(char huge *)(ptr)-(char huge *)(obj)
  #define StoreAnyPtr(obj,to,ptr) ((to)=\
                                (ptr) ? ((char huge *)(ptr)-(char huge *)(obj)) : 0)
#else
  #define LoadPtr(obj,from)    (Blob *)((char *)(obj) + (from))
  #define StorePtr(obj,to,ptr) (to)=(char *)(ptr)-(char *)(obj)
  #define StoreAnyPtr(obj,to,ptr) ((to)=\
                                (ptr) ? ((char *)(ptr)-(char *)(obj)) : 0)
#endif
#define LoadAnyPtr(obj,from) ((from) ? LoadPtr ((obj),(from)) : NULL)

static char CORRUPT [] = "*** Balanced Binary Tree "
			 "is corrupeted (%p->balance=%d)\n";

static char MSG_LEFTINVALID [] = "*** %p->Left = %p : invalid\n";
static char MSG_LEFTNOTCONN [] = "*** %p->Left = %p, %p->Father = %p\n";
static char MSG_LEFTORDER   [] = "*** %p is not greater than its Left-child (%p)\n";
static char MSG_RGHTINVALID [] = "*** %p->Right = %p : invalid\n";
static char MSG_RGHTNOTCONN [] = "*** %p->Right = %p, %p->Father = %p\n";
static char MSG_RGHTORDER   [] = "*** %p is not less than its Right-child (%p)\n";
static char MSG_UNBALANCED  [] = "*** %p is unbalanced: ln=%d, rn=%d\n";
static char MSG_BALANCOUNT  [] = "*** %p 's balance counter is invalid: %d != -%d+%d\n";

BABITED_OBJECT *BabitFind (const BABIT_ROOT *root, const BABIT_KEY *key,
			   const BabitControlBlock *bcb)
{
    return BabitFindU (root, key, bcb, bcb->fndextra);
}

BABITED_OBJECT *BabitFindU (const BABIT_ROOT *root, const BABIT_KEY *key,
			    const BabitControlBlock *bcb,
			    BABIT_EXTRAPAR uspar)
{
    int cmp;
    BabitConnector *c;
    const BABITED_OBJECT *rootp;

    if (root == NULL || key == NULL) return NULL; /* error */

    rootp = LoadAnyPtr (root, *root);
    while (rootp) {
	cmp = ((BabitFndFunEx *)bcb->fndfun) (key, rootp, uspar);
	if (cmp==0) return (BABITED_OBJECT *)rootp;
	c = MkConn (rootp, bcb);
	rootp = (cmp<0) ? LoadAnyPtr (rootp, c->Left)
		        : LoadAnyPtr (rootp, c->Right);
    }
    return NULL;
}

BABITED_OBJECT *BabitFindEx (const BABIT_ROOT *root,
			     const BABIT_KEY *key,
			     int option, int value,
			     const BabitControlBlock *bcb)
{
    return BabitFindExU (root, key, option, value, bcb, bcb->fndextra);
}

BABITED_OBJECT *BabitFindExU (const BABIT_ROOT *root,
			      const BABIT_KEY *key,
			      int option, int value,
			      const BabitControlBlock *bcb,
			      BABIT_EXTRAPAR uspar)
{
    int cmp, next;
    int valuesign;
    const BABITED_OBJECT *last;
    const BABITED_OBJECT *rootp;
    BabitConnector *c;
 
    if (option < BABIT_FIND_LT || option > BABIT_FIND_GT ||
	root == NULL || key == NULL || bcb == NULL) 
	return NULL;
 
    rootp = LoadAnyPtr (root, *root);

    if (value>0) valuesign = 1;
    else if (value<0) valuesign = -1;
    else valuesign = 0;

    last = NULL;
    next = 0;
    while (rootp) {
	cmp = ((BabitFndFunEx *)bcb->fndfun) (key, rootp, uspar);
	if (cmp > value) {
	    if (option >= BABIT_FIND_GE) last = rootp;
	    next = 1;	
	} else if (cmp < value) {
	    if (option <= BABIT_FIND_LE) last = rootp;
            next = -1;		
	} else {
	    switch (option) {
	    case BABIT_FIND_LT: next = 1; break;
	    case BABIT_FIND_LE: next = -1; last = rootp; break;
	    case BABIT_FIND_EQ:
		if (valuesign==0) return (BABITED_OBJECT *)rootp;
	        last = rootp;
		next = valuesign;
		break;
	    case BABIT_FIND_GE: next = 1; last = rootp; break;
	    case BABIT_FIND_GT: next = -1; break;
	    }
	}
        c = MkConn (rootp, bcb);
	if (next == -1) rootp = LoadAnyPtr (rootp, c->Left);
	else            rootp = LoadAnyPtr (rootp, c->Right);
    }
    return (BABITED_OBJECT *)last;
}

int BabitCheck (const BABIT_ROOT *root, int *dom, int *n,
		const BabitControlBlock *bcb)
{
    return BabitCheckU (root, dom, n, bcb, bcb->fndextra);
}

int BabitCheckU (const BABIT_ROOT *root, int *dom, int *n,
		 const BabitControlBlock *bcb,
	         BABIT_EXTRAPAR uspar)
{
    const BABITED_OBJECT *object;
    const BABITED_OBJECT *l, *r;
    BabitConnector *c, *lc, *rc;
    BABIT_OFFSET ltemp, rtemp;
    int ln, rn;
    int ldom, rdom;
    int err;

    if (root == NULL || *root == 0) {
	if (dom) *dom = 0;
	if (n) *n = 0;
	return 0;
    }

    err = 0;
    object = LoadPtr (root, *root);
    c = MkConn (object, bcb);

    if (! IsNullPtr (object, c->Left)) {
        l = LoadPtr (object, c->Left);
	if (l == NULL || l == object || c->Left == c->Right) {
	    fprintf (stderr, MSG_LEFTINVALID,
		     object, l);
	    ++err;
	}
	lc = MkConn (l, bcb);
	if (LoadPtr (l, lc->Father) != object) {
	    fprintf (stderr, MSG_LEFTNOTCONN,
		     object, l, l, LoadPtr (l, lc->Father));
	    ++err;
	}
	if (((BabitCmpFunEx *)bcb->cmpfun) (object, l, uspar) <= 0) {
	    fprintf (stderr, MSG_LEFTORDER,
		     object, l);
	    ++err;
	}
        StorePtr (&ltemp, ltemp, l);
	err += BabitCheckU (&ltemp, &ldom, &ln, bcb, uspar);
    } else ln = 0, ldom = 0;

    if (! IsNullPtr (object, c->Right)) {
        r = LoadPtr (object, c->Right);
	if (r == NULL || r == object || c->Left == c->Right) {
	    fprintf (stderr, MSG_RGHTINVALID,
		     object, r);
	    ++err;
	}
	rc = MkConn (r, bcb);
	if (LoadPtr (r, rc->Father) != object) {
	    fprintf (stderr, MSG_RGHTNOTCONN,
		     object, r, r, LoadPtr (r, rc->Father));
	    ++err;
	}
	if (((BabitCmpFunEx *)bcb->cmpfun) (object, r, uspar) >= 0) {
	    fprintf (stderr, MSG_RGHTORDER,
		     object, r);
	    ++err;
	}
        StorePtr (&rtemp, rtemp, r);
	err += BabitCheckU (&rtemp, &rdom, &rn, bcb, uspar);
    } else rn = 0, rdom = 0;

    if (rn-ln < -1 || rn-ln > 1) {
	fprintf (stderr, MSG_UNBALANCED,
		 object, ln, rn);
	++err;
    }
    if (rn-ln != c->Balance) {
	fprintf (stderr, MSG_BALANCOUNT,
		 object, c->Balance, ln, rn);
	++err;
    }
    if (dom) *dom = ldom+rdom+1;
    if (n) *n = (ln >= rn)  ? ln+1 : rn+1;
    return err;
}

static void BabBalance (BABIT_ROOT *root, BABITED_OBJECT *object,
		      int dir, int delta, const BabitControlBlock *bcb)
{
    BABITED_OBJECT *father, *l, *r, *lr, *rl,
                   *lrl, *lrr, *rll, *rlr;
    BabitConnector *c, *fc, *lc, *rc, *lrc, *rlc,
    		   *lrlc, *lrrc, *rllc, *rlrc;
    int fdir;
    int leave;

    c = MkConn (object, bcb);

    lrc = rlc = lrlc = lrrc = rllc = rlrc = fc = NULL;
    fdir = 0;
    leave = 0;
    do {
        father = LoadAnyPtr (object, c->Father);
	if (father) {
	    fc = MkConn (father, bcb);
	    fdir = (object == LoadPtr (father, fc->Left)) ? -1 : 1;
	}

	if (delta == -1) {
	    if (dir == -1) ++c->Balance;
	    else           --c->Balance;
	    if (c->Balance != 0) delta = 0; /* else delta = -1 */
	} else {
	    if (dir == -1) --c->Balance;
	    else           ++c->Balance;
	    if (c->Balance == 0) delta = 0; /* else delta = 1 */
	}

	if (c->Balance == -2) {
	    l = LoadPtr (object, c->Left);
	    lc = MkConn (l, bcb);
	    if ((lr = LoadAnyPtr (l, lc->Right)) != NULL)
	        lrc = MkConn (lr, bcb);
	    if (lc->Balance == -1) {
		StorePtr (object, c->Father, l);
		c->Balance = 0;
		if (lr) {
		    StorePtr (object, c->Left, lr);
		    StorePtr (lr, lrc->Father, object);
                } else
		    StoreNullPtr (object, c->Left);
		lc->Balance = 0;
		StorePtr (l, lc->Right, object);
		object = l;
		if (father) {
		    StorePtr (l, lc->Father, father);
		    if (fdir == -1) StorePtr (father, fc->Left, l);
		    else            StorePtr (father, fc->Right, l);
		} else
		    StoreNullPtr (l, lc->Father);
		--delta;

	    } else if (lc->Balance == 0) {
		StorePtr (object, c->Father, l);
		c->Balance = -1;
		StorePtr (object, c->Left, lr);
		StorePtr (lr, lrc->Father, object);
		lc->Balance = 1;
		StorePtr (l, lc->Right, object);
		object = l;
		if (father) {
		    StorePtr (l, lc->Father, father);
		    if (fdir == -1) StorePtr (father, fc->Left, l);
		    else            StorePtr (father, fc->Right, l);
                } else
		    StoreNullPtr (l, lc->Father);

	    } else if (lc->Balance == 1) {
	        if ((lrl = LoadAnyPtr (lr, lrc->Left)) != NULL)
		    lrlc = MkConn (lrl, bcb);
		if ((lrr = LoadAnyPtr (lr, lrc->Right)) != NULL)
		    lrrc = MkConn (lrr, bcb);
	    
		StorePtr (l, lc->Father, lr);
		lc->Balance = (lrc->Balance == 1) ? -1 : 0;

		if (lrl) {
                    StorePtr (l, lc->Right, lrl);
		    StorePtr (lrl, lrlc->Father, l);
                } else
                    StoreNullPtr (l, lc->Right);

		StorePtr (object, c->Father, lr);
		c->Balance = (lrc->Balance == -1) ? 1 : 0;

		if (lrr) {
		    StorePtr (object, c->Left, lrr);
		    StorePtr (lrr, lrrc->Father, object);
                } else
		    StoreNullPtr (object, c->Left);

		lrc->Balance = 0;
		StorePtr (lr, lrc->Left, l);
		StorePtr (lr, lrc->Right, object);
		object = lr;
		if (father) {
		    StorePtr (lr, lrc->Father, father);
		    if (fdir == -1) StorePtr (father, fc->Left, lr);
		    else            StorePtr (father, fc->Right, lr);
		} else
		    StoreNullPtr (lr, lrc->Father);
		--delta;
	    } else {
		fprintf (stderr, CORRUPT, l, lc->Balance);
		exit (12);
	    }
	} else if (c->Balance == 2) {
	    r = LoadPtr (object, c->Right);
	    rc = MkConn (r, bcb);
	    if ((rl = LoadAnyPtr (r, rc->Left)) != NULL)
                rlc = MkConn (rl, bcb);

	    if (rc->Balance == 1) {
		StorePtr (object, c->Father, r);
		c->Balance = 0;
		if (rl) {
		    StorePtr (object, c->Right, rl);
		    StorePtr (rl, rlc->Father, object);
		} else
		    StoreNullPtr (object, c->Right);
		rc->Balance = 0;
		StorePtr (r, rc->Left, object);
		object = r;
		if (father) {
		    StorePtr (r, rc->Father, father);
		    if (fdir == -1) StorePtr (father, fc->Left, r);
		    else            StorePtr (father, fc->Right, r);
		} else
		    StoreNullPtr (r, rc->Father);
		--delta;

	    } else if (rc->Balance == 0) {
		StorePtr (object, c->Father, r);
		c->Balance = 1;
		StorePtr (object, c->Right, rl);
		StorePtr (rl, rlc->Father, object);
		rc->Balance = -1;
		StorePtr (r, rc->Left, object);
		object = r;
		if (father) {
		    StorePtr (r, rc->Father, father);
		    if (fdir == -1) StorePtr (father, fc->Left, r);
		    else            StorePtr (father, fc->Right, r);
                } else
		    StoreNullPtr (r, rc->Father);
		    
	    } else if (rc->Balance == -1) {
	        if ((rll = LoadAnyPtr (rl, rlc->Left)) != NULL)
		    rllc = MkConn (rll, bcb);
		if ((rlr = LoadAnyPtr (rl, rlc->Right)) != NULL)
                    rlrc = MkConn (rlr, bcb);

		StorePtr (r, rc->Father, rl);
		rc->Balance = (rlc->Balance == -1) ? 1 : 0;

		if (rlr) {
  		    StorePtr (r, rc->Left, rlr);
		    StorePtr (rlr, rlrc->Father, r);
		} else
		    StoreNullPtr (r, rc->Left);

		StorePtr (object, c->Father, rl);
		c->Balance = (rlc->Balance == 1) ? -1 : 0;

		if (rll) {
		    StorePtr (object, c->Right, rll);
		    StorePtr (rll, rllc->Father, object);
                } else
                    StoreNullPtr (object, c->Right);

		rlc->Balance = 0;
		StorePtr (rl, rlc->Left, object);
		StorePtr (rl, rlc->Right, r);
		object = rl;
		if (father) {
		    StorePtr (rl, rlc->Father, father);
		    if (fdir == -1) StorePtr (father, fc->Left, rl);
		    else            StorePtr (father, fc->Right, rl);
		} else
		    StoreNullPtr (rl, rlc->Father);
		--delta;
	    } else {
		fprintf (stderr, CORRUPT, r, rc->Balance);
		exit (12);
	    }
	} else if (c->Balance < -2 || c->Balance > 2) {
	    fprintf (stderr, CORRUPT, object, c->Balance);
	    exit (12);
	}
	if (delta == 0 || father == NULL)
	    leave = 1;
	else {
	    object = father;
	    dir = fdir;
	    c = fc;
	}
    } while (! leave);

    if (father == NULL)
       StorePtr (root, *root, object);
}

BABITED_OBJECT *BabitInsert (BABIT_ROOT *root, BABITED_OBJECT *object,
			     const BabitControlBlock *bcb)
{
    return BabitInsertU (root, object, bcb, bcb->fndextra);
}

BABITED_OBJECT *BabitInsertU (BABIT_ROOT *root,
			      BABITED_OBJECT *object,
			      const BabitControlBlock *bcb,
			      BABIT_EXTRAPAR uspar)
{
    BABITED_OBJECT *p;
    BabitConnector *c, *pc;
    int cmp, leave;

    if (root==NULL || object==NULL) return NULL; /* error */

    c = MkConn (object,bcb);
    StoreNullPtr (object, c->Left);
    StoreNullPtr (object, c->Right);
    c->Balance = 0;

    if (IsNullPtr (root, *root)) {
	StorePtr (root, *root, object);
        StoreNullPtr (object, c->Father);    
	return NULL;                /* siker */
    }
    p = LoadPtr (root, *root);

    for (leave= 0; !leave;) {
	pc = MkConn (p, bcb);
	cmp = ((BabitCmpFunEx *)bcb->cmpfun) (object, p, uspar);
	if (cmp==0) {
	    leave= 1;           /* duplicate */

	} else if (cmp<0) {
	    if (IsNullPtr (p, pc->Left)) {
		StorePtr (p, pc->Left, object);
		StorePtr (object, c->Father, p);
		BabBalance (root, p, -1, 1, bcb);  /* baloldal hosszabb lett */
		p= NULL;            /* success */
		leave= 1;

	    } else
		p = LoadPtr (p, pc->Left);

	} else {
	    if (IsNullPtr (p, pc->Right)) {
		StorePtr (p, pc->Right, object);
		StorePtr (object, c->Father, p);
		BabBalance (root, p, 1, 1, bcb);   /* jobboldal hosszabb lett */
		p= NULL;            /* success */
		leave= 1;

	    } else
		p = LoadPtr (p, pc->Right);
	}
    }
    return p;
}

BABITED_OBJECT *BabitGetMin (const BABIT_ROOT *root, int dir,
			     const BabitControlBlock *bcb)
{
    const BabitConnector *c;
    const BABITED_OBJECT *rootp;

    if (root==NULL) return NULL; /* error */
    if (IsNullPtr (root, *root)) return NULL; /* empty */
    rootp = LoadPtr (root, *root);

    if (dir<0) {
        while (c = MkConn (rootp, bcb), ! IsNullPtr (rootp, c->Left)) 
	    rootp = LoadPtr (rootp, c->Left);

    } else if (dir>0) {
        while (c = MkConn (rootp, bcb), ! IsNullPtr (rootp, c->Right)) 
	    rootp = LoadPtr (rootp, c->Right);
    }
    return (BABITED_OBJECT *)rootp;
}

BABITED_OBJECT *BabitGetNext (const BABITED_OBJECT *object, int dir,
			      const BabitControlBlock *bcb)
{
    const BabitConnector *c, *sc;
    const BABITED_OBJECT *s;

    if (object == NULL) return NULL;

    c = MkConn (object, bcb);
    if (dir == -1) {
	if (! IsNullPtr (object, c->Left)) {
	    object = LoadPtr (object, c->Left);
	    while (c = MkConn (object, bcb), ! IsNullPtr (object, c->Right))
		object = LoadPtr (object, c->Right);
	} else do {
	    s = object;
	    sc = c;
	    if (IsNullPtr (s, sc->Father)) return NULL;
	    object = LoadPtr (s, sc->Father);
	    c = MkConn (object, bcb);
	} while (LoadPtr (object, c->Left) == s);
    } else {
	if (! IsNullPtr (object, c->Right)) {
	    object = LoadPtr (object, c->Right);
	    while (c = MkConn (object, bcb), ! IsNullPtr (object, c->Left))
		object = LoadPtr (object, c->Left);
	} else do {
	    s = object;
	    sc = c;
	    if (IsNullPtr (s, sc->Father)) return NULL;
	    object = LoadPtr (s, sc->Father);
	    c = MkConn (object, bcb);
	} while (LoadPtr (object, c->Right) == s);
    }
    return (BABITED_OBJECT *)object;
}

BABITED_OBJECT *BabitGetFather (const BABITED_OBJECT *object, int sel,
			        const BabitControlBlock *bcb)
{
    const BabitConnector *c;
    const BABITED_OBJECT *retobj;

    if (object == NULL) return NULL;
    c = MkConn (object, bcb);

    switch (sel) {
    case -1:
	retobj = LoadAnyPtr (object, c->Left);
	break;

    case 0:
        retobj = LoadAnyPtr (object, c->Father);
	break;
	
    case 1:
    	retobj = LoadAnyPtr (object, c->Right);
	break;

    default:
    	retobj = NULL;
	break;
    };
    return (BABITED_OBJECT *)retobj;
}

int BabitDelete (BABIT_ROOT *root, BABITED_OBJECT *object,
		 const BabitControlBlock *bcb)
{
    BabitConnector *c, *fc, *swapc, *swapfc;
    BABITED_OBJECT *father, *r, *l, *child,
                   *swap, *swapf, *swapl, *swapr;
    int side;

    if (root == NULL || object == NULL) return -1; /* error */
    if (IsNullPtr (root, *root)) return -1; /* empty */
    c = MkConn (object, bcb);

    if (IsNullPtr (object, c->Father)) {
        father = NULL;
	fc = NULL;
	side = 0;
    } else {
        father = LoadPtr (object, c->Father);
	fc = MkConn (father, bcb);
	if (object == LoadPtr (father, fc->Left)) side = -1;
	else                                      side = 1;
    }

    l = LoadAnyPtr (object, c->Left);
    r = LoadAnyPtr (object, c->Right);

    if (l != NULL && r != NULL) {
	if (c->Balance <= 0) {
	    swap = l;
	    while (swapc = MkConn (swap, bcb), 
	           ! IsNullPtr (swap, swapc->Right))
		swap = LoadPtr (swap, swapc->Right);
	} else {
	    swap = r;
	    while (swapc = MkConn (swap, bcb), 
	           ! IsNullPtr (swap, swapc->Left))
		swap = LoadPtr (swap, swapc->Left);
	}

	if (! IsNullPtr (swap, swapc->Left)) {
            swapl = LoadPtr (swap, swapc->Left);
            StorePtr (object, c->Left, swapl);
	    StorePtr (swapl, MkConn (swapl, bcb) ->Father, object);
        } else {
            swapl = NULL;
            StoreNullPtr (object, c->Left);
        }

	if (! IsNullPtr (swap, swapc->Right)) {
            swapr = LoadPtr (swap, swapc->Right);
	    StorePtr (object, c->Right, swapr);
	    StorePtr (swapr, MkConn (swapr, bcb) ->Father, object);
        } else {
            swapr = NULL;
	    StoreNullPtr (object, c->Right);
        }

	if (l == swap) {
	    StorePtr (swap, swapc->Left, object);
	    StorePtr (object, c->Father, swap);
	    StorePtr (swap, swapc->Right, r);
	    StorePtr (r, MkConn (r, bcb)->Father, swap);
	} else if (r == swap) {
	    StorePtr (swap, swapc->Left, l);
	    StorePtr (l, MkConn (l, bcb)->Father, swap);
	    StorePtr (swap, swapc->Right, object);
	    StorePtr (object, c->Father, swap);
	} else {
	    StorePtr (swap, swapc->Left, l);
	    StorePtr (l, MkConn (l, bcb)->Father, swap);
	    StorePtr (swap, swapc->Right, r);
	    StorePtr (r, MkConn (r, bcb)->Father, swap);
	    swapf = LoadPtr (swap, swapc->Father);
	    swapfc = MkConn (swapf, bcb);
	    StorePtr (object, c->Father, swapf);
	    if (LoadPtr (swapf, swapfc->Left) == swap) 
	        StorePtr (swapf, swapfc->Left, object);
	    else                      
	        StorePtr (swapf, swapfc->Right, object);
	}

	if (father) {
	    StorePtr (swap, swapc->Father, father);
	    if (side == -1) StorePtr (father, fc->Left, swap);
	    else            StorePtr (father, fc->Right, swap);
	} else {
	    StoreNullPtr (swap, swapc->Father);
            StorePtr (root, *root, swap);
	}

	swapc->Balance = c->Balance;

	l = swapl;
	r = swapr;
	father = LoadPtr (object, c->Father);
	fc = MkConn (father, bcb);
	if (object == LoadPtr (father, fc->Left)) side = -1;
	else                                      side = 1;
    }

    if (l) child = l; 
    else child = r;
    if (child) {
        StoreAnyPtr (child, MkConn (child, bcb)->Father, father);
    }

    if (father) {
  	if (side == -1) StoreAnyPtr (father, fc->Left, child);
	else            StoreAnyPtr (father, fc->Right, child);
	BabBalance (root, father, side, -1, bcb); /* Rovidebb lett */
    } else
	StoreAnyPtr (root, *root, child);

    StorePtr (object, c->Left, NULL);
    StorePtr (object, c->Right, NULL);
    StorePtr (object, c->Father, NULL);
    c->Balance = 0;

    return 0;
}

int BabitReplace (BABIT_ROOT *root,
		  BABITED_OBJECT *old, BABITED_OBJECT *bnew,
		  const BabitControlBlock *bcb)
{
    return BabitReplaceU (root, old, bnew, bcb, bcb->fndextra);
}

int BabitReplaceU (BABIT_ROOT *root,
		   BABITED_OBJECT *old, BABITED_OBJECT *bnew,
		   const BabitControlBlock *bcb,
	           BABIT_EXTRAPAR uspar)
{
    BabitConnector *oldc, *newc, *fc, *lc, *rc;
    BABITED_OBJECT *f, *l, *r;

    if (bcb==NULL || root==NULL || IsNullPtr (root, *root) ||
	old==NULL || bnew==NULL || old == bnew ||
	((BabitCmpFunEx *)bcb->cmpfun) (old, bnew, uspar) != 0)
	return -1;
    oldc = MkConn (old, bcb);
    newc = MkConn (bnew, bcb);

    newc->Balance = oldc->Balance;
    oldc->Balance = 0;

    if (! IsNullPtr (old, oldc->Father)) {
        f = LoadPtr (old, oldc->Father);
	fc = MkConn (f, bcb);
	if (LoadPtr (f, fc->Left) == old) StorePtr (f, fc->Left,  bnew);
	else                              StorePtr (f, fc->Right, bnew);
	StorePtr (bnew, newc->Father, f);
	StoreNullPtr (old, oldc->Father);
    } else {
	StorePtr (root, *root, bnew);
	StoreNullPtr (bnew, newc->Father);
    }

    if (! IsNullPtr (old, oldc->Left)) {
        l = LoadPtr (old, oldc->Left);
	lc = MkConn (l, bcb);
	StorePtr (l, lc->Father, bnew);
        StorePtr (bnew, newc->Left, l);
        StoreNullPtr (oldc, oldc->Left);
    } else
        StoreNullPtr (newc, newc->Left);

    if (! IsNullPtr (old, oldc->Right)) {
        r = LoadPtr (old, oldc->Right);
	rc = MkConn (r, bcb);
	StorePtr (r, rc->Father, bnew);
        StorePtr (bnew, newc->Right, r);
        StoreNullPtr (old, oldc->Right);
    } else
        StoreNullPtr (bnew, newc->Right);
	
    return 0;
}

#define BABIT_WALK_FIRST 11

BABITED_OBJECT *BabitWalk (const BABIT_ROOT *rootp,
                           BabitWalkStruct *bw,
           	  	   const BabitControlBlock *bcb)
{
    BabitConnector *c, *fc;
    BABITED_OBJECT *obj, *f;
    int preo,ino,posto,leave;

    preo = bw->options & BABIT_WALK_PREORDER;
    ino = bw->options & BABIT_WALK_INORDER;
    posto = bw->options & BABIT_WALK_POSTORDER;

    leave = 0;
    if (bw->first) {
        bw->first = 0;
	bw->depth = 0;
        obj = LoadAnyPtr (rootp, *rootp);
	if (obj) {
	    bw->dir = BABIT_WALK_FIRST;
	} else {
	    bw->dir = BABIT_WALK_EOF;
	    leave = 1;
	}
    } else {
        if (bw->dir == BABIT_WALK_EOF || bw->obj == 0) {
            bw->dir = BABIT_WALK_EOF;
	    obj = NULL;
	    leave = 1;
	} else
            obj = LoadAnyPtr (rootp, bw->obj);
    }

    c = MkConn (obj, bcb);
    while (! leave) {
	if (bw->dir == BABIT_WALK_FIRST) {
	    bw->dir = BABIT_WALK_ROOT;
	    
        } else if (bw->dir < BABIT_WALK_UL && c->Left) {
	    obj = LoadPtr (obj, c->Left);
	    c = MkConn (obj, bcb);
	    bw->dir = BABIT_WALK_DL;
	    ++bw->depth;

	} else if (bw->dir < BABIT_WALK_UR && c->Right) {
	    obj = LoadPtr (obj, c->Right);
	    c = MkConn (obj, bcb);
	    bw->dir = BABIT_WALK_DR;
	    ++bw->depth;

/* 2004-01-16: do not let depth became negative,
	       ie making partial-tree walk possible,
	       to implement function 'twalk'
-	} else if (c->Father) {
+	} else if (c->Father && bw->depth > 0) {
*/
	} else if (c->Father && bw->depth > 0) {
	    f = LoadPtr (obj, c->Father);
	    fc = MkConn (f, bcb);
	    if (LoadPtr (f, fc->Left) == obj) {
	        bw->dir = BABIT_WALK_UL;
	    } else {
	        bw->dir = BABIT_WALK_UR;
	    }
	    obj = f;
	    c = fc;
	    --bw->depth;

	} else {
	    obj = NULL;
	    bw->dir = BABIT_WALK_EOF;
	    bw->depth = 0;
	}
	if (obj == NULL) leave = 1;
	else {
	    c = MkConn (obj, bcb);
	    if (bw->dir < BABIT_WALK_UL)
	        leave = preo ||
		        (IsNullPtr (obj, c->Left) &&
	                 (ino || (IsNullPtr (obj, c->Right) && posto)));
	    else if (bw->dir == BABIT_WALK_UL)
	        leave = ino || (IsNullPtr (obj, c->Right) && posto);
	    else
	        leave = posto;
        }
    }
    StoreAnyPtr (rootp, bw->obj, obj);
    return obj;
}

int BabitDestroy (BABIT_ROOT *root,
                  BabitDestroyFun *destfun,
                  BABIT_EXTRAPAR extra,
		  const BabitControlBlock *bcb)
{
    BABITED_OBJECT *obj, *father;
    BabitConnector *c;

    if (bcb==NULL || root==NULL) return -1; /* error */

    obj = (BABITED_OBJECT *)BabitOffsPtr (root, *root);
    if (obj==NULL) return 0;                /* empty */

    c = MkConn (obj, bcb);
    if (c->Father) return -1;		    /* not root */

    do {
	c = MkConn (obj, bcb);
	while (c->Left || c->Right) {
	    if (c->Left) {
		obj = (BABITED_OBJECT *)BabitOffsPtr (obj, c->Left);
	    	c->Left = 0;
	    } else {
            	obj = (BABITED_OBJECT *)BabitOffsPtr (obj, c->Right);
	    	c->Right = 0;
	    }
	    c = MkConn (obj, bcb);
	}
	father = (BABITED_OBJECT *)BabitOffsPtr (obj, c->Father);
	destfun (obj, extra);
	obj = father;
    } while (obj);

    *root = 0;
    return 0;
}
