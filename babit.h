/* babit.h */

#ifndef BABIT_H_INCLUDED
#define BABIT_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* If you have only one 'Babited' object, you may
   predefine symbol 'BABITED_OBJECT'
   like this:

   struct YourOwnStruct;
   #define BABITED_OBJECT struct YourOwnStruct
   #include "babit.h"

   typedef struct YourOwnStruct {
       ...
       BabitConnector bc;
       ...
   } YourOwnStruct;
*/

#ifndef BABITED_OBJECT
#define BABITED_OBJECT void
#endif

/* You can predefine symbol BABIT_KEY before including babit.h */

#ifndef BABIT_KEY
#define BABIT_KEY void
#endif

#ifndef BABIT_OFFSET /* You can pre-define this to 'intptr_t' or sg else */
  #ifdef INTEL_8086_HUGE_LONG
    /* define this is you have 'far' pointers (segment:offset) on x86 
       (MSDOS, Windows16); here pointer-mathematic should be done via
       'huge' pointers and 'long' offsets */
    #define BABIT_OFFSET long
  #else
    #define BABIT_OFFSET ptrdiff_t
  #endif
#endif

typedef BABIT_OFFSET BABIT_ROOT;

/* You have to embed BabitConnector into your own data record.
   If you want to build two or more independent trees, you have to
   embed two or more BabitConnectors (with different field names).

   Note: you should not use fields of BabitConnector, but it is safe
   to compare fields Left, Right, Father fields to zero, to check
   the existence of Childs and Parent, 
   eg when Left==0 and Right==0 then it is a leaf,
   when father==0 it is the root element

   Note: long long time ago Left, Right and Father were pointers,
   but now they are just offsets, to enable special cases, like
   sharing memory between processes, or saving the whole tree to disk
   and then reload to a different location */

typedef struct BabitConnector {
	 BABIT_OFFSET Left;
	 BABIT_OFFSET Right;
	 BABIT_OFFSET Father;
	 int  Balance;
} BabitConnector;

#define EmptyBabitConnector {0,0,0,0}

/* some macros to help working with offsets: */

#ifdef INTEL_8086_HUGE_LONG
  #define BabitOffsPtr(base,off) ((off) ? (void *)((char huge *)(base)+(off)) : NULL)
  #define BabitPtrOffs(base,ptr) ((ptr) ? ((char huge *)(ptr)-(char huge *)(base)) : 0)
#else
  #define BabitOffsPtr(base,off) ((off) ? (void *)((char *)(base)+(off)) : NULL)
  #define BabitPtrOffs(base,ptr) ((ptr) ? ((char *)(ptr)-(char *)(base)) : 0)
#endif

#ifndef BABIT_EXTRAPAR
  #ifdef INTEL_8086_HUGE_LONG
    /* define this is you have 'far' pointers (segment:offset) on x86 
       (MSDOS, Windows16); here pointer-mathematic should be done via
       'huge' pointers and 'long' offsets */
    #define BABIT_EXTRAPAR long
  #else
    #define BABIT_EXTRAPAR ptrdiff_t
  #endif
#endif

/* BabitCmpFunEx and BabitFndFunEx have an extra parameter
   of type BABIT_EXTRAPAR from BabitControlBlock
   This is for special cases eg when implementing
   functions tsearch/tfind/tdelete. */

typedef int BabitCmpFun (const BABITED_OBJECT *obj1,
                         const BABITED_OBJECT *obj2);
typedef int BabitFndFun (const BABIT_KEY *key, 
                         const BABITED_OBJECT *obj);
typedef int BabitCmpFunEx (const BABITED_OBJECT *obj1,
			   const BABITED_OBJECT *obj2,
			   BABIT_EXTRAPAR);
typedef int BabitFndFunEx (const BABIT_KEY *key, 
                           const BABITED_OBJECT *obj,
			   BABIT_EXTRAPAR);

typedef struct BabitControlBlock {
         int options;            /* set this to zero */
	 unsigned offset;        /* offset of connector in structure */
	 BabitCmpFun *cmpfun;    /* compares two structures' key */
	 		         /* can be BabitCmpFun or BabitCmpFunEx */
	 BabitFndFun *fndfun;    /* comperes a key with a structure's key */
	 		         /* can be BabitFndFun or BabitFndFunEx */
	 BABIT_EXTRAPAR fndextra; /* this value is passed to */
	 			  /* cmpfun and fndfun as 3rd param */
	 			  /* 20150408: or use the new 'Babit*U' functions */
} BabitControlBlock;

#define EmptyBabitControlBlock {0,0,NULL,NULL,0}

#ifndef offsetof
#define offsetof(T,F) ((unsigned)&(((T *)0)->F))
#endif

#define BabitInit(rootp) (*(rootp) = 0)

#define BabitPtrToOffs(rootp,ptr) ((ptr) ? ((char *)(ptr)-(char *)(rootp)) : 0)
#define BabitOffsToPtr(rootp,off) ((BABITED_OBJECT *)\
                                  ((off) ? (char *)(rootp)+(off) : 0))

int BabitCheck (const BABIT_ROOT *root, int *dom, int *he,
		const BabitControlBlock *bcb);

BABITED_OBJECT *BabitInsert (BABIT_ROOT *root,
			     BABITED_OBJECT *object,
			     const BabitControlBlock *bcb);

int BabitDelete (BABIT_ROOT *root,
		 BABITED_OBJECT *object,
		 const BabitControlBlock *bcb);

BABITED_OBJECT *BabitFind (const BABIT_ROOT *root, const BABIT_KEY *key,
			   const BabitControlBlock *bcb);

/* FindExtended:
   returns an object for which fndfun (key, object)
   
    <  value   if option == BABIT_FIND_LT  (L)
    <= value   if option == BABIT_FIND_LE  (L)
    == value   if option == BABIT_FIND_EQ  (C)
    >= value   if option == BABIT_FIND_GE  (R)
    >  value   if option == BABIT_FIND_GT  (R)
    
    L: finds the leftmost (least) object for which the condition is true
    R: finds the rightmot (greatest) object for which the contidion is true
    C: finds the closest object for which the condition is true
*/    

#define BABIT_FIND_LT (-2)
#define BABIT_FIND_LE (-1)
#define BABIT_FIND_EQ   0
#define BABIT_FIND_GE   1
#define BABIT_FIND_GT   2

BABITED_OBJECT *BabitFindEx (const BABIT_ROOT *root,
			     const BABIT_KEY *key,
			     int option, int value,
			     const BabitControlBlock *bcb);

int BabitReplace (BABIT_ROOT *root,
		  BABITED_OBJECT *old, BABITED_OBJECT *bnew,
		  const BabitControlBlock *bcb);

#define BABIT_GET_MIN (-1)
#define BABIT_GET_MED   0
#define BABIT_GET_MAX   1

BABITED_OBJECT *BabitGetMin (const BABIT_ROOT *root,
			     int dir,
			     const BabitControlBlock *bcb);  
			
/* constans for BabitGetNext: */

#define BABIT_GET_NEXT 1
#define BABIT_GET_PREV (-1)

BABITED_OBJECT *BabitGetNext (const BABITED_OBJECT *object, int dir,
			      const BabitControlBlock *bcb);

/* constans for BabitGetFather: */

#define BABIT_GET_LEFT (-1)
#define BABIT_GET_FATHER 0
#define BABIT_GET_RIGHT  1
		
BABITED_OBJECT *BabitGetFather (const BABITED_OBJECT *object, int sel,
				const BabitControlBlock *bcb);

/* constans for BabitWalk: */

#define BABIT_WALK_PREORDER  1
#define BABIT_WALK_INORDER   2
#define BABIT_WALK_POSTORDER 4
#define BABIT_WALK_ALLORDER  7

#define BABIT_WALK_EOF -1
#define BABIT_WALK_ROOT 0 /* starting at root */
#define BABIT_WALK_DL   1 /* coming here from my father, I am the left child */
#define BABIT_WALK_DR   2 /* coming here from my father, I am the right child */
#define BABIT_WALK_UL   3 /* coming here from my left child */
#define BABIT_WALK_UR   4 /* coming here from my right child */

typedef struct BabitWalkStruct {
    int first;                    /* set to 1 on first call */
    int options;                  /* BABIT_WALK_xORDER */
    BABIT_OFFSET  obj;            /* output */
    int depth;			  /* output */
    int dir;                      /* output: BABIR_WALK_DL ... */
} BabitWalkStruct;

BABITED_OBJECT *BabitWalk (const BABIT_ROOT *root,
			   BabitWalkStruct *bw,
			   const BabitControlBlock *bcb);

/* 2004.01.19. BabitDestroy destroys a complete tree,
   with the user-defined deallocating function.
   This function gets the address of the object to release,
   and an extra-parameter, which comes from the caller of
   BabitDestroy.

   BabitDestroy returns non-zero if detects error,
   eg if the root element is not the 'real' root of the tree
   (ie 'Father' field is not zero)
*/

typedef void BabitDestroyFun (BABITED_OBJECT *obj, BABIT_EXTRAPAR extra);

int BabitDestroy (BABIT_ROOT *root,
		  BabitDestroyFun *destfun,
		  BABIT_EXTRAPAR extra,
                  const BabitControlBlock *bcb);

/* 20150408.LZS:
    reorganization:
	'userpar' should not be in ControlBlock, 
	so instead I add new functions
	that have a 'userpar' parameter (type: BABIT_EXTRAPAR)
	the old functions keep working
	(the other function don't use callbacks)
 */

int BabitCheckU (const BABIT_ROOT *root, int *dom, int *he,
		 const BabitControlBlock *bcb,
		 BABIT_EXTRAPAR uspar);

BABITED_OBJECT *BabitInsertU (BABIT_ROOT *root,
			      BABITED_OBJECT *object,
			      const BabitControlBlock *bcb,
			      BABIT_EXTRAPAR uspar);

BABITED_OBJECT *BabitFindU (const BABIT_ROOT *root, const BABIT_KEY *key,
			    const BabitControlBlock *bcb,
			    BABIT_EXTRAPAR uspar);

BABITED_OBJECT *BabitFindExU (const BABIT_ROOT *root,
			      const BABIT_KEY *key,
			      int option, int value,
			      const BabitControlBlock *bcb,
			      BABIT_EXTRAPAR uspar);

int BabitReplaceU (BABIT_ROOT *root,
		   BABITED_OBJECT *old, BABITED_OBJECT *bnew,
		   const BabitControlBlock *bcb,
		   BABIT_EXTRAPAR uspar);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
