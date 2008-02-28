#include "../pqsql/pqsql.h"
#include "lua.h"
#include "luabot.h"

static int lua_dbcreatequery(lua_State *ps) {
  char *s = (char *)lua_tostring(ps, 1);

  if(!s)
    LUA_RETURN(ps, LUA_FAIL);

  pqcreatequery(s);
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
PGresult *pgres;

static int lua_dbnumfields(lua_State *ps) {
  lua_pushint(ps, PQnfields(pgres));

  return 1;
}

static int lua_dbnumrows(lua_State *ps) {
  lua_pushint(ps, PQntuples(pgres));

  return 1;
}

static int lua_dbgetvalue(lua_State *ps) {
  char *r;
  int tuple, field, len;

  if(!lua_isint(ps, 1) || !lua_isint(ps, 2))
    return 0;

  tuple = lua_toint(ps, 1);
  field = lua_toint(ps, 2);

  r = PQgetvalue(pgres, lua_toint(ps, 1), lua_toint(ps, 2));
  len = PQgetlength(pgres, tuple, field);

  lua_pushlstring(ps, r, len); /* safe?! */

  return 1;
}

void lua_dbcallback(PGconn *dbconn, void *arg) {
  pgres = PQgetResult(dbconn);
  lua_callback *c = (lua_callback *)arg;

  if(!lua_listexists(c->l)) {
    luafree(c);
    return;
  }

  if(PQresultStatus(pgres) != PGRES_TUPLES_OK) {
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
  PQclear(pgres);
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
    pqquery(q);
    LUA_RETURN(ps, LUA_OK);
  }

  cb = (lua_callback *)luamalloc(sizeof(lua_callback));
  if(!cb)
    LUA_RETURN(ps, LUA_FAIL);

  cb->l = l;
  cb->args = luaL_ref(ps, LUA_REGISTRYINDEX);
  cb->handler = luaL_ref(ps, LUA_REGISTRYINDEX);

  pqasyncquery(lua_dbcallback, cb, q);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_dbescape(lua_State *ps) {
  char ebuf[8192 * 2];
  char *s = (char *)lua_tostring(ps, 1);
  int len;
  size_t elen;

  if(!s)
    return 0;

  len = lua_strlen(ps, 1);
  if(len > sizeof(ebuf) / 2 - 5)
    return 0;

  elen = PQescapeString(ebuf, s, len);
  lua_pushlstring(ps, ebuf, elen);

  return 1;
}

void lua_registerdbcommands(lua_State *l) {
  lua_register(l, "db_createquery", lua_dbcreatequery);

/*   lua_register(l, "db_loadtable", lua_dbloadtable); */

  lua_register(l, "db_query", lua_dbquery);
  lua_register(l, "db_escape", lua_dbescape);
  lua_register(l, "db_numfields", lua_dbnumfields);
  lua_register(l, "db_numrows", lua_dbnumrows);
  lua_register(l, "db_getvalue", lua_dbgetvalue);
}
