/* envvars.h */

#ifndef ENVVARS_H
#define ENVVARS_H

/* a simple associative store for environment variables
   it is a data-type; constructor=EV_Init, destructor=EV_Destroy

   also there is a default store (DefaultEnvVars)
   don't call constructor/destructor on it
 */

typedef struct EnvVars EnvVars;

int EV_Init (EnvVars **evp);
int EV_Destroy (EnvVars *evp);

extern EnvVars *DefaultEnvVars;

int EV_Clear (EnvVars *evp);
int EV_Add  (EnvVars *evp, const char *name, const char *value, int freplace);
int EV_AddB (EnvVars *evp, const ConstBuffData *name, const ConstBuffData *value, int freplace);
/* function 'strdup' is called for 'key' and 'value' */
int EV_Del  (EnvVars *evp, const char *name);
int EV_DelB (EnvVars *evp, const ConstBuffData *bn);

int EV_IsEmpty (const EnvVars *evp);

int EV_Get (const EnvVars *from, int select, const char *ref,
	const char **pname, const char **pvalue);
/* 0/1 = ok/not found */

#define EVGET_FIRST	(-3)	/* name==minimum (ref is ignored) */
#define EVGET_LT	(-2)	/* name< ref */
#define EVGET_LE	(-1)	/* name<=ref */
#define EVGET_EQ	0	/* name==ref */
#define EVGET_GE	1	/* name>=ref */
#define EVGET_GT	2	/* name> ref */
#define EVGET_LAST	3	/* name==maximum (ref is ignored) */

/* a simple 'list all' operation can be performed via EVGET_FIRST and EVGET_GT */

int EV_AddFromIniStr (EnvVars *vars, const char *str);
/* example:
    VARNAME="VALUE" */

int EV_LoadFromIniFile (EnvVars *into, const char *filename, const char *sectionname);
/* default 'into': EV_Default
   default sectionname: "EnvVars"
*/

#endif
