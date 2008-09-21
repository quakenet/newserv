%{
#include <stdio.h>
#include <string.h>
#include "../lib/stringbuf.h"
#include "newsearch.h"
#include "parser.h"

#define YYSTYPE sstring *

int yylex(void);
extern char *parseStrError;
int position;

#define MAXSTACKSIZE 50

typedef struct parserlist {
  searchASTExpr expr;
  struct parserlist *next;
} parserlist;

void yyerror(const char *str) {
  parseStrError = (char *)str;
}

static int stackpos;
static parserlist *stack[MAXSTACKSIZE];
static int stackcount[MAXSTACKSIZE];
static fnFinder functionfinder;
static void *fnfarg;
static parsertree **presult, sresult;

void resetparser(fnFinder fnf, void *arg, parsertree **result) {
  presult = result;
  
  sresult.exprlist = NULL;
  sresult.strlist = NULL;
  sresult.finished = 0;
  
  functionfinder = fnf;
  fnfarg = arg;
  
  stackpos = 0;
}

%}

%token LPAREN RPAREN WHITESPACE IDENTIFIER STRING

%%
invocation: LPAREN function argumentlist RPAREN
	{
    sstring *str = $2;
    int i, count;
    searchASTExpr *ap, *root;
    parserlist *pp, *npp;
    expressionlist *xl;
    parseFunc pfn;
    
    stackpos--;
    count = stackcount[stackpos];
    
    pfn = functionfinder(str, fnfarg);
    if(!pfn) {
      parse_free(&sresult);
      YYERROR;
    }
    /* if we're at the top of the stack we're the root of the tree */
    if(stackpos != 0) {
      /*
       * we store this function containing its children in the stack list
       * as there might be functions or other stuff after it, such as:
       * (fn (fn2) moo moo)
       */
      parserlist *rootp = malloc(sizeof(searchASTExpr) + sizeof(parserlist));
      root = &rootp->expr;
      
      stackcount[stackpos-1]++;
      rootp->next = stack[stackpos-1];
      stack[stackpos-1] = rootp;
    } else {
      /* need space for the final result and real root */
      *presult = malloc(sizeof(parsertree) + sizeof(searchASTExpr));

      memcpy(*presult, &sresult, sizeof(parsertree));
      (*presult)->finished = 1;
      
      root = (*presult)->root;
    }

    xl = malloc(sizeof(expressionlist) + sizeof(searchASTExpr) * count);    
    xl->next = (*presult)->exprlist;
    (*presult)->exprlist = xl;
    
    ap = xl->expr;
    for(i=count-1,pp=stack[stackpos];i>=0;i--,pp=npp) {
      npp = pp->next;
      memcpy(&ap[i], &pp->expr, sizeof(searchASTExpr));
      free(pp);
    }
    *root = NSASTManualNode(pfn, count, ap);
   
    freesstring(str);
	}

function: IDENTIFIER
  {
    if(stackpos >= MAXSTACKSIZE - 1)
      YYERROR;
      
    stackcount[stackpos] = 0;
    stack[stackpos] = NULL;
    
    stackpos++;
  };
  
argumentlist: /* empty */ | WHITESPACE argument argumentlist;

argument:
	invocation | literal
	{
    sstring *str = $1;
    parserlist *l = malloc(sizeof(parserlist) + sizeof(stringlist));
    stringlist *sl;
    
    l->expr = NSASTLiteral(str->content);
    l->next = stack[stackpos - 1];
    stack[stackpos - 1] = l;
    stackcount[stackpos - 1]++;

    /* HACK */
    sl = (stringlist *)(l + 1);

    sl->data = str;
    sl->next = (*presult)->strlist;
    (*presult)->strlist = sl;
	}
	;

literal: IDENTIFIER | STRING;
