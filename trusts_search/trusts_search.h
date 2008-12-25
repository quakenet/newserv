#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../newsearch/newsearch.h"
#include "../control/control.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../parser/parser.h"
#include "../lib/splitline.h"
#include "../lib/version.h"
#include "../lib/stringbuf.h"
#include "../lib/strlfunc.h"
#include "../trusts2/trusts.h"
#include "../newsearch/parser.h"

extern searchCmd *reg_tgsearch;
extern searchCmd *reg_thsearch;

void printtg(searchCtx *ctx, nick *sender, trustgroup_t *tg);
void printth(searchCtx *ctx, nick *sender, trusthost_t *tgh);
void printtgfull(searchCtx *ctx, nick *sender, trustgroup_t *g);
