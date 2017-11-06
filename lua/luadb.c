#include "lua.h"
#include "luabot.h"

#include "../dbapi2/dbapi2.h"

static DBAPIConn *luadb;

typedef struct lua_callback {
  lua_list *l;
  long handler, args;
} lua_callback;

static int lua_dbcreatequery(lua_State *ps) {
  char *s = (char *)lua_tostring(ps, 1);

  if(!luadb)
    LUA_RETURN(ps, LUA_FAIL);

  if(!s)
    LUA_RETURN(ps, LUA_FAIL);

  luadb->unsafecreatetable(luadb, NULL, NULL, "%s", s);
  LUA_RETURN(ps, LUA_OK);
}

static int lua_dbtablename(lua_State *ps) {
  char *s = (char *)lua_tostring(ps, 1);

  if(!luadb)
    LUA_RETURN(ps, LUA_FAIL);

  if(!s)
    LUA_RETURN(ps, LUA_FAIL);

  char *tablename = luadb->tablename(luadb, s);
  if(!tablename)
    LUA_RETURN(ps, LUA_FAIL);

  lua_pushstring(ps, tablename);
  return 1;
}

const static DBAPIResult *active_result;
static void lua_dbcallback(const DBAPIResult *result, void *tag) {
  lua_callback *c = (lua_callback *)tag;

  if(!lua_listexists(c->l)) {
    luafree(c);
    return;
  }

  active_result = result;
  _lua_vpcall(c->l->l, (void *)c->handler, LUA_POINTERMODE, "bR", result && result->success ? 1 : 0, c->args);
  active_result = NULL;

  if(result)
    result->clear(result);

  luaL_unref(c->l->l, LUA_REGISTRYINDEX, c->handler);
  luaL_unref(c->l->l, LUA_REGISTRYINDEX, c->args);
  luafree(c);
}

static int lua_dbnumfields(lua_State *ps) {
  if(!luadb)
    return 0;

  if(!active_result)
    return 0;

  lua_pushint(ps, active_result->fields);
  return 1;
}

static int lua_dbgetvalue(lua_State *ps) {
  if(!luadb)
    return 0;

  if(!active_result)
    return 0;

  if(!lua_isint(ps, 1))
    return 0;

  int field = lua_toint(ps, 1);
  if(field < 0 || field >= active_result->fields)
    return 0;

  char *value = active_result->get(active_result, field);
  /* bit rubbish, no way to get the length... binary data is out then */
  lua_pushstring(ps, value);

  return 1;
}

static int lua_dbnextrow(lua_State *ps) {
  if(!luadb)
    return 0;

  if(!active_result)
    return 0;

  int r = active_result->next(active_result);
  lua_pushboolean(ps, r?1:0);
  return 1;
}

static int lua_dbquery(lua_State *ps) {
  lua_list *l = lua_listfromstate(ps);
  char *q = (char *)lua_tostring(ps, 1);
  lua_callback *cb;

  if(!luadb)
    LUA_RETURN(ps, LUA_FAIL);

  /* what happens if 3 isn't there? */
  if(!l || !q)
    LUA_RETURN(ps, LUA_FAIL);

  if(!lua_isfunction(ps, 2)) {
    luadb->unsafesquery(luadb, "%s", q);
    LUA_RETURN(ps, LUA_OK);
  }

  cb = (lua_callback *)luamalloc(sizeof(lua_callback));
  if(!cb)
    LUA_RETURN(ps, LUA_FAIL);

  cb->l = l;
  cb->args = luaL_ref(ps, LUA_REGISTRYINDEX);
  cb->handler = luaL_ref(ps, LUA_REGISTRYINDEX);

  luadb->unsafequery(luadb, lua_dbcallback, cb, "%s", q);

  LUA_RETURN(ps, LUA_OK);
}

static int lua_dbescape(lua_State *ps) {
  char ebuf[8192 * 2 + 1];
  char *s = (char *)lua_tostring(ps, 1);
  int len;

  if(!luadb)
    return 0;

  if(!s)
    return 0;

  len = lua_strlen(ps, 1);
  if(len > sizeof(ebuf) / 2 - 5)
    return 0;

  luadb->escapestring(luadb, ebuf, s, len);
  lua_pushstring(ps, ebuf);

  return 1;
}

void lua_registerdbcommands(lua_State *l) {
  lua_register(l, "db_tablename", lua_dbtablename);

  lua_register(l, "db_createquery", lua_dbcreatequery);
  lua_register(l, "db_query", lua_dbquery);
  lua_register(l, "db_escape", lua_dbescape);

  lua_register(l, "db_numfields", lua_dbnumfields);
  lua_register(l, "db_getvalue", lua_dbgetvalue);
  lua_register(l, "db_nextrow", lua_dbnextrow);

  luadb = dbapi2open(NULL, "lua");

  /* TODO */
  /* open gets you a unique db */
  /* parameterised queries (huge pain due to no va_args in dbapi2) */
  /* loadtable */
  /* getrow */
}

void lua_destroydb(void) {
  if(luadb)
    luadb->close(luadb);
}
