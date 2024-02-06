#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <sys/wait.h>

// printf("Just about to start look, i: %d\n", i); fflush(stdout);
// do parant process add list 

//declare some global variables
#define MAX_LENGTH 2048
#define MAX_ARGS 512

// maximum number of background processes
#define MAX_BACK_PROCESS 100  

// array of background process IDs
pid_t backgroundPIDs[MAX_BACK_PROCESS];
// skip checking background processes
int skipBackgroundCheck = 0;

// current number of background processes
int numBackProcess = 0;  
// foreground process has exited
int exitedForeground = 0;

// SIGINT received
volatile sig_atomic_t sigintReceived = 0;

// foreground-only mode
int foregroundOnly = 0;

// struct for command
struct command {
    char* argv[MAX_ARGS];
    char* inputFile;
    char* outputFile;
    int background;
};

// declare functions
void executeCommand(struct command* cmd, int* lastStatus);
int  executeBuiltIn(struct command* cmd, int* lastStatus);
void handleSIGTSTP(int signo);
void handleSIGINT(int signo);
//void handleSIGCHLD(int signo);
void freeCommand(struct command* cmd);
char* readCommandLine();
void parseCommand(char* inputLine, struct command* cmd);
//void replaceDollar(char* token, char* pidStr);
void replaceDollar(char* token, char* pidStr, char* newToken) ;
void exitCommand();
void checkBackground();

// signal handlers
struct sigaction SIGINT_action = {0}, SIGTSTP_action = {0}, SIGCHLD_action = {0};

int main() {

    // ignore SIGINT in the shell itself
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT, &SIGINT_action, NULL);

    // handle SIGTSTP to toggle foreground-only mode
    SIGTSTP_action.sa_handler = handleSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // SIGCHLD_action.sa_handler = handleSIGCHLD;
    // sigfillset(&SIGCHLD_action.sa_mask);
    // SIGCHLD_action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    // sigaction(SIGCHLD, &SIGCHLD_action, NULL);

    //variables
    char* inputLine;
    struct command cmd;
    int lastStatus = 0;

    while (1) {
        // printf(": ");  //prompt
        // fflush(stdout);

        // Check if SIGINT was received
        if (sigintReceived) {
            printf("\nCaught SIGINT\n: ");  // Handle SIGINT as needed
            fflush(stdout);
            sigintReceived = 0;  // Reset the flag
        }

        // read the user's input
        inputLine = readCommandLine();

        // skip if it's a comment or blank line
        if (inputLine == NULL){
            continue;   
        }  

        memset(&cmd, 0, sizeof(struct command));
        parseCommand(inputLine, &cmd);

        // execute the command
        if (!executeBuiltIn(&cmd, &lastStatus)) {
            executeCommand(&cmd, &lastStatus);
        }
       
        // check if the shell should exit
        if (exitedForeground != 0) {
            
            // reset the flag after the first prompt
            exitedForeground = 0;  

        } else if (skipBackgroundCheck == 0) {
            
            // check background processes only if appropriate
            checkBackground();  
        }

        // free the command
        freeCommand(&cmd);
        free(inputLine);
    }

    return 0;
}

// signal handlers
// signit
void handleSIGINT(int signo) {
    // char* message = "Caught SIGINT, but ignored in shell\n";
    // write(STDOUT_FILENO, message, strlen(message));

    // check if SIGINT was received
    sigintReceived = 1;
}

//sigtstp
void handleSIGTSTP(int signo) {
    if (!foregroundOnly) {
        // turn on foreground-only mode
        char* message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, strlen(message));
        foregroundOnly = 1;
        printf("Success! Entered foreground-only mode\n");
        fflush(stdout);
        printf(": ");
        fflush(stdout);

        // change the check status
        skipBackgroundCheck = 1;
        exitedForeground = 0;

    } else {
        // turn off foreground-only mode
        char* message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, strlen(message));
        foregroundOnly = 0;
        printf("Bye! Exited foreground-only mode\n");
        fflush(stdout);
        printf(": ");
        fflush(stdout);

        // change the check status
        skipBackgroundCheck = 1;
        exitedForeground = 1;
    }

    
}

// void handleSIGCHLD(int signo) {
//     int childStatus;
//     pid_t childPid;
//     int lastStatus = 0;

//     // Check if any child processes have terminated
//     while ((childPid = waitpid(-1, &childStatus, WNOHANG)) > 0) {
//         // Update the last status
//         lastStatus = childStatus;

//         // print some debug information:
//         if (WIFEXITED(childStatus)) {
//             printf("\nBackground PID is %d, \nexit status: %d\n", childPid, WEXITSTATUS(childStatus));
//             fflush(stdout);

//         // print some debug information:
//         } else if (WIFSIGNALED(childStatus)) {
//             printf("\nBackground PID is %d \nterminated by signal: %d\n", childPid, WTERMSIG(childStatus));
//             fflush(stdout);
//         }
//         //fflush(stdout);
//     }
// }

// check background processes
void checkBackground() {
    int childStatus;
    pid_t childPid;

    // loop through the array of background PIDs
    for (int i = 0; i < numBackProcess; i++) {
        
        // check array of background PIDs
        childPid = waitpid(backgroundPIDs[i], &childStatus, WNOHANG);

        // check if the process has terminated
        if (childPid > 0) {

            // Process has terminated
            // print the exit status or the terminating signal
            if (WIFEXITED(childStatus)) {
                printf("\nBackground PID is %d \nexited, exit status %d\n", childPid, WEXITSTATUS(childStatus));

            } else if (WIFSIGNALED(childStatus)) {
                printf("\nBackground PID is %d \nterminated by signal %d\n", childPid, WTERMSIG(childStatus));
            }

            // remove this PID from the array
            for (int j = i; j < numBackProcess - 1; j++) {
                backgroundPIDs[j] = backgroundPIDs[j + 1];
            }

            // decrement the number of background processes
            numBackProcess--;
            
            // adjust the index because removed an element
            i--; 
        }
    }
}

// read command line
char* readCommandLine() {
  char* buffer = NULL;
  size_t bufsize = 0;

  // check if SIGINT was received
  if (sigintReceived) {
    return NULL;
  }

  // print the prompt
  printf(": ");
  fflush(stdout);

  // read the user's input
  getline(&buffer, &bufsize, stdin);

  // Remove newline character at the end
  buffer[strcspn(buffer, "\n")] = 0;

  // check if the line is a comment or blank
  if (buffer[0] == '#' || buffer[0] == '\0') {
    free(buffer);
    return NULL;
  }
  
  // return the line
  return buffer;
}

// execute command
void executeCommand(struct command* cmd, int* lastStatus) {
    // fork a new process
    pid_t spawnPid = fork();

    // check if the process is a child
    switch (spawnPid) {
        
        // Error will be printed if fork fails
        case -1:
            perror("fork");
            fflush(stdout);
            exit(1);
            
            break;
            
        // Child process will execute the command
        case 0:

            // handle SIGINT
            if (!cmd->background) {
                SIGINT_action.sa_handler = SIG_DFL;
                sigaction(SIGINT, &SIGINT_action, NULL);
            }

            // set up input / output redirection
            if (cmd->inputFile) {
                int inFD = open(cmd->inputFile, O_RDONLY);

                // if the file cannot be opened, print an error message
                if (inFD == -1) {
                    perror("Failed, input file open error");
                    fflush(stdout);
                    exit(1);
                }

                // redirect stdin to the input file
                if (dup2(inFD, 0) == -1) {
                    perror("dup2 input");
                    fflush(stdout);
                    exit(2);
                }

                // close the file
                close(inFD);
            }

            // redirect stdout to the output file
            if (cmd->outputFile) {

                int outFD = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

                // if the file cannot be opened, print an error message
                if (outFD == -1) {
                    perror("output file open");
                    fflush(stdout);
                    exit(1);
                }

                // redirect stdout to the output file
                if (dup2(outFD, 1) == -1) {
                    perror("dup2 output");
                    fflush(stdout);
                    exit(2);
                }
                close(outFD);
            }

            // execute the command
            execvp(cmd->argv[0], cmd->argv);
            fprintf(stderr, "smallsh: unable to excute: %s\n", cmd->argv[0]);
            fflush(stdout);
            exit(1);
            break;

        default:
            // parent process: Wait for child's termination
            if (!cmd->background || foregroundOnly) {
                waitpid(spawnPid, lastStatus, 0);

                // print the terminating signal
                if (WIFSIGNALED(*lastStatus)) {
                    printf("\nForeground PID is %d \nterminated by signal: %d\n", spawnPid, WTERMSIG(*lastStatus));
                    fflush(stdout);
                }

            } else {

                if (numBackProcess < MAX_BACK_PROCESS) {
                    backgroundPIDs[numBackProcess++] = spawnPid;
                }
                
                // print the background PID
                printf("Background PID is %d\n", spawnPid);
                fflush(stdout);
            }

            //break if background process
            break;
    }
}

//exit command
void exitCommand() {

    // kill all background processes
    for (int i = 0; i < numBackProcess; i++) {
        // send the SIGTERM signal
        kill(backgroundPIDs[i], SIGTERM);  
    }

    printf("\nExit program\n");
    fflush(stdout);
    // terminate the shell
    exit(0);  
}

// execute built-in command
int executeBuiltIn(struct command* cmd, int* lastStatus) {
    
    // check if the command is empty
    if (cmd->argv[0] == NULL || strcmp(cmd->argv[0], "") == 0) {
        // handle empty or invalid commands

        return 1;  
    }

    // check if the command is a built-in command
    int lastIndex = 0;

    while (cmd->argv[lastIndex] != NULL) lastIndex++;
    if (lastIndex > 0 && strcmp(cmd->argv[lastIndex - 1], "&") == 0) {
        // ignore the '&' 
        cmd->argv[lastIndex - 1] = NULL; 
    }

    //cd command
    if (strcmp(cmd->argv[0], "cd") == 0) {
        char* path;

        if (cmd->argv[1] != NULL) {
            path = cmd->argv[1];
            
        } else {
            path = getenv("HOME");
        }

        if (chdir(path) != 0) {
            perror("smallsh: cd");
            fflush(stdout);
        }

        return 1;

    // status command
    } else if (strcmp(cmd->argv[0], "status") == 0) {

        if (WIFEXITED(*lastStatus)) {
            printf("Exit status: %d\n", WEXITSTATUS(*lastStatus));
            fflush(stdout);

        } else {
            printf("Terminated by signal: %d\n", WTERMSIG(*lastStatus));
            fflush(stdout);
        }

        return 1;

    // exit command
    }else if (strcmp(cmd->argv[0], "exit") == 0) {

        exitCommand(); 

    // mkdir command
    }else if (strcmp(cmd->argv[0], "mkdir") == 0) {

        if (cmd->argv[1] == NULL) {
            fprintf(stderr, "smallsh: mkdir requires an argument\n");
            fflush(stdout);
            return 1;
        }
        if (mkdir(cmd->argv[1], 0777) != 0) {
            perror("smallsh: mkdir");
            fflush(stdout);
        }
        return 1;
    }
    
    return 0;
}

// free command
void freeCommand(struct command* cmd) {
    for (int i = 0; cmd->argv[i] != NULL; i++) {
        free(cmd->argv[i]);
    }
    free(cmd->inputFile);
    free(cmd->outputFile);
}

// replace dollar sign
void replaceDollar(char* token, char* pidStr, char* newToken) {
    int tokenIndex = 0;
    int newIndex = 0;

    // loop through the token
    while (token[tokenIndex] != '\0') {
        
        // check for "$$"
        if (token[tokenIndex] == '$' && token[tokenIndex + 1] == '$') {
            // copy PID
            strcpy(&newToken[newIndex], pidStr);
            // update indices
            newIndex += strlen(pidStr);
            // skip the "$$"
            tokenIndex += 2; 

        } else {

            // ccopy regular character
            newToken[newIndex++] = token[tokenIndex++];
        }
    }

    // Null-terminate new string
    newToken[newIndex] = '\0';  
}

// parse command
void parseCommand(char* inputLine, struct command* cmd) {
    char pidStr[20];
    sprintf(pidStr, "%d", getpid());

    char* savePtr;
    char* token = strtok_r(inputLine, " ", &savePtr);
    int argCount = 0;

    // Initialize the command struct
    cmd->background = 0;
    cmd->inputFile = NULL;
    cmd->outputFile = NULL;

    if (!token) {
        cmd->argv[0] = strdup("");  // Empty command
        return;
    }

    while (token != NULL) {

        char newToken[MAX_LENGTH];

        // replace "$$" with PID
        replaceDollar(token, pidStr, newToken);

        if (strcmp(newToken, "&") == 0 && argCount == 0) {
            // ignore '&' if it's the only character
            break;

        } else if ((strcmp(newToken, "<") == 0 || strcmp(newToken, ">") == 0) && argCount == 0) {
            // handle cases
            fprintf(stderr, "Error: Command Error '%s'\n", newToken);
            fflush(stdout);

            // set to empty to indicate error
            cmd->argv[0] = strdup("");  
            return;
        
        // & 
        } else if (strcmp(newToken, "&") == 0) {
            cmd->background = 1;

        // < 
        } else if (strcmp(newToken, "<") == 0) {
            token = strtok_r(NULL, " ", &savePtr);
            if (token == NULL) {
                fprintf(stderr, "Error: No input file specified\n");
                fflush(stdout);
                return;
            }
            cmd->inputFile = strdup(token);

        // > 
        } else if (strcmp(newToken, ">") == 0) {
            token = strtok_r(NULL, " ", &savePtr);
            if (token == NULL) {
                fprintf(stderr, "Error: No output file specified\n");
                fflush(stdout);
                return;
            }
            cmd->outputFile = strdup(token);

        // normal argument
        } else {
            cmd->argv[argCount++] = strdup(newToken);
        }

        token = strtok_r(NULL, " ", &savePtr);
    }

    cmd->argv[argCount] = NULL;
}

