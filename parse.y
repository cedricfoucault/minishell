%{

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "global.h"

struct cmd* cmdline;
int yylex();
void yyerror (char*);

%}

%union 
{
	char *string;
	struct arglist* arglist;
 	char **args;
	struct cmd* cmd;
	int token;
}

%token <string> ARG
%token PIPE AND OR SEQ APPEND OUTPUT INPUT ERROR PLAIN VOID

%type <cmd> single line mods
%type <args> args
%type <token> op dir
%type <arglist> arglist

%left AND OR PIPE

%%

main    : line
	  { cmdline = $1; }

line    : single
	| single op line
	  {
		$$ = calloc(1,sizeof(struct cmd));
		$$->type = $2;
		$$->left = $1;
		$$->right = $3;
	  }


single  : args mods
	  {
		$$ = $2;
		$$->type = C_PLAIN;
		$$->args = $1;
	  }
	| '(' line ')' mods
	  {
		$$ = $4;
		$$->type = C_VOID;
		$$->left = $2;
	  }

args     : arglist
	  {
		int cnt = 0;
		struct arglist *pt = $1, *tmp;
		while (pt) { pt = pt->next; cnt++; }
		$$ = calloc(cnt+1,sizeof(char*));
		pt = $1; cnt = 0;
		while (pt) {
			$$[cnt++] = pt->arg;
			tmp = pt; pt = pt->next; free(tmp);
		}
	  }

arglist : ARG
	  {
		$$ = calloc(1,sizeof(struct arglist));
		$$->arg = $1;
	  }
	| arglist ARG
	  {
		struct arglist* pt;
		pt = $$ = $1;
		while (pt->next) { pt = pt->next; }
		pt->next = calloc(1,sizeof(struct arglist));
		pt->next->arg = $2;
	  }

mods    : { $$ = calloc(1,sizeof(struct cmd)); }
	| mods dir ARG
	  { $$ = $1;
	    if ($2 == INPUT)  { $$->input = $3; }
	    if ($2 == OUTPUT) { $$->output = $3; }
	    if ($2 == APPEND) { $$->append = $3; }
	    if ($2 == ERROR)  { $$->error = $3; }
	  }

dir     : INPUT  { $$ = INPUT;  }
	| OUTPUT { $$ = OUTPUT; }
	| APPEND { $$ = APPEND; }
	| ERROR  { $$ = ERROR;  }

op      : PIPE { $$ = C_PIPE; }
	| AND  { $$ = C_AND;  }
	| OR   { $$ = C_OR;   }
	| SEQ  { $$ = C_SEQ;  }

%%

#include "lex.c"

void yyerror (char *info)
{ 
	fprintf(stderr,"cannot parse input: %s; discarded\n",info);
}

struct cmd* parser (char *command)
{
	yy_scan_string(command);
	if (yyparse()) return NULL;
	return cmdline;
}
