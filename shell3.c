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
int fd, amper, redirect, redirectAppend, redirecterr, piping, retid, status, argc1[10];
int fildes[2];
char *argv1[10][10], *argv2[10];
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
    for (i = 0; i < 10; i++) 
        for (int j = 0; j < 10; j++)
            argv1[i][j] = NULL;
    // clear command
    memset(command, 0, sizeof(command));
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
            strcpy(command, commands[commandCount - 1]);
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
    int num_commands = 0;
    char command2[MAXLINE];
    strcpy(command2, command);
    token = strtok (command, " ");
    while (token != NULL)
    {
        argv1[num_commands][i] = token;
        token = strtok (NULL, " ");
        i++;
        if (token && ! strcmp(token, "|")) {
            piping++;
            token = strtok (NULL, " ");

            argv1[num_commands][i] = NULL;
            argc1[num_commands] = i;
            i = 0;
            num_commands++;
            // break;
        }
    }
    argv1[num_commands][i] = NULL;
    argc1[num_commands] = i;
    num_commands++;
    if (piping) {
        if (fork()==0) {
            execvp("bash", (char *[]){"bash", "-c", command2, NULL});
        } 
        wait(&status);
        continue;
    }

    /* Is command empty */
    if (argv1[0][0] == NULL)
        continue;
    /* Is command exit */
    else if (!strcmp(argv1[0][0], "quit")) {
        printf("\n"); 
        return 0;
    }
    /* Is command cd */
    else if (!strcmp(argv1[0][0], "cd")) {
        if (argv1[0][1] == NULL)
            chdir(getenv("HOME"));
        else
            chdir(argv1[0][1]);
        continue;
    }
    /* Is command echo */
    else if (!strcmp(argv1[0][0], "echo")) {
        if (argv1[0][1] == NULL)
            printf("");
        else if (!strcmp(argv1[0][1], "$?"))
            printf("%d\n", WEXITSTATUS(status));
        else {
            for (int j = 1; j < argc1[0]; j++) {
                int flag = 0;
                if (argv1[0][j][0] == '$') {
                    // PRINT VARIABLE VALUE
                    for (int k = 0; k < variableCount; k++) {
                        if (!strcmp(argv1[0][j], variables[k].key)) {
                            printf("%s ", variables[k].value);
                            flag = 1;
                            break;
                        }
                    }
                    if (!flag)
                        printf("%s ", argv1[0][j]);
                } else {
                    printf("%s ", argv1[0][j]);
                }
            }
            printf("\n");
        }
    continue;
    /* Is command prompt = NewPromptPlaceHolder */
    } else if (!strcmp(argv1[0][0], "prompt") && !strcmp(argv1[0][1], "=")) {
        if (argv1[0][2] == NULL)
            printf("missing prompt\n");
        else {
            strcpy(prompt, argv1[0][2]);
        }
    continue;
    /* Is command set up new variable*/
    } else if (argv1[0][0][0] == '$' && argv1[0][1] && !strcmp(argv1[0][1], "=")) {
        if (argv1[0][2] == NULL)
            printf("missing variable value\n");
        else if (variableCount < MAXVARIABLES) {
            strcpy(variables[variableCount].key ,argv1[0][0]);
            strcpy(variables[variableCount].value, argv1[0][2]);
            variableCount++;
        } else {
            printf("Too many variables\n");
        }
    continue;
    /* Is command read */
    } else if (!strcmp(argv1[0][0], "read")) {
        if (argv1[0][1] == NULL) {
            printf("missing variable name\n");
        } else {
            char input[VARIABLELENGTH];
            fgets(input, VARIABLELENGTH, stdin);
            input[strlen(input) - 1] = '\0';
            if (variableCount < MAXVARIABLES) {
                char str[VARIABLELENGTH];
                strcpy(str, "$");
                strcat(str, argv1[0][1]);
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
    // if (piping) {
    //     i = 0;
    //     while (token!= NULL)
    //     {
    //         token = strtok (NULL, " ");
    //         argv2[i] = token;
    //         i++;
    //     }
    //     argv2[i] = NULL;
    // }

    /* Does command line end with & */ 
    if (!strcmp(argv1[0][argc1[0] - 1], "&")) {
        amper = 1;
        argv1[0][argc1[0] - 1] = NULL;
        }
    else 
        amper = 0; 

    /* Does command line contaisn > or >> or 2> */
    if (argc1[0] > 1 && !strcmp(argv1[0][argc1[0] - 2], "2>")) {
        redirecterr = 1;
        redirect = 0;
        redirectAppend = 0;
        argv1[0][argc1[0] - 2] = NULL;
        outfile = argv1[0][argc1[0] - 1];
    }
    else if (argc1[0] > 1 && !strcmp(argv1[0][argc1[0] - 2], ">>")) {
        redirectAppend = 1;
        redirect = 0;
        redirecterr = 0;
        argv1[0][argc1[0] - 2] = NULL;
        outfile = argv1[0][argc1[0] - 1];
    }
    else if (argc1[0] > 1 && !strcmp(argv1[0][argc1[0] - 2], ">")) {
        redirect = 1;
        redirectAppend = 0;
        redirecterr = 0;
        argv1[0][argc1[0] - 2] = NULL;
        outfile = argv1[0][argc1[0] - 1];
        }
    else { 
        redirecterr = 0;
        redirect = 0;
        redirectAppend = 0;
    }
    if (DEBUG==1) {
        printf("\n\nvariableCount: %d, commandCount: %d, redirect: %d, append: %d, redirecterr: %d, outfile: %s, amper: %d, piping: %d\n\n", variableCount,commandCount, redirect, redirectAppend, redirecterr ,outfile, amper, piping);
        printf("argvs:\n");
        for (int i = 0; i < num_commands; i++) {
            for (int j = 0; j < argc1[i]; j++) {
                printf("%s ", argv1[i][j]);
            }
            printf("\n");
        }
    }
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
        int i = 0;


        //execvp("bash", (char *[]){"bash", "-c", command, NULL});
        //wait(NULL);
        continue;

        int pipe_fds[20]; // maximum number of pipes needed = 2*(num_commands-1)
        for (i = 0; i < 2*(num_commands-1); i+=2) {
            if (pipe(pipe_fds + i) == -1) {
                perror("Pipe:");
            }
        }

        for (i = 0; i < num_commands; i++) {
            int pid = fork();
            if (pid == 0) {
                // child process
                if (i != 0) {
                    // not first command, redirect input from previous pipe
                    close(STDIN_FILENO);
                    dup(pipe_fds[(i-1)*2]);
                    close(pipe_fds[(i-1)*2]);
                    close(pipe_fds[(i-1)*2 + 1]);
                }
                if (i != num_commands-1) {
                    // not last command, redirect output to next pipe
                    close(STDOUT_FILENO);
                    dup(pipe_fds[i*2 + 1]);
                    close(pipe_fds[i*2]);
                    close(pipe_fds[i*2 + 1]);
                }
                // execute command
                execvp(argv1[i][0], argv1[i]);
                perror("Exec:");
                exit(-1);
            } else if (pid < 0) {
                perror("Fork:");
                exit(-1);
            }
        }

        // parent process
        for (i = 0; i < 2*(num_commands-1); i++) {
            close(pipe_fds[i]);
        }


    } else {
        // no piping, execute single command
        execvp(argv1[0][0], argv1[0]);
        perror("Exec:");
    }



    }
    /* parent continues over here... */
    /* waits for child to exit if required */
    if (amper == 0)
        retid = wait(&status);
    isRunning = 0;

    
}
printf("\nexiting shell\n");
}
