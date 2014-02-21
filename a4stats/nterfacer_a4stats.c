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

      while (result->next(result))
        for (i = 0; i < result->fields; i++)
          ri_append(ri, "%s", result->get(result, i));
    }

    result->clear(result);
  } else
    ri_append(ri, "FAILED");

  ri_final(ri);
}

static int handle_getlines(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT SUM(h0) AS h0, SUM(h1) AS h1, SUM(h2) AS h2, SUM(h3) AS h3, "
    "SUM(h4) AS h4, SUM(h5) AS h5, SUM(h6) AS h6, SUM(h7) AS h7, SUM(h8) AS h8, SUM(h9) AS h9, SUM(h10) AS h10, "
    "SUM(h11) AS h11, SUM(h12) AS h12, SUM(h13) AS h13, SUM(h14) AS h14, SUM(h15) AS h15, SUM(h16) AS h16, "
    "SUM(h17) AS h17, SUM(h18) AS h18, SUM(h19) AS h19, SUM(h20) AS h20, SUM(h21) AS h21, SUM(h22) AS h22, SUM(h23) AS h23 "
    "FROM ? WHERE channel = ?", "Tss", "users", argv[0]);
  return 0;
}

static int handle_getusers(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT account, accountid, seen, rating, lines, chars, words, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11, "
    "h12, h13, h14, h15, h16, h17, h18, h19, h20, h21, h22, h23, "
    "last, quote, quotereset INT, mood_happy, mood_sad, questions, yelling, caps, "
    "slaps, slapped, highlights, kicks, kicked, ops, deops, actions, skitzo, foul, "
    "firstseen, curnick FROM ? WHERE channel = ? ORDER BY lines DESC LIMIT 25", "Tss", "users", argv[0]);
  return 0;
}

static int handle_getkicks(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT kicker, kickerid, victim, victimid, timestamp, reason FROM ? WHERE channel = ? ORDER BY timestamp DESC LIMIT 10", "Ts", "kicks", argv[0]);
  return 0;
}

static int handle_gettopics(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT topic, timestamp, setby, setbyid FROM ? WHERE channel = ? ORDER BY timestamp DESC LIMIT 10", "Ts", "topics", argv[0]);
  return 0;
}

static int handle_getuser(struct rline *ri, int argc, char **argv) {
  a4statsdb->query(a4statsdb, a4stats_nt_query_cb, ri, "SELECT account, accountid, seen, rating, lines, chars, words, h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11, "
    "h12, h13, h14, h15, h16, h17, h18, h19, h20, h21, h22, h23, "
    "last, quote, quotereset INT, mood_happy, mood_sad, questions, yelling, caps, "
    "slaps, slapped, highlights, kicks, kicked, ops, deops, actions, skitzo, foul, "
    "firstseen, curnick FROM ? WHERE channel = ? AND account = ?", "Tss", "users", argv[0], argv[1]);
  return 0;
}

void _init(void) {
  a4stats_node = register_service("a4stats");
  if (!a4stats_node)
    return;

  register_handler(a4stats_node, "getlines", 1, handle_getlines);
  register_handler(a4stats_node, "getusers", 1, handle_getusers);
  register_handler(a4stats_node, "getkicks", 1, handle_getkicks);
  register_handler(a4stats_node, "gettopics", 1, handle_gettopics);
  register_handler(a4stats_node, "getuser", 2, handle_getuser);
}

void _fini(void) {
  if (a4stats_node)
    deregister_service(a4stats_node);
}
