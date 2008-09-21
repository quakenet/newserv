%option noyywrap

%{
#include "y.tab.h"
#include <string.h>

#define YYSTYPE char *
extern YYSTYPE yylval;

#include <stdlib.h>
#include "../lib/stringbuf.h"

#define MAX_STR_CONST 512
%}

digit		[0-9]
letter		[a-zA-Z]

%x str
%%

	char bufdata[MAX_STR_CONST];
	StringBuf buf;

\"		{
		  sbinit(&buf, bufdata, sizeof(bufdata));
		  BEGIN(str);
		}

<str>{
  \"		{
		  BEGIN(INITIAL);
		  sbterminate(&buf);
		  yylval = strdup(bufdata);
		  return STRING;
		}
  \\\\		{ sbaddchar(&buf, '\\');	}
  \\\"		{ sbaddchar(&buf, '"');		}
  [^\\]		{ sbaddchar(&buf, *yytext); 	}
}

"("		{ return LPAREN;		}
")"		{ return RPAREN;		}
[ \t]+		{ return WHITESPACE;		}
{letter}+	{
		  yylval = strdup(yytext);
		  return IDENTIFIER;
		}

[\n\r]		/* ignore */;

<<EOF>>		{
		  BEGIN(INITIAL);
		  yyterminate();
		}
%%
