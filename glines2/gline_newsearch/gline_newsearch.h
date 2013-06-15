#ifndef __TRUSTS_NEWSEARCH_H_
#define __TRUSTS_NEWSEARCH_H

#include "../../patriciasearch/patriciasearch.h"
#include "../gline.h"
#include "../gline_search/gline_search.h"

struct searchNode *gsns_glexpire_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *gsns_glisrealname_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *gsns_glrealname_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *gsns_glbadchan_parse(searchCtx *ctx, int argc, char **argv);
struct searchNode *gsns_glcreator_parse(searchCtx *ctx, int argc, char **argv);


int glns_dornglist(void *source, int cargc, char **cargv);
int glns_doclearchan(void *source, int cargc, char **cargv);

#endif
