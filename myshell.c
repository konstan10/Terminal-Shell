#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct Node{
    char* data;
    struct Node* next;
};
struct Node* newNode(char* cmd){
    struct Node *new = (struct Node*)malloc(sizeof(struct Node));
    new->data = strdup(cmd);
    new->next = NULL;
    return new;
};
struct Node* parseCommands(char* line){
    int sz = 0;
    char* token = strtok(line, "|");
    if(token == NULL){
        struct Node* cmds = newNode(token);  
        return cmds; 
    }
    else{
        struct Node* cmds = newNode(token);
        struct Node* temp = cmds;
        while(1){
            token = strtok(NULL, "|");
            if(token != NULL){
                temp->next = newNode(token);
                temp = temp->next;
            }
            else{
                break;
            } 
        }
        return cmds;
    }
}
void findInOut(char* line, int* ir, int* or){
    *ir = -1;
    *or = -1;
    char *in = strchr(line, '<');
    char *out = strchr(line, '>');
    if(in != NULL){
        *ir = 1;
    }
    if(out != NULL){
        *or = 1;
    }
   // printf("%d", *or);
}
void removeSpaces(struct Node* cmds){
    struct Node* ptr = cmds;
    while(ptr != NULL){
        char* start = ptr->data;
        char* end = ptr->data + strlen(ptr->data) - 1;
        while(isspace(*start)){
            start++;
        }
        while(isspace(*end)){
            end--;
        }
        *(end + 1) = '\0';
        if(start != ptr->data){
            memmove(ptr->data, start, end - start + 2);
        }
        ptr = ptr->next;
    }
}
void parseArgs(struct Node* cmds, char** args){
    int i = 0;
    char* tempstr = (char *)malloc(strlen(cmds->data) + 1);
    strcpy(tempstr, cmds->data);
    char* token = strtok(tempstr, " <>");
    if(token == NULL){
        *(args) = tempstr;
        *(args + 1) = NULL;
    }
    else{
        *(args) = token;
        i++;
        while(1){
            token = strtok(NULL, " <>");
            if(token != NULL){
                *(args+i) = token;
                i++;
            }
            else{
                *(args+i) = NULL;
                break;
            } 
        }
    }
}
void parseAllArgs(struct Node* cmds, char* allArgs[][16], int* szCmds){
    struct Node* cmdptr = cmds;
    int i = 0;
    while(cmdptr != NULL){
        parseArgs(cmdptr, allArgs[i]);
        cmdptr = cmdptr->next;
        i++;
    }
    allArgs[i][0] = NULL;
    *szCmds = i;
}
int lenCmd(char* allArgs[][16], int idx){
    int i = 0;
    while(allArgs[idx][i] != NULL){
        i++;
    }
    return i;
}
int hasAmpersand(char* allArgs[][16], int len){
    int i = 0;
    while(allArgs[len - 1][i] != NULL){
        if(strcmp(allArgs[len - 1][i], "&") == 0){
            return i;
        }
        i++;
    }
    return -1;
}
void handle_signal(int signo) {
    if (signo == SIGCHLD) {
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {
            
        }
    }
}
void execCmds(char* allArgs[][16], int* szCmds, int* ir, int* or){
    int i;
    int pipefd[2];
    int fdholder;
    int lenfirst;
    int lenlast;
    int flags1 = O_CREAT | O_RDONLY;
    int flags2 = O_CREAT | O_WRONLY;
    if(*ir == 1){
        lenfirst = lenCmd(allArgs, 0);
        //printf("%d\n", lenfirst);
    }
    if(*or == 1){
        lenlast = lenCmd(allArgs, *szCmds - 1);
        //printf("%d\n", lenlast);
    }
    fflush(stdout);

    for(i = 0; i < *szCmds; i++){
        pipe(pipefd);
        if(fork() == 0){
            if((i == 0) && (*ir >= 0)){                                      //if on first cmd and it is input redir
                if((i == *szCmds - 1) && (*or >= 0)){                            //if first cmd also has output redir
                    int outputFD = open(allArgs[i][lenlast-1], flags2, 0666);
                    dup2(outputFD, STDOUT_FILENO);
                    allArgs[i][lenlast-1] = NULL;
                    close(outputFD);
                    int inputFD = open(allArgs[i][lenlast-2], flags1, 0666);
                    dup2(inputFD, STDIN_FILENO);
                    close(inputFD);
                    allArgs[i][lenlast-2] = NULL;
                    execvp(allArgs[i][0],allArgs[i]);
                    perror("failed to execute");
                    exit(1); 
                }
                else{
                    int inputFD = open(allArgs[i][lenfirst-1], flags1, 0666);
                    dup2(inputFD, STDIN_FILENO);
                    close(inputFD);
                    allArgs[i][lenfirst-1] = NULL;
                    *ir = -1;
                }
            }
            if((i == *szCmds - 1) && (*or >= 0)){                            //if on last cmd and it is output redir
                int outputFD = open(allArgs[i][lenlast-1], flags2, 0666);
                dup2(outputFD, STDOUT_FILENO);
                allArgs[i][lenlast-1] = NULL;
                close(outputFD);
            }
            if(i > 0){                                                      //if not on first command
                dup2(fdholder, STDIN_FILENO);
                close(fdholder);
            }
            if((i < *szCmds - 1) || ((i == *szCmds - 1) && (*or == -1))){   //if not on last command or on last command and doesnt have output redir
                close(pipefd[0]);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            execvp(allArgs[i][0],allArgs[i]);
            perror("failed to execute");
            exit(1); 
        }
        else{
            int status;
            wait(&status);
            if(WIFEXITED(status)){
                close(pipefd[1]);
                if(*szCmds == (i + 1)){
                    char out[1024];
                    int sz = read(pipefd[0], out, sizeof(out));
                    printf("%.*s", sz, out);
                    close(pipefd[0]);
                }
                else{
                    fdholder = dup(pipefd[0]);
                }
                close(pipefd[0]); 
            }
        }
    }
    
}
void freePointers(struct Node* cmds, char* allArgs[][16], int szCmds){
    struct Node* temp = cmds;
    int i;
    while(cmds != NULL){
        temp = cmds->next;
        free(cmds->data);
        free(cmds);
        cmds = temp;
    }
    for(i = szCmds-1; i >= 0; i--){
        free(*(allArgs[i]));
    }
}

int main(int argc, char** argv){
    signal(SIGCHLD, handle_signal);
    while(1){
        char userinput[512];
        char* allArgs[256][16];
        int* ir = malloc(sizeof(int));
        int* or = malloc(sizeof(int));
        int* szCmds = malloc(sizeof(int));

        if(argv[1] == NULL){
            printf("my_shell$ ");
        }
        else{
            if(strcmp(argv[1], "-n") == 0){
                ;
            }
            else{
                printf("my_shell$ ");
            }
        }

        char* line = fgets(userinput, 512, stdin);

        if (line == NULL) {
            break;
        }
        findInOut(line, ir, or);
        struct Node* cmds = parseCommands(userinput);
        removeSpaces(cmds);
        parseAllArgs(cmds, allArgs, szCmds);
        //printf("%s, %s\n", allArgs[1][0], allArgs[1][1]);

        int ampersand = hasAmpersand(allArgs, *szCmds);
        int pid = fork();
        if(pid == 0){
            if(ampersand >= 0){
                allArgs[*szCmds - 1][ampersand] = NULL;
                setsid();
            }
            execCmds(allArgs, szCmds, ir, or);
            exit(0);
        }
        else{
            if(ampersand < 0){
                int status;
                wait(&status);
            }
        }
        
        free(ir);
        free(or);
        freePointers(cmds, allArgs, *szCmds);
        free(szCmds);
    }
    return 0;
}
