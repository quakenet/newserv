#include <stdio.h>

#include "../../newsearch/newsearch.h"
#include "../../control/control.h"
#include "../../lib/stringbuf.h"
#include "../glines.h"

void printgl_header(searchCtx *ctx, nick *sender, void *args) {
  ctx->reply(sender,"Mask                                                Expires in          Created By                Reason");
}

void printgl(searchCtx *ctx, nick *sender, gline *g) {
  char tmp[250];
  snprintf(tmp, 249, "%s", glinetostring(g));
  ctx->reply(sender,"%s%-50s %-19s %-25s %s", g->flags & GLINE_ACTIVE ? "+" : "-", tmp, g->flags & GLINE_ACTIVE ? (char *)longtoduration(g->expires - getnettime(), 0) : "<inactive>",
            g->creator ? g->creator->content : "", g->reason ? g->reason->content : "");
}

