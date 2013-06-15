#include "gline_newsearch.h"
#include "../../newsearch/newsearch.h"

static void glnsmessagewrapper(nick *np, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  controlreply(np, "%s", buf);
}

static void glnswallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  controlwall(NO_OPER, level, "%s", buf);
}

/*
int tsns_dotrustlist(void *source, int cargc, char **cargv) {
  searchASTExpr tree;
  searchASTExpr nodes[2];
  
  if(cargc < 1) {
    controlreply(source,"Syntax: trustlist <#groupid>");
    return CMD_ERROR;
  }

  if(cargv[0][0]== '#'){
    tree = NSASTNode(eq_parse, NSASTNode(tsns_tgid_parse), NSASTLiteral(&cargv[0][1]));
  } else {
    tree = NSASTNode(eq_parse, NSASTNode(tsns_tgid_parse), NSASTLiteral(cargv[0]));
  }

//  nodes[0] = NSASTNode(tsns_tgid_parse);
//  nodes[1] = cargv[0];
//  tree =
//      NSASTManualNode(eq_parse, 2, nodes 
//    );
  return ast_tgsearch(&tree, tsnsmessagewrapper , source, tsnswallwrapper, printtgfull, NULL, NULL, 1);
}
*/

int glns_dornglist(void *source, int cargc, char **cargv) {
  searchASTExpr tree;

  tree = NSASTNode(gsns_glrealname_parse);

  return ast_glsearch(&tree, glnsmessagewrapper, source, glnswallwrapper, printgl, printgl_header, NULL, 500);
}

int glns_doclearchan(void *source, int cargc, char **cargv) {
  searchASTExpr tree;
  searchASTExpr nodes[2];
  channel *cp;
  int type, duration; 
  char *reason;

  if(cargc < 2) {
    controlreply(source,"Syntax: clearchan #channel type [duration] [reason]");
    controlreply(source,"type 1 = kick, 2 = kill, 3 = ident gline, 5 = ident@trust/*@host, 6=5+unauthed only");
    return CMD_ERROR;
  }

  if(cargv[0][0]!= '#'){
    controlreply(source, "First argument must be a channel name");
    return CMD_ERROR;
  }

  if ((cp=findchannel(cargv[0]))==NULL) {
    controlreply(source,"Couldn't find channel: %s",cargv[0]);
    return CMD_ERROR;
  }

  // duration is optional TODO
  type=strtoul(cargv[1],NULL,10);
  if (cargc >= 3) {
    duration= strtoul(cargv[2],NULL,10);
  }
  if (cargc < 4) {
    reason = "Clearing Channel...";
  } else {
    reason = cargv[3];
  } 

  switch (type) {
    case 1:
      tree = NSASTNode(and_parse, NSASTNode(channel_parse, NSASTLiteral(cargv[0])), NSASTNode(kick_parse, NSASTLiteral(reason)));
      break;

  }  
//  tree = NSASTNode(channel_parse, NSASTLiteral(cargv[0]));
  return ast_nicksearch(&tree, glnsmessagewrapper, source, glnswallwrapper, printnick, printnick_header, NULL, 500);  
}
