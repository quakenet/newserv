#include "../newsearch/newsearch.h"
#include "../patricia/patricia.h"
#include "../patricianick/patricianick.h"
#include "../newsearch/parser.h"

typedef void (*NodeDisplayFunc)(struct searchCtx *, nick *, patricia_node_t *);

void printnode(searchCtx *, nick *, patricia_node_t *);

void pnodesearch_exe(struct searchNode *search, searchCtx *ctx, patricia_node_t *subset);

int do_pnodesearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv);

int ast_nodesearch(searchASTExpr *tree, replyFunc reply, void *sender, wallFunc wall, NodeDisplayFunc display, HeaderFunc header, void *headerarg, int limit, patricia_node_t *target);

void regpnodedisp(const char *name, NodeDisplayFunc handler);
void unregpnodedisp(const char *name, NodeDisplayFunc handler);

extern NodeDisplayFunc defaultpnodefn;
extern searchCmd *reg_nodesearch;
 
struct searchNode *ps_nick_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *ps_users_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *ps_ipv6_parse(searchCtx *ctx, int argc, char **argv);
