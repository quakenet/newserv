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
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT timestamp, active, privacy FROM ? WHERE name = ?", "Ts", "channels", argv[0]);
  return 0;
}

static int handle_getlines(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT SUM(users.h0) AS h0, SUM(users.h1) AS h1, SUM(users.h2) AS h2, SUM(users.h3) AS h3, "
    "SUM(users.h4) AS h4, SUM(users.h5) AS h5, SUM(users.h6) AS h6, SUM(users.h7) AS h7, SUM(users.h8) AS h8, SUM(users.h9) AS h9, SUM(users.h10) AS h10, "
    "SUM(users.h11) AS h11, SUM(users.h12) AS h12, SUM(users.h13) AS h13, SUM(users.h14) AS h14, SUM(users.h15) AS h15, SUM(users.h16) AS h16, "
    "SUM(users.h17) AS h17, SUM(users.h18) AS h18, SUM(users.h19) AS h19, SUM(users.h20) AS h20, SUM(users.h21) AS h21, SUM(users.h22) AS h22, SUM(users.h23) AS h23 "
    "FROM ? JOIN ? ON channels.id = users.channelid WHERE channels.name = ?", "TTs", "users", "channels", argv[0]);

  return 0;
}

static int handle_getusers(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT users.account, users.accountid, users.seen, users.rating, users.lines, users.chars, users.words, users.h0, users.h1, users.h2, users.h3, users.h4, users.h5, users.h6, users.h7, users.h8, users.h9, users.h10, users.h11, users."
    "h12, users.h13, users.h14, users.h15, users.h16, users.h17, users.h18, users.h19, users.h20, users.h21, users.h22, users.h23, users."
    "last, users.quote, users.quotereset, users.mood_happy, users.mood_sad, users.questions, users.yelling, users.caps, users."
    "slaps, users.slapped, users.highlights, users.kicks, users.kicked, users.ops, users.deops, users.actions, users.skitzo, users.foul, users."
    "firstseen, users.curnick FROM ? JOIN ? ON channels.id = users.channelid WHERE channels.name = ? AND users.quote IS NOT NULL ORDER BY lines DESC LIMIT 25", "Tss", "users", "channels", argv[0]);

  return 0;
}

static int handle_getkicks(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT kicks.kicker, kicks.kickerid, kicks.victim, kicks.victimid, kicks.timestamp, kicks.reason "
    "FROM ? JOIN ? ON channels.id = kicks.channelid WHERE channels.name = ? ORDER BY kicks.timestamp DESC LIMIT 10", "TTs", "kicks", "channels", argv[0]);

  return 0;
}

static int handle_gettopics(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT topics.topic, topics.timestamp, topics.setby, topics.setbyid "
    "FROM ? JOIN ? ON channels.id = topics.channelid WHERE channels.name = ? ORDER BY topics.timestamp DESC LIMIT 10", "TTs", "topics", "channels", argv[0]);

  return 0;
}

static int handle_getuser(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT users.account, users.accountid, users.seen, users.rating, users.lines, users.chars, users.words, users.h0, users.h1, users.h2, users.h3, users.h4, users.h5, users.h6, users.h7, users.h8, users.h9, users.h10, users.h11, users."
    "h12, users.h13, users.h14, users.h15, users.h16, users.h17, users.h18, users.h19, users.h20, users.h21, users.h22, users.h23, users."
    "last, users.quote, users.quotereset INT, users.mood_happy, users.mood_sad, users.questions, users.yelling, users.caps, users."
    "slaps, users.slapped, users.highlights, users.kicks, users.kicked, users.ops, users.deops, users.actions, users.skitzo, users.foul, users."
    "firstseen, users.curnick FROM ? JOIN ? ON channels.id = users.channelid WHERE channels.name = ? AND (users.accountid != 0 AND users.accountid = ? OR users.accountid = 0 AND users.account = ?)", "TTsss", "users", "channels", argv[0], argv[2], argv[1]);

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
}

void _fini(void) {
  if (a4stats_node)
    deregister_service(a4stats_node);
}
