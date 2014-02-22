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
    "CREATE TABLE ? (id INTEGER PRIMARY KEY AUTOINCREMENT, name VARCHAR(64) UNIQUE, active INT DEFAULT 1, privacy INT DEFAULT 1)", "T", "channels");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channelid INT, kicker VARCHAR(64), kickerid INT, victim VARCHAR(64), victimid INT, timestamp INT, reason VARCHAR(256))", "T", "kicks");

  a4statsdb->squery(a4statsdb, "CREATE INDEX ? ON kicks (channelid)", "T", "kicks_channelid_index");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channelid INT, setby VARCHAR(64), setbyid INT, timestamp INT, topic VARCHAR(512))", "T", "topics");

  a4statsdb->squery(a4statsdb, "CREATE INDEX ? ON topics (channelid)", "T", "topics_channelid_index");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channelid INT, account VARCHAR(64), accountid INT, seen INT DEFAULT 0, rating INT DEFAULT 0, lines INT DEFAULT 0, chars INT DEFAULT 0, words INT DEFAULT 0, "
    "h0 INT DEFAULT 0, h1 INT DEFAULT 0, h2 INT DEFAULT 0, h3 INT DEFAULT 0, h4 INT DEFAULT 0, h5 INT DEFAULT 0, "
    "h6 INT DEFAULT 0, h7 INT DEFAULT 0, h8 INT DEFAULT 0, h9 INT DEFAULT 0, h10 INT DEFAULT 0, h11 INT DEFAULT 0, "
    "h12 INT DEFAULT 0, h13 INT DEFAULT 0, h14 INT DEFAULT 0, h15 INT DEFAULT 0, h16 INT DEFAULT 0, h17 INT DEFAULT 0, "
    "h18 INT DEFAULT 0, h19 INT DEFAULT 0, h20 INT DEFAULT 0, h21 INT DEFAULT 0, h22 INT DEFAULT 0, h23 INT DEFAULT 0, "
    "last VARCHAR(512), quote VARCHAR(512), quotereset INT, mood_happy INT DEFAULT 0, mood_sad INT DEFAULT 0, questions INT DEFAULT 0, yelling INT DEFAULT 0, caps INT DEFAULT 0, "
    "slaps INT DEFAULT 0, slapped INT DEFAULT 0, highlights INT DEFAULT 0, kicks INT DEFAULT 0, kicked INT DEFAULT 0, ops INT DEFAULT 0, deops INT DEFAULT 0, actions INT DEFAULT 0, skitzo INT DEFAULT 0, foul INT DEFAULT 0, "
    "firstseen INT DEFAULT 0, curnick VARCHAR(16))", "T", "users");

  a4statsdb->squery(a4statsdb, "CREATE INDEX ? ON users (channelid)", "T", "users_channelid_index");
  a4statsdb->squery(a4statsdb, "CREATE UNIQUE INDEX ? ON users (channelid, account, accountid)", "T", "users_channelid_account_accountid_index");

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

typedef struct db_callback_info {
  struct db_callback_info *next;

  lua_State *interp;
  char callback[64];
  int uarg_index;
} db_callback_info;

static db_callback_info *dci_head;

static void a4stats_delete_dci(db_callback_info *dci) {
db_callback_info **pnext;

  for (pnext = &dci_head; *pnext; pnext = &((*pnext)->next)) {
    if (*pnext == dci) {
      *pnext = dci->next;
      break;
    }
  }

  free(dci);
}

static void a4stats_fetch_user_cb(const struct DBAPIResult *result, void *uarg) {
  db_callback_info *dci = uarg;
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

  if (dci->interp) {
    lua_vpcall(dci->interp, dci->callback, "llR", seen, quotereset, dci->uarg_index);
    luaL_unref(dci->interp, LUA_REGISTRYINDEX, dci->uarg_index);
  }

  a4stats_delete_dci(dci);
}

static int a4stats_lua_fetch_user(lua_State *ps) {
  const char *account, *callback;
  unsigned long channelid, accountid;
  db_callback_info *dci;

  if (!lua_islong(ps, 1) || !lua_isstring(ps, 2) || !lua_isnumber(ps, 3) || !lua_isstring(ps, 4))
    LUA_RETURN(ps, LUA_FAIL);

  channelid = lua_tonumber(ps, 1);
  account = lua_tostring(ps, 2);
  accountid = lua_tonumber(ps, 3);
  callback = lua_tostring(ps, 4);

  dci = malloc(sizeof(*dci));
  dci->interp = ps;

  strncpy(dci->callback, callback, sizeof(dci->callback));
  dci->callback[sizeof(dci->callback) - 1] = '\0';

  lua_pushvalue(ps, 5);
  dci->uarg_index = luaL_ref(ps, LUA_REGISTRYINDEX);

  a4statsdb->query(a4statsdb, a4stats_fetch_user_cb, dci, "SELECT seen, quotereset FROM ? WHERE channelid = ? AND (accountid != 0 AND accountid = ? OR accountid = 0 AND account = ?)", "TUUs", "users", channelid, accountid, account);

  LUA_RETURN(ps, LUA_OK);
}

typedef struct user_update_info {
  int stage;
  char *update;
  unsigned long channelid;
  char *account;
  unsigned long accountid;
} user_update_info;

static void a4stats_update_user_cb(const struct DBAPIResult *result, void *uarg) {
  user_update_info *uui = uarg;

  uui->stage++;

  if (uui->stage == 1 || uui->stage == 3)
    a4statsdb->query(a4statsdb, a4stats_update_user_cb, uui, uui->update, "TUUs", "users", uui->channelid, uui->accountid, uui->account);
  else {
    if (result->affected > 0 || uui->stage == 4) {
      if (result->affected == 0 && uui->stage == 4)
        Error("a4stats", ERR_WARNING, "Unable to update user.");

      free(uui->update);
      free(uui->account);
      free(uui);
      return;
    }

    a4statsdb->query(a4statsdb, a4stats_update_user_cb, uui, "INSERT INTO ? (channelid, account, accountid, firstseen) VALUES (?, ?, ?, ?)", "TUsUt", "users", uui->channelid, uui->account, uui->accountid, time(NULL));
  }
}

static int a4stats_lua_update_user(lua_State *ps) {
  const char *account;
  unsigned long channelid, accountid;
  char query[4096];
  int first = 1;
  user_update_info *uui;

  if (!lua_isnumber(ps, 1) || !lua_isstring(ps, 2) || !lua_isnumber(ps, 3))
    LUA_RETURN(ps, LUA_FAIL);

  channelid = lua_tonumber(ps, 1);
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

  strcat(query, " WHERE channelid = ? AND (accountid != 0 AND accountid = ? OR accountid = 0 AND account = ?)");

  uui = malloc(sizeof(*uui));
  uui->stage = 0;
  uui->update = strdup(query);
  uui->channelid = channelid;
  uui->account = strdup(account);
  uui->accountid = accountid;

  a4stats_update_user_cb(NULL, uui);

  LUA_RETURN(ps, LUA_OK);
}

static void a4stats_fetch_channels_cb(const struct DBAPIResult *result, void *uarg) {
  db_callback_info *dci = uarg;
  unsigned long channelid;
  int active;
  char *channel;

  if (result) {
    if (result->success) {
      while (result->next(result)) {
        channelid = strtoul(result->get(result, 0), NULL, 10);
        channel = result->get(result, 1);
        active = atoi(result->get(result, 2));

        if (dci->interp)
          lua_vpcall(dci->interp, dci->callback, "lslR", channelid, channel, active, dci->uarg_index);
      }
    }

    result->clear(result);
  }

  if (dci->interp)
    luaL_unref(dci->interp, LUA_REGISTRYINDEX, dci->uarg_index);

  a4stats_delete_dci(dci);
}

static int a4stats_lua_fetch_channels(lua_State *ps) {
  const char *callback;
  db_callback_info *dci;

  if (!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  callback = lua_tostring(ps, 1);

  dci = malloc(sizeof(*dci));
  dci->interp = ps;

  strncpy(dci->callback, callback, sizeof(dci->callback));
  dci->callback[sizeof(dci->callback) - 1] = '\0';

  lua_pushvalue(ps, 2);
  dci->uarg_index = luaL_ref(ps, LUA_REGISTRYINDEX);

  a4statsdb->query(a4statsdb, a4stats_fetch_channels_cb, dci, "SELECT id, name, active FROM ?", "T", "channels");

  LUA_RETURN(ps, LUA_OK);
}

static int a4stats_lua_add_channel(lua_State *ps) {
  if (!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  a4statsdb->squery(a4statsdb, "INSERT INTO ? (name) VALUES (?)", "Ts", "channels", lua_tostring(ps, 1));

  LUA_RETURN(ps, LUA_OK);
}

static int a4stats_lua_add_kick(lua_State *ps) {
  unsigned long channelid, kickerid, victimid;
  const char *kicker, *victim, *reason;

  if (!lua_isnumber(ps, 1) || !lua_isstring(ps, 2) || !lua_isnumber(ps, 3) || !lua_isstring(ps, 4) || !lua_isnumber(ps, 5) || !lua_isstring(ps, 6))
    LUA_RETURN(ps, LUA_FAIL);

  channelid = lua_tonumber(ps, 1);
  kicker = lua_tostring(ps, 2);
  kickerid = lua_tonumber(ps, 3);
  victim = lua_tostring(ps, 4);
  victimid = lua_tonumber(ps, 5);
  reason = lua_tostring(ps, 6);

  a4statsdb->squery(a4statsdb, "INSERT INTO ? (channelid, kicker, kickerid, victim, victimid, timestamp, reason) VALUES (?, ?, ?, ?, ?, ?, ?)", "TUsUsUts",
    "kicks", channelid, kicker, kickerid, victim, victimid, time(NULL), reason);

  LUA_RETURN(ps, LUA_OK);
}

static int a4stats_lua_add_topic(lua_State *ps) {
  unsigned long channelid, setbyid;
  const char *topic, *setby;

  if (!lua_isnumber(ps, 1) || !lua_isstring(ps, 2) || !lua_isstring(ps, 3) || !lua_isnumber(ps, 4))
    LUA_RETURN(ps, LUA_FAIL);

  channelid = lua_tonumber(ps, 1);
  topic = lua_tostring(ps, 2);
  setby = lua_tostring(ps, 3);
  setbyid = lua_tonumber(ps, 4);

  a4statsdb->squery(a4statsdb, "INSERT INTO ? (channelid, topic, timestamp, setby, setbyid) VALUES (?, ?, ?, ?, ?)", "TUstsU",
    "topics", channelid, topic, time(NULL), setby, setbyid);

  LUA_RETURN(ps, LUA_OK);
}

static void a4stats_hook_loadscript(int hooknum, void *arg) {
  void **args = arg;
  lua_State *l = args[1];

  lua_register(l, "a4_add_channel", a4stats_lua_add_channel);
  lua_register(l, "a4_fetch_channels", a4stats_lua_fetch_channels);
  lua_register(l, "a4_add_kick", a4stats_lua_add_kick);
  lua_register(l, "a4_add_topic", a4stats_lua_add_topic);
  lua_register(l, "a4_fetch_user", a4stats_lua_fetch_user);
  lua_register(l, "a4_update_user", a4stats_lua_update_user);
  lua_register(l, "a4_escape_string", a4stats_lua_escape_string);
}

#define lua_unregister(L, n) (lua_pushnil(L), lua_setglobal(L, n))

static void a4stats_hook_unloadscript(int hooknum, void *arg) {
  db_callback_info **pnext, *dci;
  lua_list *l;

  for (pnext = &dci_head; *pnext; pnext = &((*pnext)->next)) {
    dci = *pnext;
    if (dci->interp == arg) {
      *pnext = dci->next;
      free(dci);
    }
  }

  for (l = lua_head; l; l = l->next) {
    lua_unregister(l->l, "a4_add_channel");
    lua_unregister(l->l, "a4_fetch_channels");
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
  lua_list *l;

  a4stats_closedb();

  for (l = lua_head; l;l = l->next) {
    a4stats_hook_loadscript(HOOK_LUA_UNLOADSCRIPT, l->l);
  }

  deregisterhook(HOOK_LUA_LOADSCRIPT, a4stats_hook_loadscript);
  deregisterhook(HOOK_LUA_UNLOADSCRIPT, a4stats_hook_unloadscript);
}

