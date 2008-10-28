%{
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "../lib/stringbuf.h"
#include "newsearch.h"
#include "parser.h"

#define YYSTYPE sstring *

int yylex(void);
extern char *parseStrError;
extern int parseStrErrorPos;

#define MAXSTACKSIZE 50

typedef struct parserlist {
  searchASTExpr expr;
  struct parserlist *next;
} parserlist;

static int stackpos;
static parserlist *stack[MAXSTACKSIZE];
static int stackcount[MAXSTACKSIZE];
static fnFinder functionfinder;
static void *fnfarg;
static parsertree **presult, sresult;

extern int lexerror, lexpos;
void yystrerror(const char *format, ...);
void lexreset(void);

void resetparser(fnFinder fnf, void *arg, parsertree **result) {
  presult = result;
  *presult = &sresult;
  
  sresult.exprlist = NULL;
  sresult.strlist = NULL;
  sresult.finished = 0;

  functionfinder = fnf;
  fnfarg = arg;
 
  stackpos = 0;
  lexreset();
}

void yyerror(const char *str) {
  if(lexerror) {
    lexerror = 0;
    yystrerror("lexical error");
    return;
  }

  parseStrError = (char *)str;
  parseStrErrorPos = lexpos;
}

void yystrerror(const char *format, ...) {
  va_list va;
  static char buf[512];

  parse_free(&sresult);

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  yyerror(buf);
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
    
    pfn = functionfinder(str->content, fnfarg);
    if(!pfn) {
      yystrerror("unknown function: %s", str->content);
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
    if(stackpos >= MAXSTACKSIZE - 1) {
      yyerror("function stack overflow");
      YYERROR;
    }

    stackcount[stackpos] = 0;
    stack[stackpos] = NULL;
    
    stackpos++;
  };
  
argumentlist: /* empty */ | WHITESPACE argument argumentlist;

argument:
	invocation | literal
	{
    sstring *str = $1;
    parserlist *l = malloc(sizeof(parserlist));

    if(str) {
      stringlist *sl;
      l->expr = NSASTLiteral(str->content);

      sl = malloc(sizeof(stringlist));
      sl->data = str;
      sl->next = (*presult)->strlist;
      (*presult)->strlist = sl;
    } else {
      l->expr = NSASTLiteral("");
    }

    l->next = stack[stackpos - 1];
    stack[stackpos - 1] = l;
    stackcount[stackpos - 1]++;

	}
	;

literal: IDENTIFIER | STRING;
