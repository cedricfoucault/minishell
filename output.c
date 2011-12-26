#include "global.h"
#include <stdio.h>
#include <stdlib.h>

// some helper procedures to output, see below
void output_args (struct cmd *cmd, char *tabs)
{
	int i;
	printf("%sarguments:",tabs);
	for (i = 0; cmd->args[i]; i++) printf(" [%s]",cmd->args[i]);
	printf("\n");
}

void output_mods (struct cmd *cmd, char *tabs)
{
	if (cmd->input)
		printf("%sinput redirected to %s\n",tabs,cmd->input);
	if (cmd->output)
		printf("%soutput redirected to %s\n",tabs,cmd->output);
	if (cmd->append)
		printf("%soutput appended to %s\n",tabs,cmd->append);
	if (cmd->error)
		printf("%serror redirected to %s\n",tabs,cmd->error);
}

// outputs the structure of the parsed command; useful for debugging
void output (struct cmd *cmd, int indent)
{
	// output formatting
	int i;
	char *tabs = calloc(1,indent+1);
	for (i = 0; i < indent; i++) tabs[i]='\t';

	if (!cmd)
	{
		printf("some parse error occurred\n");
		free(tabs);
		return;
	}

	switch (cmd->type)
	{
	    case C_PLAIN:
		printf("%sa normal command that can be executed\n",tabs);
		output_args(cmd,tabs);
		output_mods(cmd,tabs);
		break;
	    case C_VOID:
		printf("%sa group of commands in parentheses\n",tabs);
		output_mods(cmd,tabs);
		output(cmd->left,indent+1);
		printf("%sparenthese over\n",tabs);
		break;
	    case C_AND:
		if (cmd->type == C_AND)
			printf("%sAND (if the left command succeeds, "
				"execute the right)\n",tabs);
	    case C_OR:
		if (cmd->type == C_OR)
			printf("%sOR (if the left command fails, "
				"execute the right)\n",tabs);
	    case C_PIPE:
		if (cmd->type == C_PIPE)
			printf("%sPIPE (redirect output of the left command "
				"to the right)\n",tabs);
	    case C_SEQ:
		if (cmd->type == C_SEQ)
			printf("%sSEQUENCE (execute the left command, "
				"then the right)\n",tabs);
		printf("%sleft command:\n",tabs);
		output(cmd->left,indent+1);
		printf("%sright command:\n",tabs);
		output(cmd->right,indent+1);
		printf("%sright command over\n",tabs);
		break;
	}

	free(tabs);
}
