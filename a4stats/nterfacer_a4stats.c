#include <stdio.h>
#include <stdarg.h>
#include "../lib/version.h"
#include "../core/error.h"
#include "../nterfacer/library.h"
#include "../nterfacer/nterfacer.h"
#include "a4stats.h"

MODULE_VERSION("");

static struct service_node *a4stats_node;

static void a4stats_nt_query_cb(const struct DBAPIResult *result, void *uarg) {
  struct rline *ri = uarg;
  int i;

  if (result) {
    ri_append(ri, "%s", result->success ? "OK" : "FAILED");

    if (result->success) {
      ri_append(ri, "%d", result->fields);

      while (result->next(result)) {
        for (i = 0; i < result->fields; i++) {
          const char *field = result->get(result, i);
          ri_append(ri, "%s", field ? field : "");
        }
      }
    }

    result->clear(result);
  } else
    ri_append(ri, "FAILED");

  ri_final(ri);
}

static int handle_getchannel(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT timestamp, active, privacy, deleted FROM ? WHERE name = ?", "Ts", "channels", argv[0]);
  return 0;
}

static int handle_getlines(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11,"
    "h12, h13, h14, h15, h16, h17, h18, h19, h20, h21, h22, h23 "
    "FROM ? WHERE name = ?", "Ts", "channels", argv[0]);

  return 0;
}

static int handle_getusers(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT users.account, users.accountid, users.seen, users.rating, users.lines, users.chars, users.words, users.h0, users.h1, users.h2, users.h3, users.h4, users.h5, users.h6, users.h7, users.h8, users.h9, users.h10, users.h11, users."
    "h12, users.h13, users.h14, users.h15, users.h16, users.h17, users.h18, users.h19, users.h20, users.h21, users.h22, users.h23, users."
    "last, users.quote, users.quotereset, users.mood_happy, users.mood_sad, users.questions, users.yelling, users.caps, users."
    "slaps, users.slapped, users.highlights, users.kicks, users.kicked, users.ops, users.deops, users.actions, users.skitzo, users.foul, users."
    "firstseen, users.curnick FROM ? LEFT JOIN ? ON channels.id = users.channelid WHERE channels.name = ? AND users.quote IS NOT NULL ORDER BY lines DESC LIMIT 25", "TTs", "users", "channels", argv[0]);

  return 0;
}

static int handle_getkicks(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT kicks.kicker, kicks.kickerid, ukicker.seen, ukicker.curnick, kicks.victim, kicks.victimid, uvictim.seen, uvictim.curnick, kicks.timestamp, kicks.reason "
    "FROM ? "
    "LEFT JOIN ? ON channels.id = kicks.channelid "
    "LEFT JOIN ? AS ukicker ON (ukicker.channelid = channels.id AND kicks.kickerid = ukicker.accountid AND kicks.kicker = ukicker.account) "
    "LEFT JOIN ? AS uvictim ON (uvictim.channelid = channels.id AND kicks.victimid = uvictim.accountid AND kicks.victim = uvictim.account) "
    "WHERE channels.name = ? ORDER BY kicks.timestamp DESC LIMIT 10", "TTTTs", "kicks", "channels", "users", "users", argv[0]);

  return 0;
}

static int handle_gettopics(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT topics.topic, topics.timestamp, topics.setby, topics.setbyid, users.seen, users.curnick "
    "FROM ? "
    "LEFT JOIN ? ON channels.id = topics.channelid "
    "LEFT JOIN ? ON (users.channelid = channels.id AND topics.setbyid = users.accountid AND topics.setby = users.account) "
    "WHERE channels.name = ? ORDER BY topics.timestamp DESC LIMIT 10", "TTTs", "topics", "channels", "users", argv[0]);

  return 0;
}

static int handle_getuser(struct rline *ri, int argc, char **argv) {
#define USER_QUERY(a, b) a "users.account, users.accountid, users.seen, users.rating, users.lines, users.chars, users.words, " \
                    "users.h0, users.h1, users.h2, users.h3, users.h4, users.h5, users.h6, users.h7, users.h8, users.h9, " \
                    "users.h10, users.h11, users.h12, users.h13, users.h14, users.h15, users.h16, users.h17, users.h18, " \
                    "users.h19, users.h20, users.h21, users.h22, users.h23, users.last, users.quote, users.quotereset, " \
                    "users.mood_happy, users.mood_sad, users.questions, users.yelling, users.caps, users.slaps, users.slapped, " \
                    "users.highlights, users.kicks, users.kicked, users.ops, users.deops, users.actions, users.skitzo, " \
                    "users.foul, users.firstseen, users.curnick" b

  /*
    Possible cases:
    accountid = 0, account = "username" -> new-style account, look up latest account for user
    accountid = 0, account = "#username" -> legacy account or user not authed, look up using "username" (remove #)
    accountid = <some value>, account = <unused> -> new-style account, look up by account id
  */

  if (argv[1][0] == '#') {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, USER_QUERY("SELECT ", " FROM ? LEFT JOIN ? ON channels.id = users.channelid WHERE channels.name = ? AND (users.accountid = 0 AND users.account = ?)"),
      "TTss", "users", "channels", argv[0], &(argv[1][1]));
  } else if (atoi(argv[2]) == 0) {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, USER_QUERY("SELECT ", " FROM ? LEFT JOIN ? ON channels.id = users.channelid WHERE channels.name = ? AND (users.accountid != 0 AND users.account = ?) ORDER BY users.accountid DESC LIMIT 1"),
      "TTss", "users", "channels", argv[0], argv[1]);
  } else {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, USER_QUERY("SELECT ", " FROM ? LEFT JOIN ? ON channels.id = users.channelid WHERE channels.name = ? AND users.accountid = ?"),
      "TTss", "users", "channels", argv[0], argv[2]);
  }

  return 0;
}

static int handle_getrelations(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT relations.first, relations.firstid, ufirst.curnick, ufirst.seen, relations.second, relations.secondid, usecond.curnick, usecond.seen, relations.seen, relations.score "
    "FROM ? "
    "LEFT JOIN ? ON relations.channelid = channels.id "
    "LEFT JOIN ? AS ufirst ON (ufirst.channelid = channels.id AND relations.firstid = ufirst.accountid AND relations.first = ufirst.account) "
    "LEFT JOIN ? AS usecond ON (usecond.channelid = channels.id AND relations.secondid = usecond.accountid AND relations.second = usecond.account) "
    "WHERE channels.name = ? ORDER BY score DESC LIMIT 25",
    "TTTTs", "relations", "channels", "users", "users", argv[0]);

  return 0;
}

static int handle_getuserrelations(struct rline *ri, int argc, char **argv) {
  /*
    Possible cases:
    accountid = 0, account = "username" -> new-style account, look up latest account for user
    accountid = 0, account = "#username" -> legacy account or user not authed, look up using "username" (remove #)
    accountid = <some value>, account = <unused> -> new-style account, look up by account id
  */

  if (argv[1][0] == '#') {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT relations.first, relations.firstid, ufirst.curnick, ufirst.seen, relations.second, relations.secondid, usecond.curnick, usecond.seen, relations.seen, relations.score "
      "FROM ? "
      "LEFT JOIN ? ON relations.channelid = channels.id "
      "LEFT JOIN ? AS ufirst ON (ufirst.channelid = channels.id AND relations.firstid = ufirst.accountid AND relations.first = ufirst.account) "
      "LEFT JOIN ? AS usecond ON (usecond.channelid = channels.id AND relations.secondid = usecond.accountid AND relations.second = usecond.account) "
      "WHERE channels.name = ? AND (relations.first = ? AND relations.firstid = 0 OR relations.second = ? AND relations.secondid = 0) ORDER BY score DESC LIMIT 10",
      "TTTTsss", "relations", "channels", "users", "users", argv[0], &(argv[1][1]), &(argv[1][1]));
  } else if (atoi(argv[2]) == 0) {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT relations.first, relations.firstid, ufirst.curnick, ufirst.seen, relations.second, relations.secondid, usecond.curnick, usecond.seen, relations.seen, relations.score "
      "FROM ? "
      "LEFT JOIN ? ON relations.channelid = channels.id "
      "LEFT JOIN ? AS ufirst ON (ufirst.channelid = channels.id AND relations.firstid = ufirst.accountid AND relations.first = ufirst.account) "
      "LEFT JOIN ? AS usecond ON (usecond.channelid = channels.id AND relations.secondid = usecond.accountid AND relations.second = usecond.account) "
      "WHERE channels.name = ? AND (ROW(relations.first, relations.firstid) = "
      "(SELECT account, accountid FROM ? WHERE accountid != 0 AND account = ? ORDER BY accountid DESC LIMIT 1) OR "
      "ROW(relations.second, relations.secondid) = (SELECT account, accountid FROM ? WHERE accountid != 0 AND account = ? ORDER BY accountid DESC LIMIT 1)) "
      "ORDER BY score DESC LIMIT 10",
      "TTTTsTsTs", "relations", "channels", "users", "users", argv[0], "users", argv[1], "users", argv[1]);
  } else {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT relations.first, relations.firstid, ufirst.curnick, ufirst.seen, relations.second, relations.secondid, usecond.curnick, usecond.seen, relations.seen, relations.score "
      "FROM ? "
      "LEFT JOIN ? ON relations.channelid = channels.id "
      "LEFT JOIN ? AS ufirst ON (ufirst.channelid = channels.id AND relations.firstid = ufirst.accountid AND relations.first = ufirst.account) "
      "LEFT JOIN ? AS usecond ON (usecond.channelid = channels.id AND relations.secondid = usecond.accountid AND relations.second = usecond.account) "
      "WHERE channels.name = ? AND (relations.firstid = ? OR relations.secondid = ?) ORDER BY score DESC LIMIT 10",
      "TTTTsssss", "relations", "channels", "users", "users", argv[0], argv[1], argv[2], argv[1], argv[2]);
  }

  return 0;
}

static int handle_setprivacy(struct rline *ri, int argc, char **argv) {
  a4statsdb->squery(a4statsdb, "UPDATE ? SET privacy = ? WHERE name = ?", "Tss", "channels", argv[1], argv[0]);
  return ri_final(ri);
}

static int handle_finduser(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT users.account, users.accountid, users.seen, users.curnick, channels.name, channels.privacy  "
    "FROM ? "
    "LEFT JOIN ? ON channels.id = users.channelid "
    "WHERE (lines > 5 OR accountid != 0) AND (LOWER(curnick) LIKE ? OR LOWER(account) LIKE ?) ORDER BY seen DESC LIMIT 150",
    "TTss", "users", "channels", argv[0], argv[0]);

  return 0;
}

static int handle_findchan(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT name, privacy FROM ? WHERE active = 1 AND LOWER(name) LIKE ? LIMIT 25",
    "Ts", "channels", argv[0]);

  return 0;
}

static int handle_getuserchans(struct rline *ri, int argc, char **argv) {
#define USERCHANS_QUERY(b) "SELECT channels.name, channels.privacy, users.account, users.accountid, users.curnick, users.firstseen, users.seen FROM ? JOIN ? ON channels.id = users.channelid WHERE channels.active = 1 AND " b " ORDER BY channels.name"
  /*
    Possible cases:
    accountid = 0, account = "username" -> new-style account, look up latest account for user
    accountid = 0, account = "#username" -> legacy account or user not authed, look up using "username" (remove #)
    accountid = <some value>, account = <unused> -> new-style account, look up by account id
  */

  if (argv[0][0] == '#') {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, USERCHANS_QUERY("(users.accountid = 0 AND users.account = ?)"),
      "TTs", "users", "channels", &(argv[0][1]));
  } else if (atoi(argv[1]) == 0) {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, USERCHANS_QUERY("(ROW(users.account, users.accountid) = (SELECT account, accountid FROM ? WHERE users.accountid != 0 AND users.account = ? ORDER BY users.accountid DESC LIMIT 1))"),
      "TTTs", "users", "channels", "users", argv[0]);
  } else {
    a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, USERCHANS_QUERY("users.accountid = ?"),
      "TTs", "users", "channels", argv[1]);
  }

  return 0;
}

void _init(void) {
  a4stats_node = register_service("a4stats");
  if (!a4stats_node)
    return;

  register_handler(a4stats_node, "getchannel", 1, handle_getchannel);
  register_handler(a4stats_node, "getlines", 1, handle_getlines);
  register_handler(a4stats_node, "getusers", 1, handle_getusers);
  register_handler(a4stats_node, "getkicks", 1, handle_getkicks);
  register_handler(a4stats_node, "gettopics", 1, handle_gettopics);
  register_handler(a4stats_node, "getuser", 3, handle_getuser);
  register_handler(a4stats_node, "getrelations", 1, handle_getrelations);
  register_handler(a4stats_node, "getuserrelations", 3, handle_getuserrelations);
  register_handler(a4stats_node, "setprivacy", 2, handle_setprivacy);
  register_handler(a4stats_node, "finduser", 1, handle_finduser);
  register_handler(a4stats_node, "findchan", 1, handle_findchan);
  register_handler(a4stats_node, "getuserchans", 2, handle_getuserchans);
}

void _fini(void) {
  if (a4stats_node)
    deregister_service(a4stats_node);
}
