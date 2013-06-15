#ifndef __TRUSTS_NEWSEARCH_H_
#define __TRUSTS_NEWSEARCH_H

#include "../../patriciasearch/patriciasearch.h"
#include "../trusts.h"
#include "../search/trusts2_search.h"

void printtrust_group(searchCtx *ctx, nick *sender, patricia_node_t *node);
void printtrust_block(searchCtx *ctx, nick *sender, patricia_node_t *node);
void printtrust_blockprivate(searchCtx *ctx, nick *sender, patricia_node_t *node);

struct searchNode *tsns_trusted_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgid_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgexpire_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgmaxperip_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgownerid_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgstartdate_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tglastused_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgmaxusage_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgcurrenton_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgmaxclones_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgmaxperident_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgenforceident_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgcreated_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_tgmodified_parse(searchCtx *ctx, int argc, char **argv);

struct searchNode *tsns_thcreated_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_thexpire_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_thid_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_thlastused_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_thmaxusage_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_thmodified_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *tsns_thstartdate_parse(searchCtx *ctx, int argc, char **argv);

struct searchNode *tsns_tbid_parse(searchCtx *ctx, int argc, char **argv);

struct searchNode *tsns_istrusted_parse(searchCtx *ctx, int argc, char **argv);

int tsns_dotrustlist(void *source, int cargc, char **cargv);
int tsns_dotrustdenylist(void *source, int cargc, char **cargv);

#endif
