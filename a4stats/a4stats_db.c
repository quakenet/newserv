#include <stdio.h>
#include <stdarg.h>
#include "../lib/version.h"
#include "../dbapi2/dbapi2.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../control/control.h"
#include "../irc/irc.h"
#include "../lua/lua.h"

MODULE_VERSION("");

DBAPIConn *a4statsdb;

static int a4stats_connectdb(void) {
  if(!a4statsdb) {
    a4statsdb = dbapi2open(NULL, "a4stats");
    if(!a4statsdb) {
      Error("a4stats", ERR_WARNING, "Unable to connect to db -- not loaded.");
      return 0;
    }
  }

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channel VARCHAR(64), kicker VARCHAR(64), kickerid INT, victim VARCHAR(64), victimid INT, timestamp INT, reason VARCHAR(256))", "T", "kicks");

  a4statsdb->squery(a4statsdb, "CREATE INDEX ? ON kicks (channel)", "T", "kicks_channel_index");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channel VARCHAR(64), topic VARCHAR(512), timestamp INT, setby VARCHAR(64), setbyid INT)", "T", "topics");

  a4statsdb->squery(a4statsdb, "CREATE INDEX ? ON topics (channel)", "T", "topics_channel_index");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channel VARCHAR(64), account VARCHAR(64), accountid INT, seen INT DEFAULT 0, rating INT DEFAULT 0, lines INT DEFAULT 0, chars INT DEFAULT 0, words INT DEFAULT 0, "
    "h0 INT DEFAULT 0, h1 INT DEFAULT 0, h2 INT DEFAULT 0, h3 INT DEFAULT 0, h4 INT DEFAULT 0, h5 INT DEFAULT 0, h6 INT DEFAULT 0, h7 INT DEFAULT 0, h8 INT DEFAULT 0, h9 INT DEFAULT 0, h10 INT DEFAULT 0, h11 INT DEFAULT 0, "
    "h12 INT DEFAULT 0, h13 INT DEFAULT 0, h14 INT DEFAULT 0, h15 INT DEFAULT 0, h16 INT DEFAULT 0, h17 INT DEFAULT 0, h18 INT DEFAULT 0, h19 INT DEFAULT 0, h20 INT DEFAULT 0, h21 INT DEFAULT 0, h22 INT DEFAULT 0, h23 INT DEFAULT 0, "
    "last VARCHAR(512), quote VARCHAR(512), quotereset INT, mood_happy INT DEFAULT 0, mood_sad INT DEFAULT 0, questions INT DEFAULT 0, yelling INT DEFAULT 0, caps INT DEFAULT 0, "
    "slaps INT DEFAULT 0, slapped INT DEFAULT 0, highlights INT DEFAULT 0, kicks INT DEFAULT 0, kicked INT DEFAULT 0, ops INT DEFAULT 0, deops INT DEFAULT 0, actions INT DEFAULT 0, skitzo INT DEFAULT 0, foul INT DEFAULT 0, "
    "firstseen INT DEFAULT 0, curnick VARCHAR(16))", "T", "users");

  a4statsdb->squery(a4statsdb, "CREATE UNIQUE INDEX ? ON users (channel, account)", "T", "users_channel_account_index");

  return 1;
}

static void a4stats_closedb(void) {
  if(!a4statsdb)
    return;

  a4statsdb->close(a4statsdb);
  a4statsdb = NULL;
}

static int a4stats_lua_escape_string(lua_State *ps) {
  const char *input;
  char *buf, *buf2;
  size_t len, i, o;

  if (!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  input = lua_tostring(ps, 1);
  len = strlen(input);

  buf = malloc(len * 2 + 1);

  if (!buf)
    LUA_RETURN(ps, LUA_FAIL);

  a4statsdb->escapestring(a4statsdb, buf, input, len);

  buf2 = malloc(len * 4 + 1);

  if (!buf2) {
    free(buf);
    LUA_RETURN(ps, LUA_FAIL);
  }

  /* escape "?" */
  o = 0;
  for (i = 0; buf[i]; i++) {
    if (buf[i] == '?') {
      buf2[i + o] = '\\';
      o++;
    }

    buf2[i + o] = buf[i];
  }
  buf2[i + o] = '\0';

  free(buf);

  lua_pushstring(ps, buf2);

  free(buf2);

  return 1;
}

typedef struct fetch_user_info {
  struct fetch_user_info *next;

  lua_State *interp;
  char callback[64];
  int uarg_index;
} fetch_user_info;

static fetch_user_info *fui_head;

static void a4stats_fetch_user_cb(const struct DBAPIResult *result, void *uarg) {
  fetch_user_info **pnext, *fui = uarg;
  time_t seen = 0, quotereset = 0;

  if (result) {
    if (result->success) {
      while (result->next(result)) {
        seen = (result->get(result, 0)) ? (time_t)strtoul(result->get(result, 0), NULL, 10) : 0;
        quotereset = (result->get(result, 1)) ? (time_t)strtoul(result->get(result, 1), NULL, 10) : 0;
      }
    }

    result->clear(result);
  }

  if (fui->interp) {
    lua_vpcall(fui->interp, fui->callback, "llR", seen, quotereset, fui->uarg_index);
    luaL_unref(fui->interp, LUA_REGISTRYINDEX, fui->uarg_index);
  }

  for (pnext = &fui_head; *pnext; pnext = &((*pnext)->next)) {
    if (*pnext == fui) {
      *pnext = fui->next;
      break;
    }
  }

  free(fui);
}

static int a4stats_lua_fetch_user(lua_State *ps) {
  const char *channel, *account, *callback;
  unsigned long accountid;
  fetch_user_info *fui;

  if (!lua_isstring(ps, 1) || !lua_isstring(ps, 2) || !lua_isnumber(ps, 3) || !lua_isstring(ps, 4))
    LUA_RETURN(ps, LUA_FAIL);

  channel = lua_tostring(ps, 1);
  account = lua_tostring(ps, 2);
  accountid = lua_tonumber(ps, 3);
  callback = lua_tostring(ps, 4);

  fui = malloc(sizeof(*fui));
  fui->interp = ps;

  strncpy(fui->callback, callback, sizeof(fui->callback));
  fui->callback[sizeof(fui->callback) - 1] = '\0';

  lua_pushvalue(ps, 5);
  fui->uarg_index = luaL_ref(ps, LUA_REGISTRYINDEX);

  a4statsdb->query(a4statsdb, a4stats_fetch_user_cb, fui, "SELECT seen, quotereset FROM ? WHERE channel = ? AND (accountid != 0 AND accountid = ? OR accountid = 0 AND account = ?)", "TsUs", "users", channel, accountid, account);

  LUA_RETURN(ps, LUA_OK);
}

typedef struct user_update_info {
  int stage;
  char *update;
  char *channel;
  char *account;
  unsigned long accountid;
} user_update_info;

static void a4stats_update_user_cb(const struct DBAPIResult *result, void *uarg) {
  user_update_info *uui = uarg;

  uui->stage++;

  if (uui->stage == 1 || uui->stage == 3)
    a4statsdb->query(a4statsdb, a4stats_update_user_cb, uui, uui->update, "TsUs", "users", uui->channel, uui->accountid, uui->account);
  else {
    if (result->affected > 0 || uui->stage == 4) {
      if (result->affected == 0 && uui->stage == 4)
        Error("a4stats", ERR_WARNING, "Unable to update user.");

      free(uui->update);
      free(uui->channel);
      free(uui->account);
      free(uui);
      return;
    }

    a4statsdb->query(a4statsdb, a4stats_update_user_cb, uui, "INSERT INTO ? (channel, account, accountid, firstseen) VALUES (?, ?, ?, ?)", "TssUt", "users", uui->channel, uui->account, uui->accountid, time(NULL));
  }
}

static int a4stats_lua_update_user(lua_State *ps) {
  const char *channel, *account;
  unsigned long accountid;
  char query[4096];
  int first = 1;
  user_update_info *uui;

  if (!lua_isstring(ps, 1) || !lua_isstring(ps, 2) || !lua_isnumber(ps, 3))
    LUA_RETURN(ps, LUA_FAIL);

  channel = lua_tostring(ps, 1);
  account = lua_tostring(ps, 2);
  accountid = lua_tonumber(ps, 3);

  strcpy(query, "UPDATE ? SET ");

  lua_pushvalue(ps, 4);
  lua_pushnil(ps);

  while (lua_next(ps, -2)) {
    const char *value = lua_tostring(ps, -1);

    if (first)
      first = 0;
    else
      strcat(query, ", ");

    strcat(query, value);

    lua_pop(ps, 1);
  }

  lua_pop(ps, 1);

  strcat(query, " WHERE channel = ? AND (accountid != 0 AND accountid = ? OR accountid = 0 AND account = ?)");

  uui = malloc(sizeof(*uui));
  uui->stage = 0;
  uui->update = strdup(query);
  uui->channel = strdup(channel);
  uui->account = strdup(account);
  uui->accountid = accountid;

  a4stats_update_user_cb(NULL, uui);

  LUA_RETURN(ps, LUA_OK);
}

static int a4stats_lua_add_kick(lua_State *ps) {
  if (!lua_isstring(ps, 1) || !lua_isstring(ps, 2) || !lua_isnumber(ps, 3) || !lua_isstring(ps, 4) || !lua_isnumber(ps, 5) || !lua_isstring(ps, 6))
    LUA_RETURN(ps, LUA_FAIL);

  a4statsdb->squery(a4statsdb, "INSERT INTO ? (channel, kicker, kickerid, victim, victimid, timestamp, reason) VALUES (?, ?, ?, ?, ?, ?, ?)", "TssUsUts",
    "kicks", lua_tostring(ps, 1), lua_tostring(ps, 2), lua_tonumber(ps, 3), lua_tostring(ps, 4), lua_tonumber(ps, 5), time(NULL), lua_tostring(ps, 6));

  LUA_RETURN(ps, LUA_OK);
}

static int a4stats_lua_add_topic(lua_State *ps) {
  if (!lua_isstring(ps, 1) || !lua_isstring(ps, 2) || !lua_isstring(ps, 3) || !lua_isnumber(ps, 4))
    LUA_RETURN(ps, LUA_FAIL);

  a4statsdb->squery(a4statsdb, "INSERT INTO ? (channel, topic, timestamp, setby, setbyid) VALUES (?, ?, ?, ?, ?)", "TsstsU",
    "topics", lua_tostring(ps, 1), lua_tostring(ps, 2), time(NULL), lua_tostring(ps, 3), lua_tonumber(ps, 4));

  LUA_RETURN(ps, LUA_OK);
}

static void a4stats_hook_loadscript(int hooknum, void *arg) {
  void **args = arg;
  lua_State *l = args[1];

  lua_register(l, "a4_add_kick", a4stats_lua_add_kick);
  lua_register(l, "a4_add_topic", a4stats_lua_add_topic);
  lua_register(l, "a4_fetch_user", a4stats_lua_fetch_user);
  lua_register(l, "a4_update_user", a4stats_lua_update_user);
  lua_register(l, "a4_escape_string", a4stats_lua_escape_string);
}

#define lua_unregister(L, n) (lua_pushnil(L), lua_setglobal(L, n))

static void a4stats_hook_unloadscript(int hooknum, void *arg) {
  fetch_user_info **pnext, *fui;
  lua_list *l;

  for (pnext = &fui_head; *pnext; pnext = &((*pnext)->next)) {
    fui = *pnext;
    if (fui->interp == arg) {
      *pnext = fui->next;
      free(fui);
    }
  }

  for (l = lua_head; l; l = l->next) {
    lua_unregister(l->l, "a4_add_kick");
    lua_unregister(l->l, "a4_add_topic");
    lua_unregister(l->l, "a4_fetch_user");
    lua_unregister(l->l, "a4_update_user");
    lua_unregister(l->l, "a4_escape_string");
  }
}

void _init(void) {
  lua_list *l;
  void *args[2];

  a4stats_connectdb();

  registerhook(HOOK_LUA_LOADSCRIPT, a4stats_hook_loadscript);
  registerhook(HOOK_LUA_UNLOADSCRIPT, a4stats_hook_unloadscript);

  args[0] = NULL;
  for (l = lua_head; l;l = l->next) {
    args[1] = l->l;
    a4stats_hook_loadscript(HOOK_LUA_LOADSCRIPT, args);
  }
}

void _fini(void) {
  a4stats_closedb();

  deregisterhook(HOOK_LUA_LOADSCRIPT, a4stats_hook_loadscript);
  deregisterhook(HOOK_LUA_UNLOADSCRIPT, a4stats_hook_unloadscript);
}

