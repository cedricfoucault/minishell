#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>

#include "global.h"

// MAXDIGIT_INT: the maximum number of digits of int written in base 10
// in a 64-bit machine
#define MAXDIGIT_INT 19 

int execute (struct cmd *cmd);
int executeAux (struct cmd *cmd);
void propagate (struct cmd *cmd);

int main (int argc, char **argv) {
    printf("welcome to lsvsh!\n");

    // Initialize environment variables

    // PWD variable to store the present working directory
    char cwd[MAXPATHLEN + 1];

    if (getcwd(cwd, MAXPATHLEN) == NULL) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        fprintf(stderr, "cannot get current working directory\n");
        exit (-1);
    }
    if (setenv("PWD", cwd, 1)) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        fprintf(stderr, "cannot create $?\n");
        exit (-1);
    }

    // "?" variable to store the last command's exit value
    if (setenv("?", "0", 1)) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        fprintf(stderr, "cannot create $?\n");
        exit (-1);
    }

    // Ignore SIGINT (CTRL-C)
    if (signal(SIGINT, SIG_IGN) == SIG_ERR) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(-1);
    }


    while (1) {
        int exitval;
        char exitstr[MAXDIGIT_INT];

        char *line = readline ("shell> ");
        if (!line) break;	// user pressed CTRL+D; quit shell
        if (!*line) continue;	// empty line

        add_history (line);	// add line to history

        struct cmd *cmd = parser(line);
        if (!cmd) continue;	// some parse error occurred; ignore
        
        exitval = execute(cmd);
        if (exitval == SIGINT) {
            // print a newline if the program terminated with SIGINT
            printf("\n");
        }

        // maintain the "?" variable
        sprintf(exitstr, "%d", exitval);
        if (setenv("?", exitstr, 1)) {
            fprintf(stderr, "error: %s\n", strerror(errno));
            fprintf(stderr, "cannot update $?\n");
            exit (-1);
        }
    }

    printf("goodbye!\n");
    return 0;
}

int execute (struct cmd *cmd) {
    propagate(cmd);
    return executeAux(cmd);
}

int executeAux (struct cmd *cmd) {
    int retval; // return value of execute
    int in, out, err; // used to restore the process's stdin, stdout, stderr at the end of the execution

    in = dup(0);
    if (in == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit (-1);
    }
    out = dup(1);
    if (out == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit (-1);
    }
    err = dup(2);
    if (err == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(-1);
    }

    switch (cmd->type) {
        case C_PLAIN:
        // builtin "cd" command (change directory)
        if (strcmp(cmd->args[0], "cd") == 0) {
            // builtin "cd" (change directory)
            char cwd[MAXPATHLEN + 1];

            if (chdir(cmd->args[1]) == -1) {
                fprintf(stderr, "error: %s\n", strerror(errno));
                retval = -1;
                break;
            }

            if (getcwd(cwd, MAXPATHLEN) == NULL) {
                fprintf(stderr, "error: %s\n", strerror(errno));
                retval = -1;
                break;
            }

            if (setenv("PWD", cwd, 1)) {
                fprintf(stderr, "error: %s\n", strerror(errno));
                fprintf(stderr, "cannot create $?\n");
                retval = -1;
                break;
            }

            retval = 0; 
            break;
        }

        // external program
        if (fork()) {
            // father - wait for child to terminate
            int statval;

            if (wait(&statval) == -1) 
            {
                fprintf(stderr, "error: %s\n", strerror(errno));   
                retval = -1;
                break;
            }

            if (WIFEXITED(statval)) { // termination by call to exit
                // return the child's exit value
                retval = WEXITSTATUS(statval);
                break;
            } else if (WIFSIGNALED(statval)) { // termination by signal
                // return the signal's value
                retval = WTERMSIG(statval);
                break;
            } else {
                fprintf(stderr, "error: child process did not terminate with exit or due to the receipt of a signal\n");
                retval = -1;
                break;
            }
        } else {	
            // child - execute the command
            mode_t mask, filemode;

            // restore SIGINT's default action
            if (signal(SIGINT, SIG_DFL) == SIG_ERR) {
                fprintf(stderr, "error: %s\n", strerror(errno));
                exit(-1);
            }

            // handle the redirections

            // get the current umask value
            mask = umask(0);
            umask (mask);
            // compute the actual file permission mode
            filemode = (0666) ^ mask;

            if (cmd->input) {
                int input = open(cmd->input, O_RDONLY);

                if (input == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                if (dup2(input, 0) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                if (close(input) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
            }

            if (cmd->output) {
                int output = open(cmd->output, O_WRONLY| O_TRUNC | O_CREAT, filemode);

                if (output == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                if (dup2(output, 1) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                if (close(output) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
            }

            if (cmd->append) {
                int append = open(cmd->append, O_WRONLY | O_APPEND | O_CREAT, filemode);

                if (append == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                if (dup2(append, 1) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1); 
                }
                if (close(append) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }       
            }

            if (cmd->error) {
                int error = open(cmd->error, O_WRONLY | O_TRUNC | O_CREAT, filemode);

                if (error == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                if (dup2(error, 2) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                if (close(error) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
            }

            // execute the command
            execvp(cmd->args[0],cmd->args);

            // if we get to this line, the command has failed
            fprintf(stderr, "error: %s\n", strerror(errno));
            exit(-1);
        }

        case C_VOID:
        retval = executeAux(cmd->left);
        break;

        case C_AND: {
            int error;  

            error = executeAux(cmd->left);
            if (!error) {
                retval = executeAux(cmd->right);
            } else {
                retval = error;
            }
            break;
        }

        case C_OR: {
            int error;  

            error = executeAux(cmd->left);
            if (error) {
                retval = executeAux(cmd->right);
            } else {
                retval = error;
            }
            break;
        }

        case C_PIPE: {
            int filepipe[2];

            if (pipe(filepipe) == -1) {
                fprintf(stderr, "error: %s\n", strerror(errno));
                exit(-1);
            }
            if (fork()) {
                // parent - handles the right command of the pipe
                int statval;

                // close the unused part of the pipe
                if (close(filepipe[1]) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    retval = -1; 
                    break;
                }
                // replace stdin with the read part of the pipe
                if (dup2(filepipe[0], 0) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    retval = -1; 
                    break;
                }
                // close the unused copy
                if (close(filepipe[0]) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    retval = -1; 
                    break;
                }
                // execute the right command
                retval = executeAux(cmd->right);
                // wait for child to terminate
                if (wait(&statval) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));   
                    retval = -1;
                    break;
                }
                if (WIFEXITED(statval)) {
                    // return the child's exit value
                    retval = retval || WEXITSTATUS(statval);
                    break;
                } else {
                    fprintf(stderr, "error: child process did not terminate with exit \n");
                    retval = -1;
                    break;
                }
            } else {
                // child - handles the left command of the pipe

                // close the unused part of the pipe
                if (close(filepipe[0]) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                // replace stdout with the output part of the pipe
                if (dup2(filepipe[1], 1) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                // close the unused copy
                if (close(filepipe[1]) == -1) {
                    fprintf(stderr, "error: %s\n", strerror(errno));
                    exit(-1);
                }
                // execute the left command
                exit(executeAux(cmd->left));
            }
        }

        case C_SEQ:
        executeAux(cmd->left);
        retval = executeAux(cmd->right);
        break;
    }

    if (dup2(in, 0) == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(-1);
    }
    if (close(in) == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(-1);
    }
    if (dup2(out, 1) == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(-1);
    }
    if (dup2(err, 0) == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(-1);
    }
    if (close(err) == -1) {
        fprintf(stderr, "error: %s\n", strerror(errno));
        exit(-1);
    }

    return retval;
}

// This function is used to propagate properly the redirections to the subcommands
void propagate (struct cmd *cmd) {
    switch (cmd->type) {
        // the command has no subcommand
        case C_PLAIN:
        return;

        // handle the pipes properly: don't interfer with the pipe's in/out
        case C_PIPE:
        if (cmd->output && !(cmd->right)->output) {
            (cmd->right)->output = cmd->output;
        }
        if (cmd->append && !(cmd->right)->append) {
            (cmd->right)->append = cmd->append;
        }
        if (cmd->error && !(cmd->right)->error) {
            (cmd->right)->error = cmd->error;
        }
        propagate(cmd->right);

        if (cmd->input && !(cmd->left)->input) {
            (cmd->left)->input = cmd->input;
        }
        if (cmd->error && !(cmd->left)->error) {
            (cmd->left)->error = cmd->error;
        }
        propagate(cmd->left);

        break;


        // the command has a right subcommand
        case C_AND: case C_OR: case C_SEQ:
        if (cmd->input && !(cmd->right)->input) {
            (cmd->right)->input = cmd->input;
        }
        if (cmd->output && !(cmd->right)->output) {
            (cmd->right)->output = cmd->output;
        }
        if (cmd->append && !(cmd->right)->append) {
            (cmd->right)->append = cmd->append;
        }
        if (cmd->error && !(cmd->right)->error) {
            (cmd->right)->error = cmd->error;
        }
        propagate(cmd->right);

        // the command has a left subcommand
        default:
        if (cmd->input && !(cmd->left)->input) {
            (cmd->left)->input = cmd->input;
        }
        if (cmd->output && !(cmd->left)->output) {
            (cmd->left)->output = cmd->output;
        }
        if (cmd->append && !(cmd->left)->append) {
            (cmd->left)->append = cmd->append;
        }
        if (cmd->error && !(cmd->left)->error) {
            (cmd->left)->error = cmd->error;
        }
        propagate(cmd->left);
    }
}
