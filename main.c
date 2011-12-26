#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>

#include "global.h"

// This is the function that you should work on.
// It takes a command parsed at the command line.
// The structure of the command is explained in output.c.
//
// Currently, the procedure can only execute simple commands and simply
// emits error messages if the user has entered something more complicated.
//
void execute (struct cmd *cmd)
{
	switch (cmd->type)
	{
	    case C_PLAIN:
		if (cmd->input || cmd->output || cmd->append || cmd->error)
		{
			fprintf(stderr,"I do not know how to redirect, "
			                           "please help me!\n");
			         
			break;
		}

		if (fork())
		{
			// father - wait for child to terminate
			wait(NULL);
		}
		else
		{	// child - execute the command
			execvp(cmd->args[0],cmd->args);
			// if we get to this line, the command has failed
			fprintf(stderr,"error: %s\n", strerror(errno));
			exit(-1);
		}
		break;
	    case C_VOID:
	    if (cmd->input || cmd->output || cmd->append || cmd->error)
		{
			fprintf(stderr,"I do not know how to redirect, "
					"please help me!\n");
			break;
		}

		if (fork())
		{
			// father - wait for child to terminate
			wait(NULL);
		}
		else
		{	// child - execute the command
			execute(cmd->left);
			// if we get to this line, the command has failed
			fprintf(stderr,"error: %s\n", strerror(errno));
			exit(-1);
		}
	    case C_AND:
        execute(cmd->left);
	    if (!error) {
            execute(cmd->right);
	    }
        break;
	    case C_OR:
        execute(cmd->left);
        if (error) {
            execute(cmd->right);
        }
        break;
	    case C_PIPE:
	    case C_SEQ:
        execute(cmd->left);
        execute(cmd->right);
		fprintf(stderr,"I do not know how to do this, "
				"please help me!\n");
		break;
	}
}

int main (int argc, char **argv)
{
	printf("welcome to lsvsh!\n");

	while (1)
	{
		char *line = readline ("shell> ");
		if (!line) break;	// user pressed CTRL+D; quit shell
		if (!*line) continue;	// empty line

		add_history (line);	// add line to history

		struct cmd *cmd = parser(line);
		if (!cmd) continue;	// some parse error occurred; ignore
		// LINE TO SWItCH BETWEEN DISPLAYING OUTPUT AND EXECUtE
        output(cmd,0);      // activate this for debugging
        // execute(cmd);
	}

	printf("goodbye!\n");
	return 0;
}
