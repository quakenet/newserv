#include "lua.h"
#include "luabot.h"

#include "../config.h"

#ifdef HAVE_PGSQL
#include "../dbapi/dbapi.h"

#warning Needs testing with new db API.

static int lua_dbcreatequery(lua_State *ps) {
  char *s = (char *)lua_tostring(ps, 1);

  if(!s)
    LUA_RETURN(ps, LUA_FAIL);

  dbcreatequery("%s", s);
  LUA_RETURN(ps, LUA_OK);
}

/* TODO */
/*
static int lua_dbloadtable(lua_State *ps) {
  lua_list *l = lua_listfromstate(ps);
  if(!l)
    LUA_RETURN(ps, LUA_FAIL);

}
*/

typedef struct lua_callback {
  lua_list *l;
  long handler, args;
} lua_callback;

/* hack */
DBResult *pgres;

static int lua_dbnumfields(lua_State *ps) {
  lua_pushint(ps, dbnumfields(pgres));

  return 1;
}

/* TODO */
/*
static int lua_dbgetrow(lua_State *ps) {
  char *r;
  int tuple, field, len;

  if(!lua_isint(ps, 1))
    return 0;

  lua_pushstring(ps, dbgetvalue(pgres, lua_toint(ps, 1)), len);

  return 1;
}
*/

void lua_dbcallback(DBConn *dbconn, void *arg) {
  pgres = dbgetresult(dbconn);
  lua_callback *c = (lua_callback *)arg;

  if(!lua_listexists(c->l)) {
    luafree(c);
    return;
  }

  if(!dbquerysuccessful(pgres)) {
    _lua_vpcall(c->l->l, (void *)c->handler, LUA_POINTERMODE, "bR", 0, c->args);

    luaL_unref(c->l->l, LUA_REGISTRYINDEX, c->handler);
    luaL_unref(c->l->l, LUA_REGISTRYINDEX, c->args);
    luafree(c);
    return;
  }

  _lua_vpcall(c->l->l, (void *)c->handler, LUA_POINTERMODE, "bR", 1, c->args);

  luaL_unref(c->l->l, LUA_REGISTRYINDEX, c->handler);
  luaL_unref(c->l->l, LUA_REGISTRYINDEX, c->args);
  luafree(c);
  dbclear(pgres);
  pgres = NULL;
}

static int lua_dbquery(lua_State *ps) {
  lua_list *l = lua_listfromstate(ps);
  char *q = (char *)lua_tostring(ps, 1);
  lua_callback *cb;

  /* what happens if 3 isn't there? */
  if(!l || !q)
    LUA_RETURN(ps, LUA_FAIL);

  if(!lua_isfunction(ps, 2)) {
    dbquery("%s", q);
    LUA_RETURN(ps, LUA_OK);
  }

  cb = (lua_callback *)luamalloc(sizeof(lua_callback));
  if(!cb)
    LUA_RETURN(ps, LUA_FAIL);

  cb->l = l;
  cb->args = luaL_ref(ps, LUA_REGISTRYINDEX);
  cb->handler = luaL_ref(ps, LUA_REGISTRYINDEX);

  dbasyncquery(lua_dbcallback, cb, "%s", q);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_dbescape(lua_State *ps) {
  char ebuf[8192 * 2 + 1];
  char *s = (char *)lua_tostring(ps, 1);
  int len;

  if(!s)
    return 0;

  len = lua_strlen(ps, 1);
  if(len > sizeof(ebuf) / 2 - 5)
    return 0;

  dbescapestring(ebuf, s, len);
  lua_pushstring(ps, ebuf);

  return 1;
}

void lua_registerdbcommands(lua_State *l) {
  lua_register(l, "db_createquery", lua_dbcreatequery);

/*   lua_register(l, "db_loadtable", lua_dbloadtable); */

  lua_register(l, "db_query", lua_dbquery);
  lua_register(l, "db_escape", lua_dbescape);
  lua_register(l, "db_numfields", lua_dbnumfields);
/*  lua_register(l, "db_getvalue", lua_dbgetvalue);*/
}

#else

void lua_registerdbcommands(lua_State *l) {
}

#endif
