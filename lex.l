%{

#include <string.h>

%}

%option nounput
%option noinput
%option noyywrap

%%

"("		{ return '('; }
")"             { return ')'; }
"|"             { return PIPE; }
";"             { return SEQ; }
"&&"            { return AND; }
"||"            { return OR; }
">>"            { return APPEND; }
">"             { return OUTPUT; }
"<"             { return INPUT; }
"2>"            { return ERROR; }

[0-9A-z*?/.-]*  {
		  yylval.string = strdup(yytext);
		  return ARG;
		}
\"[^"]*\"       {
		  yylval.string = strdup(yytext+1);
		  yylval.string[strlen(yylval.string)-1] = 0;
		  return ARG;
		}
\'[^']*\'       {
		  yylval.string = strdup(yytext+1);
		  yylval.string[strlen(yylval.string)-1] = 0;
		  return ARG;
		}

.               ;
