/*
** $Id: ltm.h,v 2.6.1.1 2007/12/27 13:02:25 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_USEDINDEX,
  TM_GC,
  TM_MODE,
  TM_EQ,  /* last tag method with `fast' access */
  TM_ADD, /* keep TM_ADD up to TM_SHR aligned with TM_ADD_EQ to TM_SHR_EQ when adding operators */
  TM_SUB,
  TM_MUL,
  TM_DIV,
  TM_MOD,
  TM_POW,
  TM_AND,
  TM_OR,
  TM_XOR,
  TM_SHL,
  TM_SHR,
  TM_NOT,
  TM_UNM,
  TM_LEN,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_STRHOOK,
  TM_ADD_EQ,
  TM_SUB_EQ,
  TM_MUL_EQ,
  TM_DIV_EQ,
  TM_MOD_EQ,
  TM_POW_EQ,
  TM_AND_EQ,
  TM_OR_EQ,
  TM_XOR_EQ,
  TM_SHL_EQ,
  TM_SHR_EQ,
  TM_N		/* number of elements in the enum */
} TMS;



#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

LUAI_DATA const char *const luaT_typenames[];


LUAI_FUNC TValue *luaT_gettm (Table *events, TMS event, TString *ename);
LUAI_FUNC TValue *luaT_gettmbyobj (lua_State *L, TValue *o, TMS event);
LUAI_FUNC void luaT_init (lua_State *L);

#endif
