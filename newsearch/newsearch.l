%option noyywrap

%{
#include "../lib/sstring.h"

#define YYSTYPE sstring *
extern YYSTYPE yylval;

#include "y.tab.h"

#include <stdlib.h>
#include "../lib/stringbuf.h"

#define MAX_STR_CONST 512

/* shuts gcc up */
static void yyunput (int c,char *buf_ptr);
void newsearch_parser_dummy(void) {
  yyunput(0, NULL);
}

int fileno(void *arg) {
  return 0;
}

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
		  yylval = getsstring(bufdata, 512);
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
		  yylval = getsstring(yytext, 512);
		  return IDENTIFIER;
		}

[\n\r]		/* ignore */;

<<EOF>>		{
		  BEGIN(INITIAL);
		  yyterminate();
		}
%%
