#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

void close_all_pipes(int** pfd, int pipes);
void write_history(FILE* hist, char* copy_history);
void inthandler(int signo);
void sighandler(int signo);

int x = -1;

/* A process is a single process.  */
typedef struct process
{
  char argv[30];                /* for exec */
  pid_t pid;                  /* process ID */
  char completed;             /* true if process has completed */
  char stopped;               /* true if process has stopped */
  int status;                 /* reported status value */
} process;

int pid;
char command[40];
process* pids[20];

int main(){

    pid_t p1;
    char input[125], *token, *arg[20], i;
    char cwd[200], *ptr, *dir;
    int success, in;
    char change_prompt[200], *prompt_name;
    int prompt_flag = 0, executed_flag = 1, status, ip_len = 0, redirect_flag = 0;
    char path[300] = "/usr/bin/:/usr/sbin/:/usr/lib/", copy[300], copy2[300], copy_history[125], buffer[125];
    char file_path[200], *filename, *temp, **pip;
    int pipes, **pfd, h;
    FILE* hist;
    
    char* status_name[2] = {"Stopped", "Running"};

    hist = fopen("bash_history", "w+");
    signal(SIGINT, inthandler);
    signal(SIGTSTP, sighandler);
    //signal(SIGTTOU, SIG_IGN);
    
    while(1){
        
        /* Check if the stdin was changed and revert back settigns using the
         * duplicate pointer created */
        if(redirect_flag == 1){
            dup2(in, 0);
            redirect_flag = 0;
        }

        /* Check if the stdout was changed and revert back settigns using the
         * duplicate pointer created */
        if(redirect_flag == 2){
            dup2(in, 1);
            redirect_flag = 0;
        }

        i = 0;
        executed_flag = 0;
        success = -1;
        pipes = 0;

        /* Check if the user wants a different prompt to be displayed*/
        if(!prompt_flag){
            getcwd(cwd, sizeof(cwd));
            printf("user@prompt:");
            printf("%s> ", cwd);
        }
        else{
            /* Display the other prompt if the user changed the prompt */
            printf("%s>", change_prompt);
        }

        ip_len = scanf("%[^\n]%*c", input);
        
        /* Check if the user wants to exit using CTRL-D */
        if(feof(stdin)){
            printf("\n");
            return 0;
        }

        /* Check if the user simply hit enter */
        if(ip_len < 1){
            while(getchar() != '\n')
                ;
            continue;
        }
        
        strcpy(copy_history, input); // Create a ccopy of the original command to write to history
        
        /* Handle history command*/
        if(!strcmp(input, "history")){
            
            fseek(hist, 0, SEEK_SET);
            while(fscanf(hist, "%[^\n]%*c", buffer) != EOF){
                printf("%s\n", buffer);
            }
            fseek(hist, 0, SEEK_END);
            fprintf(hist, "history\n");
            continue;
        }

        /* Start the process in the background using bg */
        if(!strcmp(input, "bg")){
            kill(pids[x]->pid, SIGCONT); // Send SIGCONT to resume process
            pids[x]->status = 1;
            continue;
        }

        /* Implementation of fg */
        if(!strcmp(input, "fg")){
            tcsetpgrp(stdin, pids[x]->pid); //Use tcsetpgrp to allow the process group access to the terminal
            kill(pids[x]->pid, SIGCONT); // Set SIGCONT to resume process 
            waitpid(pids[x]->pid, &status, WUNTRACED); // Wait until process changes state
            pids[x]->status = 1;
            continue;
        }

        if(!strcmp(input, "jobs")){
                for(int k = 0; k <= x; k++){
                    printf("[%d]+\t%s\t\t\t\t%s\n", k+1, status_name[pids[k]->status], pids[k]->argv);
                }
            continue;
        }
        /* Check if the user wnats to change the prompt */
        token = strtok(input, "=");
        
        /* Just a check to ensure only '=' wasn't pressed */
        if(!token)
            continue;
        
        if(!strcmp(token, "PS1")){
            prompt_name = strtok(NULL, "=");
            /* Check if no prompt name was entered */
            if(!prompt_name){
                printf("Please enter the prompt name you wish to set\n");
                continue;
            }
            /* if \w$ is entered revert back to the original prompt */
            if(!strcmp(prompt_name, "\\w$")){
                prompt_flag = 0;
                continue;
            }
            else{
                /* Display the new prompt name */
                strcpy(change_prompt, prompt_name );
                prompt_flag = 1;
            }
            write_history(hist, copy_history);
            continue;
        }
       
        /* Check if the user wishes to add to path */
        if(!strcmp(token, "PATH")){
            strcat(path, ":");
            token = strtok(NULL, "=");
            if(!token){
                printf("Please enter a valid path\n");
                continue;
            }
            strcat(path, token);
            strcat(path, "/");
            write_history(hist, copy_history);
            continue;
        }

        /* Check for input redirection */
        token = strtok(input, "<");
        if(token){
            
            strcpy(copy, input);
            if(filename = strtok(NULL, "<")){
                /* Redirect the output if filename is given else display on
                 * screen */

                /* Create a duplicate pointer to the standard keyboard input so
                 * that we revive the original settings */
                in = dup(0);
                close(0);
                /* Check for whitespaces */
                if(*filename == ' '){
                    temp = filename + 1;
                    success = open(temp, O_RDONLY);
                }
                else
                    success = open(filename, O_RDONLY);
                
                /* Check if open was a success */
                if(success == -1){
                    redirect_flag = 0;
                    continue;
                }
                else{
                    write_history(hist, copy_history);
                }
                redirect_flag = 1;
            }
        }
        else{
            continue;
        }

        /* Check for output redirection similar to that of input redirection */
        token = strtok(input, ">");
        if(token){
            strcpy(copy, input);
            filename = strtok(NULL, ">");
            if(filename){
                in = dup(1);
                close(1);
                temp = filename;
                if(*temp == ' '){
                    temp = filename + 1;
                    success = open(temp, O_RDWR | O_CREAT);
                }
                else{
                    success = open(filename, O_RDWR | O_CREAT);
                }

                if(success == -1){
                    redirect_flag = 0;
                    continue;
                }
                else
                    write_history(hist, copy_history); // Write to history if command successful
                
                redirect_flag = 2;
            }
        }
        else{
            continue;
        }
    
        /* Handling pipes */
        strcpy(copy, input);
        token = strtok(copy, "|");
        while(token = strtok(NULL, "|")){
            pipes += 1;
        }

        if(pipes){
            pfd = (int**)malloc(sizeof(int*)*pipes);
            pip = (char**)malloc(sizeof(char*)*(pipes+2));

            for(i = 0; i < pipes; i++){
                pfd[i] = (int*)malloc(sizeof(int)*2);
                pipe(pfd[i]);
            }

            pip[0] = strtok(input, "|");
            i = 0;

            while(pip[i]){
                pip[++i] = strtok(NULL, "|");
            }

            h = 0;
            while(pip[h]){
                i = 0;
                arg[0] = strtok(pip[h], " ");
                while(arg[i]){
                    arg[++i] = strtok(NULL, " ");
                }
                if(h == 0){
                    p1 = fork();
                    if(p1 == 0){
                        close(1);
                        dup(pfd[h][1]);
                        close_all_pipes(pfd, pipes);
                        execvp(arg[0], arg);
                    }
                    else{
                        write_history(hist, copy_history);
                    }
                }
                else if(h == pipes){
                    p1 = fork();
                    if(p1 == 0){
                        close(0);
                        dup(pfd[h-1][0]);
                        close_all_pipes(pfd, pipes);
                        execvp(arg[0], arg);
                    }
                    else{
                        close_all_pipes(pfd, pipes);
                        for(int k = 0; k <= pipes; k++)
                            wait(0);
                    }
                }
                else{
                    p1 = fork();
                    if(p1 == 0){
                        close(0);
                        dup(pfd[h-1][0]);
                        close(1);
                        dup(pfd[h][1]);
                        close_all_pipes(pfd, pipes);
                        execvp(arg[0], arg);
                    }
                }
                h++;
            }
            free(pfd);
            free(pip);
            continue;
        }

        /* Handle the case where the string gets trimmed due to the strtok
         * function and use the copy string instead for the original values */
        if(redirect_flag != 0){
            arg[0] = strtok(copy, " ");
        }
        else{
            /* Separate the command to be executed from the input string */
            arg[0] = strtok(input, " ");
        }
        strcpy(command, arg[0]);

        /* Exit the shell when the user wishes to exit */
        if(!strcmp(input, "exit"))
            return 0;

        /* 20 be the maximum arguments entered */
        while(arg[i]){
            if(i == 20){
                printf("Too many arguments\n");
                break;
            }
            arg[++i] = strtok(NULL, " ");
        }
        /* If the number of arguments exceed 20 continue to get another valid
         * command */
        if(i == 20)
            continue;

        /* Check if user wishes to change the directory */
        if(!strcmp(arg[0], "cd")){
            chdir(arg[1]);
            continue;
        }
        /* Check if the user wishes to execute a file located in the current directory */
        arg[0] = command;
        p1 = fork();

        if(p1 == 0){
            success = execv(arg[0], arg);
            exit(1);
        }
        else{
            pid = p1;
            wait(&success);
            if(success == 0){
                write_history(hist, copy_history);
                continue;
            }
        }

        /* Create a copy so that the strtok function does not alter the original
         * string */
        strcpy(copy2, path);
        token = strtok(copy2, ":");

        /* Iterate while the command is not found in the paths specified by the
         * user */
        while(token){
            strcpy(file_path, token);
            strcat(file_path, command);
            arg[0] = file_path;
            p1 = fork();

            if(p1 == 0){
                success = execv(arg[0], arg);
                exit(1); // To notify that the command has not yet been executed
            }
            else{
                pid = p1;
                waitpid(pid, &status, WUNTRACED);
                /* If the program returns a 0 status means that the program was
                 * executed */
                if(status == 0){
                    write_history(hist, copy_history);
                    executed_flag = 1;
                    break;
                }
            }
            token = strtok(NULL, ":");
        }
        if(executed_flag == 0){
            printf("command not found \n");
        }
    }

    return 0;

}

void close_all_pipes(int **pfd, int pipes){
    for(int i = 0; i < pipes; i++){
        close(pfd[i][0]);
        close(pfd[i][1]);
    }
    return;
}

void write_history(FILE* hist, char* copy_history){
    fprintf(hist, "%s\n", copy_history);
    return;
}

void inthandler(int signo){
    kill(pid, SIGTERM);
    return;
}

void sighandler(int signo){
    
    process* new;
    
    new = malloc(sizeof(process));
    kill(pid, SIGSTOP);
    strcpy(new->argv, command);
    new->pid = pid;
    new->stopped = 1;
    new->completed = 0;
    new->status = 0;
    pids[++x] = new;
    return;
}
