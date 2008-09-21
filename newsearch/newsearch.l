%option noyywrap

%{
#include <stdlib.h>
#include <string.h>

#include "../lib/sstring.h"

#define YYSTYPE sstring *
extern YYSTYPE yylval;

#include "y.tab.h"
#include "../lib/stringbuf.h"

#define MAX_STR_CONST 512

int lexpos, lexerror;

#define addpos(x) lexpos+=x;
#define incpos() addpos(1);

void lexreset(void) {
  lexerror = 0;
  lexpos = -1;
}

%}

digit		[0-9]
letter		[a-zA-Z]

%x str pascalstr
%%

	char bufdata[MAX_STR_CONST];
	StringBuf buf;

\"		{ incpos();
		  sbinit(&buf, bufdata, sizeof(bufdata));
		  BEGIN(str);
		}

<str>{
  \"		{ incpos();
		  BEGIN(INITIAL);
		  sbterminate(&buf);
		  yylval = getsstring(bufdata, 512);
		  return STRING;
		}
  \\\\		{ addpos(2);
		  sbaddchar(&buf, '\\');
		}
  \\\"		{ addpos(2);
		  sbaddchar(&buf, '"');
		}
  [^\\]		{ incpos();
		  sbaddchar(&buf, *yytext);
		}
}

\'		{ incpos();
		  sbinit(&buf, bufdata, sizeof(bufdata));
		  BEGIN(pascalstr);
		}

<pascalstr>{
  \'\'		{ addpos(2);
		  sbaddchar(&buf, '\'');
		}
  \'		{ incpos();
		  BEGIN(INITIAL);
		  sbterminate(&buf);
		  yylval = getsstring(bufdata, 512);
		  return STRING;
		}
  .		{ incpos();
		  sbaddchar(&buf, *yytext);
		}
}

"("		{ incpos();
		  return LPAREN;
		}
")"		{ incpos();
		  return RPAREN;
		}
[ \t]+		{ addpos(strlen(yytext));
		  return WHITESPACE;
		}
({letter}|{digit}|[#\-\+\.])+	{ addpos(strlen(yytext));
		  yylval = getsstring(yytext, 512);
		  return IDENTIFIER;
		}

[\n\r]		{ incpos();
		  /* ignore */;
		}

.		{
		  incpos();
		  lexerror = 1;
		  yyterminate();
		}

<<EOF>>		{
		  BEGIN(INITIAL);
		  yyterminate();
		}
%%
