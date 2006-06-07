/*
  nterfacer lua module
  Copyright (C) 2006 Chris Porter.
*/

#include "../lib/version.h"

#include "../lua/lua.h"

#include "../nterface/library.h"
#include "../nterface/nterfacer.h"

MODULE_VERSION("$Id")

#define ERR_SCRIPT_NOT_FOUND            0x01
#define ERR_SCRIPT_ERROR                0x02
#define ERR_NO_NTERFACER                0x03
#define ERR_SCRIPT_RETURN_ERROR         0x04
#define ERROR_SCRIPT_ERROR_OFFSET       1000

int handle_scriptcommand(struct rline *li, int argc, char **argv);

struct service_node *u_node;

void _init(void) {
  u_node = register_service("U");
  if(!u_node)
    return;

  register_handler(u_node, "scriptcommand", 2, handle_scriptcommand);
}

void _fini(void) {
  if(u_node)
    deregister_service(u_node);
}

int handle_scriptcommand(struct rline *li, int argc, char **argv) {
  lua_list *l2 = lua_scriptloaded(argv[0]);
  lua_State *l;
  int top;
  int iresult = 0;
  char *cresult;
  int ret, i;

  if(!l2)
    return ri_error(li, ERR_SCRIPT_NOT_FOUND, "Script not found.");

  l = l2->l;

  top = lua_gettop(l);

  lua_getglobal(l, "scripterror");
  lua_getglobal(l, "onnterfacer");
  if(!lua_isfunction(l, -1)) {
    lua_settop(l, top);
    return ri_error(li, ERR_NO_NTERFACER, "No nterfacer handler exists for this script.");
  }

  for(i=1;i<argc;i++)
    lua_pushstring(l, argv[i]);

  if(lua_debugpcall(l, "onnterfacer", argc - 1, 2, top + 1)) {
    ret = ri_error(li, ERR_SCRIPT_ERROR, "Script error: %s", lua_tostring(l, -1));
    lua_settop(l, top);
    return ret;
  }

  if(!lua_isint(l, -1) || !lua_isstring(l, -2)) {
    lua_settop(l, top);
    return ri_error(li, ERR_SCRIPT_RETURN_ERROR, "Script didn't return expected values.");
  }

  iresult = lua_toint(l, -2);
  cresult = (char *)lua_tostring(l, -1);
  if(iresult) {
    ret = ri_error(li, iresult + ERROR_SCRIPT_ERROR_OFFSET, "Script error: %s", cresult);
    lua_settop(l, top);
    return ret;
  }

  ret = ri_append(li, "%s", cresult);
  lua_settop(l, top);

  if(ret == BF_OVER)
    return ri_error(li, BF_OVER, "Buffer overflow");

  return ri_final(li);
}
