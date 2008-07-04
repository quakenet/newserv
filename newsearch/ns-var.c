/*
 * VAR functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct searchNode *var_parse(searchCtx *ctx, int type, int argc, char **argv) {
  if(argc < 1) {
    parseError = "var: usage: var variable";
    return NULL;
  }
  
  return var_get(ctx, type, argv[0]);
}
