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

#define CLEANUP_KEEP 10 /* keep this many topics and kicks per channel around */
#define CLEANUP_INTERVAL 84600 /* db cleanup interval (in seconds) */
#define CLEANUP_INACTIVE_DAYS 30 /* disable channels where nothing happened for this many days */
#define CLEANUP_DELETE_DAYS 5 /* delete data for channels that have been disabled for this many days */

MODULE_VERSION("");

DBAPIConn *a4statsdb;

static int a4stats_connectdb(void) {
  if(!a4statsdb) {
    a4statsdb = dbapi2open("pqsql", "a4stats");
    if(!a4statsdb) {
      Error("a4stats", ERR_WARNING, "Unable to connect to db -- not loaded.");
      return 0;
    }
  }

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (id SERIAL PRIMARY KEY, name VARCHAR(256) UNIQUE, timestamp INT DEFAULT 0, active INT DEFAULT 1, deleted INT DEFAULT 0, privacy INT DEFAULT 2, "
    "h0 INT DEFAULT 0, h1 INT DEFAULT 0, h2 INT DEFAULT 0, h3 INT DEFAULT 0, h4 INT DEFAULT 0, h5 INT DEFAULT 0, "
    "h6 INT DEFAULT 0, h7 INT DEFAULT 0, h8 INT DEFAULT 0, h9 INT DEFAULT 0, h10 INT DEFAULT 0, h11 INT DEFAULT 0, "
    "h12 INT DEFAULT 0, h13 INT DEFAULT 0, h14 INT DEFAULT 0, h15 INT DEFAULT 0, h16 INT DEFAULT 0, h17 INT DEFAULT 0, "
    "h18 INT DEFAULT 0, h19 INT DEFAULT 0, h20 INT DEFAULT 0, h21 INT DEFAULT 0, h22 INT DEFAULT 0, h23 INT DEFAULT 0)", "T", "channels");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channelid INT, kicker VARCHAR(128), kickerid INT, victim VARCHAR(128), victimid INT, timestamp INT, reason VARCHAR(256),"
    "FOREIGN KEY (channelid) REFERENCES ? (id) ON DELETE CASCADE)", "TT", "kicks", "channels");

  a4statsdb->squery(a4statsdb, "CREATE INDEX kicks_channelid_index ON ? (channelid)", "T", "kicks");
  a4statsdb->squery(a4statsdb, "CREATE INDEX kicks_timestamp_index ON ? (timestamp)", "T", "kicks");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channelid INT, setby VARCHAR(128), setbyid INT, timestamp INT, topic VARCHAR(512),"
    "FOREIGN KEY (channelid) REFERENCES ? (id) ON DELETE CASCADE)", "TT", "topics", "channels");

  a4statsdb->squery(a4statsdb, "CREATE INDEX topics_channelid_index ON ? (channelid)", "T", "topics");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channelid INT, account VARCHAR(128), accountid INT, seen INT DEFAULT 0, rating INT DEFAULT 0, lines INT DEFAULT 0, chars INT DEFAULT 0, words INT DEFAULT 0, "
    "h0 INT DEFAULT 0, h1 INT DEFAULT 0, h2 INT DEFAULT 0, h3 INT DEFAULT 0, h4 INT DEFAULT 0, h5 INT DEFAULT 0, "
    "h6 INT DEFAULT 0, h7 INT DEFAULT 0, h8 INT DEFAULT 0, h9 INT DEFAULT 0, h10 INT DEFAULT 0, h11 INT DEFAULT 0, "
    "h12 INT DEFAULT 0, h13 INT DEFAULT 0, h14 INT DEFAULT 0, h15 INT DEFAULT 0, h16 INT DEFAULT 0, h17 INT DEFAULT 0, "
    "h18 INT DEFAULT 0, h19 INT DEFAULT 0, h20 INT DEFAULT 0, h21 INT DEFAULT 0, h22 INT DEFAULT 0, h23 INT DEFAULT 0, "
    "last VARCHAR(512), quote VARCHAR(512), quotereset INT DEFAULT 0, mood_happy INT DEFAULT 0, mood_sad INT DEFAULT 0, questions INT DEFAULT 0, yelling INT DEFAULT 0, caps INT DEFAULT 0, "
    "slaps INT DEFAULT 0, slapped INT DEFAULT 0, highlights INT DEFAULT 0, kicks INT DEFAULT 0, kicked INT DEFAULT 0, ops INT DEFAULT 0, deops INT DEFAULT 0, actions INT DEFAULT 0, skitzo INT DEFAULT 0, foul INT DEFAULT 0, "
    "firstseen INT DEFAULT 0, curnick VARCHAR(16), FOREIGN KEY (channelid) REFERENCES ? (id) ON DELETE CASCADE)", "TT", "users", "channels");

  a4statsdb->squery(a4statsdb, "CREATE INDEX users_account_index ON ? (account)", "T", "users");
  a4statsdb->squery(a4statsdb, "CREATE INDEX users_accountid_index ON ? (accountid)", "T", "users");
  a4statsdb->squery(a4statsdb, "CREATE INDEX users_channelid_index ON ? (channelid)", "T", "users");
  a4statsdb->squery(a4statsdb, "CREATE UNIQUE INDEX users_channelid_account_accountid_index ON ? (channelid, account, accountid)", "T", "users");
  a4statsdb->squery(a4statsdb, "CREATE INDEX users_channelid_lines_index ON ? (channelid, lines)", "T", "users");

  a4statsdb->createtable(a4statsdb, NULL, NULL,
    "CREATE TABLE ? (channelid INT, first VARCHAR(128), firstid INT, second VARCHAR(128), secondid INT, seen INT, score INT DEFAULT 1,"
    "FOREIGN KEY (channelid) REFERENCES ? (id) ON DELETE CASCADE)", "TT", "relations", "channels");

  a4statsdb->squery(a4statsdb, "CREATE INDEX relations_channelid_index ON ? (channelid)", "T", "relations");
  a4statsdb->squery(a4statsdb, "CREATE INDEX relations_score_index ON ? (score)", "T", "relations");
 

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
    lua_vpcall(dci->interp, dci->callback, "llR", (long)seen, (long)quotereset, dci->uarg_index);
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

  if (uui->stage == 1 || (result != NULL && uui->stage == 3))
    a4statsdb->query(a4statsdb, a4stats_update_user_cb, uui, uui->update, "TUUs", "users", uui->channelid, uui->accountid, uui->account);
  else {
    if (result == NULL || result->affected > 0 || uui->stage == 4) {
      if (result == NULL || (result->affected == 0 && uui->stage == 4))
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

typedef struct relation_update_info {
  int stage;
  unsigned long channelid;
  char *first;
  unsigned long firstid;
  char *second;
  unsigned long secondid;
} relation_update_info;

static void a4stats_update_relation_cb(const struct DBAPIResult *result, void *uarg) {
  relation_update_info *rui = uarg;

  rui->stage++;

  if (rui->stage == 1) {
    a4statsdb->query(a4statsdb, a4stats_update_relation_cb, rui, "UPDATE ? SET score = score + 1, seen = ? "
      "WHERE channelid = ? AND first = ? AND firstid = ? AND second = ? AND secondid = ?",
      "TtUsUsU", "relations", time(NULL), rui->channelid, rui->first, rui->firstid, rui->second, rui->secondid);
    return;
  } else if (rui->stage == 2 && result && result->affected == 0) {
    a4statsdb->query(a4statsdb, a4stats_update_relation_cb, rui, "INSERT INTO ? (channelid, first, firstid, second, secondid, seen) VALUES (?, ?, ?, ?, ?, ?)",
      "TUsUsUt", "relations", rui->channelid, rui->first, rui->firstid, rui->second, rui->secondid, time(NULL));
    return;
  }

  if (!result || result->affected == 0)
    Error("a4stats", ERR_WARNING, "Unable to update relation.");

  free(rui->first);
  free(rui->second);
  free(rui);
}

static int a4stats_lua_update_relation(lua_State *ps) {
  const char *user1, *user2;
  unsigned long channelid, user1id, user2id;
  relation_update_info *rui;

  if (!lua_isnumber(ps, 1) || !lua_isstring(ps, 2) || !lua_isnumber(ps, 3) || !lua_isstring(ps, 4) || !lua_isnumber(ps, 5))
    LUA_RETURN(ps, LUA_FAIL);

  channelid = lua_tonumber(ps, 1);
  user1 = lua_tostring(ps, 2);
  user1id = lua_tonumber(ps, 3);
  user2 = lua_tostring(ps, 4);
  user2id = lua_tonumber(ps, 5);

  rui = malloc(sizeof(*rui));
  rui->stage = 0;
  rui->channelid = channelid;

  if (user1id < user2id || (user1id == user2id && strcmp(user1, user2) <= 0)) {
    rui->first = strdup(user1);
    rui->firstid = user1id;
    rui->second = strdup(user2);
    rui->secondid = user2id;
  } else {
    rui->first = strdup(user2);
    rui->firstid = user2id;
    rui->second = strdup(user1);
    rui->secondid = user1id;
  }

  a4stats_update_relation_cb(NULL, rui);

  LUA_RETURN(ps, LUA_OK);
}

static int a4stats_lua_add_line(lua_State *ps) {
  char query[256];
  const char *channel;
  int hour;

  if (!lua_isstring(ps, 1) || !lua_isnumber(ps, 2))
    LUA_RETURN(ps, LUA_FAIL);

  channel = lua_tostring(ps, 1);
  hour = lua_tonumber(ps, 2);

  snprintf(query, sizeof(query), "UPDATE ? SET h%d = h%d + 1 WHERE name = ?", hour, hour);

  a4statsdb->squery(a4statsdb, query, "Ts", "channels", channel);

  LUA_RETURN(ps, LUA_OK);
}

static struct {
  time_t start;
  unsigned long pending;
  unsigned long topicskicks;
  unsigned long disabled;
  unsigned long deleted;
} cleanupdata;

static void a4stats_cleanupdb_got_result(void) {
  if (!--cleanupdata.pending) {
    controlwall(NO_OPER, NL_CLEANUP, "CLEANUPA4STATS: Deleted %lu old topics and kicks. Disabled %lu inactive channels. Deleted data for %lu channels.",
        cleanupdata.topicskicks, cleanupdata.disabled, cleanupdata.deleted);
    cleanupdata.start = 0;
  }
}

static void a4stats_cleanupdb_cb_countrows(const struct DBAPIResult *result, void *arg) {
  unsigned long *counter = arg;

  if (result)
    *counter += result->affected;

  a4stats_cleanupdb_got_result();
}

static void a4stats_cleanupdb_cb_active(const struct DBAPIResult *result, void *null) {
  unsigned long channelid;
  time_t seen;
  
  if (result && result->success) {
    while (result->next(result)) {
      channelid = strtoul(result->get(result, 0), NULL, 10);
      seen = (time_t)strtoul(result->get(result, 2), NULL, 10);
      /* use channel enabling timestamp if there was never any event */
      if (!seen)
        seen = (time_t)strtoul(result->get(result, 1), NULL, 10);

      if (seen < cleanupdata.start - CLEANUP_INACTIVE_DAYS * 86400) {
        /* disable inactive channels */
        cleanupdata.pending++;
        a4statsdb->query(a4statsdb, a4stats_cleanupdb_cb_countrows, &cleanupdata.disabled,
            "UPDATE ? SET active = 0, deleted = ? WHERE id = ? AND active = 1",
            "TtU", "channels", cleanupdata.start, channelid);
      } else {
        /* cleanup old kicks/topics */
        cleanupdata.pending++;
        a4statsdb->query(a4statsdb, a4stats_cleanupdb_cb_countrows, &cleanupdata.topicskicks, 
            "DELETE FROM ? WHERE channelid = ? AND timestamp <= "
            "(SELECT timestamp FROM ? WHERE channelid = ? ORDER BY timestamp DESC OFFSET ? LIMIT 1)",
            "TUTUU", "kicks", channelid, "kicks", channelid, (unsigned long)CLEANUP_KEEP);
        cleanupdata.pending++;
        a4statsdb->query(a4statsdb, a4stats_cleanupdb_cb_countrows, &cleanupdata.topicskicks,
            "DELETE FROM ? WHERE channelid = ? AND timestamp <= "
            "(SELECT timestamp FROM ? WHERE channelid = ? ORDER BY timestamp DESC OFFSET ? LIMIT 1)",
            "TUTUU", "topics", channelid, "topics", channelid, (unsigned long)CLEANUP_KEEP);
      }
    }
  }
}

static void a4stats_cleanupdb(void *null) {
  controlwall(NO_OPER, NL_CLEANUP, "Starting a4stats_db cleanup.");

  if (cleanupdata.start != 0) {
    controlwall(NO_OPER, NL_CLEANUP, "a4stats cleanup already in progress.");
    return;
  }

  cleanupdata.start = time(NULL);
  cleanupdata.pending = 0;
  cleanupdata.topicskicks = 0;
  cleanupdata.disabled = 0;
  cleanupdata.deleted = 0;

  a4statsdb->query(a4statsdb, a4stats_cleanupdb_cb_active, NULL,
      "SELECT id, timestamp, MAX(users.seen) FROM ? LEFT JOIN ? AS users ON id = users.channelid WHERE active = 1 GROUP BY id",
      "TT", "channels", "users");
  cleanupdata.pending++;
  a4statsdb->query(a4statsdb, a4stats_cleanupdb_cb_countrows, &cleanupdata.deleted,
      "DELETE FROM ? WHERE active = 0 AND deleted < ?", "Tt", "channels", (time_t)(cleanupdata.start - CLEANUP_DELETE_DAYS * 84600));
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
          lua_vpcall(dci->interp, dci->callback, "lsiR", channelid, channel, active, dci->uarg_index);
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

static int a4stats_lua_enable_channel(lua_State *ps) {
  if (!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  a4statsdb->squery(a4statsdb, "INSERT INTO ? (name, timestamp) VALUES (?, ?)", "Tst", "channels", lua_tostring(ps, 1), time(NULL));
  a4statsdb->squery(a4statsdb, "UPDATE ? SET active = 1, deleted = 0 WHERE name = ?", "Ts", "channels", lua_tostring(ps, 1));

  LUA_RETURN(ps, LUA_OK);
}

static int a4stats_lua_disable_channel(lua_State *ps) {
  if (!lua_isstring(ps, 1))
    LUA_RETURN(ps, LUA_FAIL);

  a4statsdb->squery(a4statsdb, "UPDATE ? SET active = 0, deleted = ? WHERE name = ?", "Tts", "channels", time(NULL), lua_tostring(ps, 1));

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

  lua_register(l, "a4_enable_channel", a4stats_lua_enable_channel);
  lua_register(l, "a4_disable_channel", a4stats_lua_disable_channel);
  lua_register(l, "a4_fetch_channels", a4stats_lua_fetch_channels);
  lua_register(l, "a4_add_kick", a4stats_lua_add_kick);
  lua_register(l, "a4_add_topic", a4stats_lua_add_topic);
  lua_register(l, "a4_add_line", a4stats_lua_add_line);
  lua_register(l, "a4_fetch_user", a4stats_lua_fetch_user);
  lua_register(l, "a4_update_user", a4stats_lua_update_user);
  lua_register(l, "a4_update_relation", a4stats_lua_update_relation);
  lua_register(l, "a4_escape_string", a4stats_lua_escape_string);
}

#define lua_unregister(L, n) (lua_pushnil(L), lua_setglobal(L, n))

static void a4stats_hook_unloadscript(int hooknum, void *arg) {
  db_callback_info **pnext, *dci;
  lua_State *l = arg;

  for (pnext = &dci_head; *pnext; pnext = &((*pnext)->next)) {
    dci = *pnext;
    if (dci->interp == l) {
      *pnext = dci->next;
      free(dci);
    }
  }
}

void _init(void) {
  lua_list *l;
  void *args[2];

  a4stats_connectdb();

  registerhook(HOOK_LUA_LOADSCRIPT, a4stats_hook_loadscript);
  registerhook(HOOK_LUA_UNLOADSCRIPT, a4stats_hook_unloadscript);
  schedulerecurring(time(NULL), 0, CLEANUP_INTERVAL, a4stats_cleanupdb, NULL);

  args[0] = NULL;
  for (l = lua_head; l;l = l->next) {
    args[1] = l->l;
    a4stats_hook_loadscript(HOOK_LUA_LOADSCRIPT, args);
  }
}

void _fini(void) {
  lua_list *l;

  deleteschedule(NULL, a4stats_cleanupdb, NULL);
  a4stats_closedb();

  for (l = lua_head; l;l = l->next) {
    a4stats_hook_unloadscript(HOOK_LUA_UNLOADSCRIPT, l->l);

    lua_unregister(l->l, "a4_enable_channel");
    lua_unregister(l->l, "a4_disable_channel");
    lua_unregister(l->l, "a4_fetch_channels");
    lua_unregister(l->l, "a4_add_kick");
    lua_unregister(l->l, "a4_add_topic");
    lua_unregister(l->l, "a4_add_line");
    lua_unregister(l->l, "a4_fetch_user");
    lua_unregister(l->l, "a4_update_user");
    lua_unregister(l->l, "a4_update_relation");
    lua_unregister(l->l, "a4_escape_string");
  }

  deregisterhook(HOOK_LUA_LOADSCRIPT, a4stats_hook_loadscript);
  deregisterhook(HOOK_LUA_UNLOADSCRIPT, a4stats_hook_unloadscript);
}

