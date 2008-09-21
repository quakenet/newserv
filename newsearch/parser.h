#ifndef NEWSEARCH_PARSER_H
#define NEWSEARCH_PARSER_H

#include "newsearch.h"

typedef struct stringlist {
  struct stringlist *next;
  char data[];
} stringlist;

typedef struct expressionlist {
  struct expressionlist *next;
  searchASTExpr expr[];
} expressionlist;

typedef struct parsertree {
  expressionlist *exprlist;
  stringlist *strlist;
  int finished;
  searchASTExpr root[];
} parsertree;

typedef parseFunc (*fnFinder)(char *, void *);

parsertree *parse_string(searchCmd *, const char *);
void parse_free(parsertree *);

void resetparser(fnFinder fnf, void *arg, parsertree **result);

extern char *parseStrError;

#endif
