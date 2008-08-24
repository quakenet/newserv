#include "../newsearch/newsearch.h"
#include "../patricia/patricia.h"
#include "../patricianick/patricianick.h"

typedef void (*NodeDisplayFunc)(struct searchCtx *, nick *, patricia_node_t *);

void printnode(searchCtx *, nick *, patricia_node_t *);

void pnodesearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, NodeDisplayFunc display, int limit, patricia_node_t *subset);

int do_pnodesearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);

int ast_pnodesearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, NodeDisplayFunc display, HeaderFunc header, void *headerarg, int limit);

extern NodeDisplayFunc defaultpnodefn;
extern searchCmd *reg_nodesearch;
 
struct searchNode *ps_nick_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *ps_users_parse(searchCtx *ctx, int argc, char **argv);
