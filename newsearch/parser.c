#include <stdio.h>
#include "newsearch.h"
#include "parser.h"

struct yy_buffer_state;

struct yy_buffer_state *yy_scan_string(const char *);
void yy_delete_buffer(struct yy_buffer_state *);
void yy_flush_buffer(struct yy_buffer_state *);

int yyparse(void);
char *parseStrError;

parseFunc fnfinder(char *name, void *arg) {
  searchCmd *cmdtree = arg;
  struct Command *cmd;
  parseFunc ret;
  
  if (!(cmd=findcommandintree(cmdtree->searchtree, name, 1))) {
    parseError = "Unknown command (for valid command list, see help <searchcmd>)";
    return NULL;
  }
  ret = (parseFunc)cmd->handler;
  /* TODO */
  
  /*
    if (!controlpermitted(ret->level, ret->sender)) { 
      parseError = "Access denied (for valid command list, see help <searchcmd>)";
      return NULL;
    }
    return ((parseFunc)cmd->handler)(ctx, j, argvector+1);
  }*/

  return ret;
}

parsertree *parse_string(searchCmd *cmd, const char *str) {
  int ret;
  struct yy_buffer_state *b;
  parsertree *pt;
  
  b = yy_scan_string(str);
  if(!b) {
    parseStrError = "string buffer creation error";
    return NULL;
  }

  resetparser(fnfinder, cmd, &pt);
  ret = yyparse();

  yy_flush_buffer(b);
  yy_delete_buffer(b);

  if(ret) /* error occured, parseStrError has it */
    return NULL;

  parseStrError = "not yet implemented";
  return pt;
}

void parse_free(parsertree *pt) {
  stringlist *sl, *nsl;
  expressionlist *xl, *nxl;
  
  for(sl=pt->strlist;sl;sl=nsl) {
    nsl = sl->next;
    freesstring(sl->data);
    free(sl);
  }

  for(xl=pt->exprlist;xl;xl=nxl) {
    nxl = xl->next;
    free(xl);
  }
  
  if(pt->finished)
    free(pt);
}
