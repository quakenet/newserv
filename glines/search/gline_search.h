#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "../../newsearch/newsearch.h"
#include "../../control/control.h"
#include "../../irc/irc_config.h"
#include "../../lib/irc_string.h"
#include "../../parser/parser.h"
#include "../../lib/splitline.h"
#include "../../lib/version.h"
#include "../../lib/stringbuf.h"
#include "../../lib/strlfunc.h"
#include "../glines.h"
#include "../../newsearch/parser.h"

extern searchCmd *reg_glsearch;

typedef void (*GLDisplayFunc)(struct searchCtx *, nick *, gline *);

int do_glsearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);
void glsearch_exe(struct searchNode *search, searchCtx *ctx);

int ast_glsearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, GLDisplayFunc display, HeaderFunc header, void *headerarg, int limit);

void printgl(searchCtx *ctx, nick *sender, gline *gl);
void printgl_header(searchCtx *ctx, nick *sender, void *);
