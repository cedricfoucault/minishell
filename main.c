#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

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
        // if (cmd->input || cmd->output || cmd->append || cmd->error)
        // {
        // 
        //     fprintf(stderr,"I do not know how to redirect, "
        //         "please help me!\n");
        //     break;
        // }
        if (fork())
        {
            // father - wait for child to terminate
            if (wait(NULL) == -1) 
            {
                fprintf(stderr,"error: %s\n", strerror(errno));   
                exit(-1);
            }
        }
        else
        {	// child - execute the command

            // handle the redirections
            if (cmd->input) 
            {
                int input = open(cmd->input, O_RDONLY);

                if (input != -1) 
                {
                    if (dup2(input, 0) == -1)
                    {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    close(input);
                } 
                else 
                {
                    fprintf(stderr,"error: %s\n", strerror(errno));
                    exit(-1);
                }
            }

            if (cmd->output) 
            {
                int output = open(cmd->output, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR |  S_IWUSR | S_IRGRP | S_IROTH);

                if (output != -1) 
                {
                    if (dup2(output, 1) == -1)
                    {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    close(output);
                } 
                else 
                {
                    fprintf(stderr,"error: %s\n", strerror(errno));
                    exit(-1);
                }
            }

            if (cmd->append) 
            {
                int append = open(cmd->append, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR |  S_IWUSR | S_IRGRP | S_IROTH);

                if (append != -1) 
                {
                    if (dup2(append, 1) == -1)
                    {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    close(append);
                }
                else 
                {
                    fprintf(stderr,"error: %s\n", strerror(errno));
                    exit(-1);
                }
            }

            if (cmd->error) {
                int error = open(cmd->error, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR |  S_IWUSR | S_IRGRP | S_IROTH);

                if (error != -1) {
                    if (dup2(error, 2) == -1) 
                    {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    close(error);
                } 
                else 
                {
                    fprintf(stderr,"error: %s\n", strerror(errno));
                    exit(-1);
                }
            }

            // execute the command
            execvp(cmd->args[0],cmd->args);

            // if we get to this line, the command has failed
            fprintf(stderr,"error: %s\n", strerror(errno));
            exit(-1);
        }
        break;
        case C_VOID:
        // if (cmd->input || cmd->output || cmd->append || cmd->error)
        // {
        //     fprintf(stderr,"I do not know how to redirect, "
        //         "please help me!\n");
        //     break;
        // }
        // 
        // if (fork())
        // {
        //     // father - wait for child to terminate
        //     wait(NULL);
        // }
        // else
        // {    // child - execute the command
        //     execute(cmd->left);
        //     // if we get to this line, the command has failed
        //     fprintf(stderr,"error: %s\n", strerror(errno));
        //     exit(-1);
        // }
        case C_AND:
        // execute(cmd->left);
        // if (!error) {
        //     execute(cmd->right);
        // }
        break;

        case C_OR:
        // execute(cmd->left);
        // if (error) {
        //     execute(cmd->right);
        // }
        break;

        case C_PIPE:
        {
            int filepipe[2];

            if (pipe(filepipe) == -1) {
                fprintf(stderr,"error: %s\n", strerror(errno));
                exit(-1);
            }
            if (fork()) {
                 // father - wait for child to terminate
                if (wait(NULL) == -1) 
                {
                    fprintf(stderr,"error: %s\n", strerror(errno));   
                    exit(-1);
                }
            } else {
                if (fork()) {
                    // first child - handles the left command of the pipe
                    // replace stdout with the output part of the pipe
                    if (dup2(filepipe[1], 1) == -1) {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    // close the unused file descriptors
                    if (close(filepipe[0]) == -1) {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    if (close(filepipe[1]) == -1) {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    // execute the left command
                    execute(cmd->left);
                    close(1); // if ...
                    close(0); // if ...
                } else {
                    // second child - handles the right command of the pipe
                    // replace stdin with the read part of the pipe
                    if (dup2(filepipe[0], 0) == -1) {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    // close the unused file descriptors
                    if (close(filepipe[0]) == -1) {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    if (close(filepipe[1]) == -1) {
                        fprintf(stderr,"error: %s\n", strerror(errno));
                        exit(-1);
                    }
                    // execute the left command
                    execute(cmd->right);
                    close(0); // if error ...
                    close(1); // if error ...
                }
            }
            break;
        }

        case C_SEQ:
        execute(cmd->left);
        execute(cmd->right);
        // fprintf(stderr,"I do not know how to do this, "
        //     "please help me!\n");
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
        // LINE TO SWITCH BETWEEN DISPLAYING OUTPUT AND EXECUtE
        // output(cmd,0);      // activate this for debugging
        execute(cmd);
    }

    printf("goodbye!\n");
    return 0;
}
