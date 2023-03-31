#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>

#define MAXLINE 1024
#define CMDHISTORY 20

#define VARIABLELENGTH 1024
#define MAXVARIABLES 20

#define PROMPTMAXSIZE 20

typedef struct variable {
    char key[VARIABLELENGTH];
    char value[VARIABLELENGTH];
} variable;

int isRunning;
pid_t pid;

void sig_handler(int signum)
{
    if(signum == SIGINT) {
        if (isRunning) {
		    if (kill(pid, SIGINT) == -1)
                perror("kill");
        } else
            printf("You typed Control-C!\n");
	}
}





//int main() {
int main(int argCOUNT, char *argVARIABLES[]) {
    

signal(SIGINT, sig_handler);
int DEBUG = 0;
char command[MAXLINE];
char commands[CMDHISTORY][MAXLINE];
variable variables[MAXVARIABLES];
int commandCount = 0;
int variableCount = 0;
int commandPointer = 0;

char *token;
int i;
char *outfile;
int fd, amper, redirect, redirectAppend, redirecterr, piping, retid, status, argc1;
int fildes[2];
char *argv1[10], *argv2[10];
char prompt[PROMPTMAXSIZE];
strcpy(prompt, "hello:");

// if we got an argument, define debugging mode
if (argCOUNT > 1) {
    if (!strcmp(argVARIABLES[1], "-d")) {
        printf("Debugging mode is on.\n");
        DEBUG = 1;
    }
}


while (1)
{
    /* clear argv1 and 2 */
    for (i = 0; i < 10; i++) {
        argv1[i] = NULL;
        argv2[i] = NULL;
    }
    /* Get input if we are not using arrow keys*/
    if (!commandPointer)
        printf("%s ", prompt);
    fgets(command, 1024, stdin);
    /* if command is null */
    if (command[0] == '\n') {
        // replace command with commands at commandPointer
        strcat(commands[(commandCount-commandPointer)%commandCount], " ");
        strcpy(command, commands[(commandCount-commandPointer)%commandCount]);
    }
    /* if command is arrow up */
    else if (command[0] == 27 && command[1] == 91 && command[2] == 65) {
        commandPointer++;
        printf("%s: %s\n", prompt, commands[(commandCount-commandPointer)%commandCount]);
        continue;
    } 
    /* else if is arrow down */
    else if (command[0] == 27 && command[1] == 91 && command[2] == 66) {
        commandPointer--;
        printf("%s: %s\n", prompt, commands[(commandCount-commandPointer)%commandCount]);
        continue;
    }
    commandPointer = 0;

    /* Is command IF/ELSE bash flow */
    if (command[0] == 'i' && command[1] == 'f') {
        command[strlen(command)-1] = '\n';
        while (1) {
            char currentCodeLine[MAXLINE];
            fgets(currentCodeLine, 1024, stdin);
            strcat(command, currentCodeLine);
            command[strlen(command) - 1] = '\n';
            if (!strcmp(currentCodeLine, "fi\n"))
                break;
        }
        isRunning = 1;
        if (fork()==0) {
            execvp("bash", (char *[]){"bash", "-c", command, NULL});
        } 
        wait(&status);
        isRunning = 0;
        continue;

    }
    command[strlen(command)-1] = '\0';

    /* Replace command if got "!!" */
    if (!strcmp(command, "!!")) {
        if (commandCount > 1) {
            strcpy(command, commands[commandCount - 2]);
        } else {
            printf("No commands in history.\n");
            continue;
        }
    }
    /* Manage command history */
    if (commandCount < CMDHISTORY) {
        strcpy(commands[commandCount], command);
        commandCount++;
    } else {
        for (int i = 0; i < CMDHISTORY - 1; i++) {
            strcpy(commands[i], commands[i + 1]);
        }
        strcpy(commands[CMDHISTORY - 1], command);
    }
    piping = 0;
    
    /* parse command line */
    i = 0;
    token = strtok (command, " ");
    while (token != NULL)
    {
        argv1[i] = token;
        token = strtok (NULL, " ");
        i++;
        if (token && ! strcmp(token, "|")) {
            piping += 1;
            
        }
    }
    argv1[i] = NULL;
    argc1 = i;

    /* Is command empty */
    if (argv1[0] == NULL)
        continue;
    /* Is command exit */
    else if (!strcmp(argv1[0], "quit")) {
        printf("\n"); 
        return 0;
    }
    /* Is command cd */
    else if (!strcmp(argv1[0], "cd")) {
        if (argv1[1] == NULL)
            chdir(getenv("HOME"));
        else
            chdir(argv1[1]);
        continue;
    }
    /* Is command echo */
    else if (!strcmp(argv1[0], "echo")) {
        if (argv1[1] == NULL)
            printf("");
        else if (!strcmp(argv1[1], "$?"))
            printf("%d\n", WEXITSTATUS(status));
        else {
            for (int j = 1; j < argc1; j++) {
                int flag = 0;
                if (argv1[j][0] == '$') {
                    // PRINT VARIABLE VALUE
                    for (int k = 0; k < variableCount; k++) {
                        if (!strcmp(argv1[j], variables[k].key)) {
                            printf("%s ", variables[k].value);
                            flag = 1;
                            break;
                        }
                    }
                    if (!flag)
                        printf("%s ", argv1[j]);
                } else {
                    printf("%s ", argv1[j]);
                }
            }
            printf("\n");
        }
    continue;
    /* Is command prompt = NewPromptPlaceHolder */
    } else if (!strcmp(argv1[0], "prompt") && !strcmp(argv1[1], "=")) {
        if (argv1[2] == NULL)
            printf("missing prompt\n");
        else {
            strcpy(prompt, argv1[2]);
        }
    continue;
    /* Is command set up new variable*/
    } else if (argv1[0][0] == '$' && argv1[1] && !strcmp(argv1[1], "=")) {
        if (argv1[2] == NULL)
            printf("missing variable value\n");
        else if (variableCount < MAXVARIABLES) {
            strcpy(variables[variableCount].key ,argv1[0]);
            strcpy(variables[variableCount].value, argv1[2]);
            variableCount++;
        } else {
            printf("Too many variables\n");
        }
    continue;
    /* Is command read */
    } else if (!strcmp(argv1[0], "read")) {
        if (argv1[1] == NULL) {
            printf("missing variable name\n");
        } else {
            char input[VARIABLELENGTH];
            fgets(input, VARIABLELENGTH, stdin);
            input[strlen(input) - 1] = '\0';
            if (variableCount < MAXVARIABLES) {
                char str[VARIABLELENGTH];
                strcpy(str, "$");
                strcat(str, argv1[1]);
                strcpy(variables[variableCount].key , str);
                strcpy(variables[variableCount].value, input);
                variableCount++;
            } else {
                printf("Too many variables\n");
            }

        }
    continue;
    }
    /* Does command contain pipe */
    if (piping) {
        i = 0;
        while (token!= NULL)
        {
            token = strtok (NULL, " ");
            argv2[i] = token;
            i++;
        }
        argv2[i] = NULL;
    }

    /* Does command line end with & */ 
    if (!strcmp(argv1[argc1 - 1], "&")) {
        amper = 1;
        argv1[argc1 - 1] = NULL;
        }
    else 
        amper = 0; 

    /* Does command line contaisn > or >> or 2> */
    if (argc1 > 1 && !strcmp(argv1[argc1 - 2], "2>")) {
        redirecterr = 1;
        redirect = 0;
        redirectAppend = 0;
        argv1[argc1 - 2] = NULL;
        outfile = argv1[argc1 - 1];
    }
    else if (argc1 > 1 && !strcmp(argv1[argc1 - 2], ">>")) {
        redirectAppend = 1;
        redirect = 0;
        redirecterr = 0;
        argv1[argc1 - 2] = NULL;
        outfile = argv1[argc1 - 1];
    }
    else if (argc1 > 1 && !strcmp(argv1[argc1 - 2], ">")) {
        redirect = 1;
        redirectAppend = 0;
        redirecterr = 0;
        argv1[argc1 - 2] = NULL;
        outfile = argv1[argc1 - 1];
        }
    else { 
        redirecterr = 0;
        redirect = 0;
        redirectAppend = 0;
    }
    if (DEBUG==1)
        printf("\n\nvariableCount: %d, commandCount: %d, redirect: %d, append: %d, redirecterr: %d, outfile: %s, amper: %d, piping: %d\n\n", variableCount,commandCount, redirect, redirectAppend, redirecterr ,outfile, amper, piping);
    isRunning = 1;
    /* for commands not part of the shell command language */ 
    pid = fork();
    if (pid == 0) { 
        /* redirection of IO ? */
        if (redirect || redirectAppend) {
            if (redirect) {
                fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
            }
            else {
                fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0660);
            }
            close (STDOUT_FILENO) ; 
            dup(fd); 
            close(fd); 
            /* stdout is now redirected */
        } else if (redirecterr) {
            fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0660);
            close (STDERR_FILENO) ; 
            dup(fd); 
            close(fd); 
            /* stderr is now redirected */
        }
        if (piping) {
            while (piping > 0) {
                if (pipe(fildes) == -1) {
                    perror("Pipe:");
                }
                int first_proc_pid = fork();
                if (first_proc_pid == 0) {
                    /* first component of command line */
                    //printf("first child process\n");
                    close(STDOUT_FILENO); // 1
                    /* Duplicate the output side of pipe to stdout */
                    dup(fildes[1]);
                    //printf("pipeline fd duplicated to stdout!\n");
                    close(fildes[1]);
                    close(fildes[0]);
                    /* stdout now goes to pipe */ 
                    /* child process does command */ 
                    if (execvp(argv1[0], argv1) == -1) {
                        printf("Failed to execute %s\n", argv1[0]);
                        exit(-1);
                    }
                } 
                /* 2nd command component of command line */ 
                close(STDIN_FILENO); // 0
                /* Duplicate the input side of pipe to stdin */
                dup(fildes[0]);
                close(fildes[0]);
                close(fildes[1]);
                /* standard input now comes from pipe */ 
                execvp(argv2[0], argv2);
                piping--;
                // Two programs finished executing 
                // while the first redirected its output
                // to the second
            }
        } 
        else {
            //printf("execvp(%s, %s)", argv1[0], argv1);
            execvp(argv1[0], argv1);
        }
    }
    /* parent continues over here... */
    /* waits for child to exit if required */
    if (amper == 0)
        retid = wait(&status);
    isRunning = 0;

    
}
}
