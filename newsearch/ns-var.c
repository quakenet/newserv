/*
 * VAR functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct searchNode *var_parse(searchCtx *ctx, int argc, char **argv) {
  if(argc < 1) {
    parseError = "var: usage: var variable";
    return NULL;
  }
  
  /* @argv usage OK */
  return var_get(ctx, argv[0]);
}
