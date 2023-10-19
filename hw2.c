#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

const int MAXLINE = 80;
const int MAXARGS = 80;
const int MAXJOB = 5;

struct Shell{
    char* home;
    char* argument;
    char* argv[MAXARGS]; // probably throwaway
};

// main signal handling, set default to do nothing
void handle_sigint(int signum) {
    // printf("\nCtrl-C (SIGINT) is ignored. Press Enter to continue.\n");
}
void handle_sigtstp(int signum) {
    // printf("\nCtrl-Z (SIGTSTP) is ignored. Press Enter to continue.\n");
}
// built-in func handling also commands
void handle_pwd();
int handle_cd(struct Shell *s);
void handle_ls();
void handle_fork(char* args[MAXARGS]);
void handle_ps();
// misc
void set_home_dir(struct Shell *s);
void initialize_shell(struct Shell* s){
    set_home_dir(s);
    // printf("this is home: %s\n", s->home);
    s->argument = NULL;
}
void waiting4pid(pid_t processID);
void parseline(char* line, char* arr[]);
void my_loop(struct Shell* shell);

int main() {

    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    
    struct Shell shell;
    initialize_shell(&shell);
    my_loop(&shell);
    free(shell.home);
        
    return 0;
}

int handle_cd(struct Shell *s){
    if (strcmp(s->argument, "") == 0){
        printf("its empty.");
        s->argument = s->home;
    }
    if (chdir(s->argument) == 0) {
        // next line only for testing where its changed to
        // printf("Changed to directory: %s\n", s->argument);
        return 0; // Success
    } else {
        perror("chdir");
        return 1; // Error
    }
}

void handle_pwd(){
    char* current_directory = getcwd(NULL, 0);

    if (current_directory != NULL) {
        printf("%s\n", current_directory);
        free(current_directory); // Free the dynamically allocated memory
    } else {
        perror("getcwd");
        printf("Failed to get PWD\n");
    }
}

void handle_ls(){
    int exit_status = system("ls");

    if (exit_status == 0){
        printf("success.\n");
    } else {
        printf("failed\n");
    }
}

void handle_ps(){
    FILE *ps_output = popen("ps -o pid,tty,time,comm", "r");
    char text[256];
    while (fgets(text, 256, ps_output) != NULL){
        printf("%s", text);
    }
    pclose(ps_output);
}

void set_home_dir(struct Shell* s){
    char *home_directory = getenv("HOME");

    if (home_directory != NULL) {
        // printf("Home directory: %s\n", home_directory);
        s->home = (char *)malloc(strlen(home_directory) + 1);

        strcpy(s->home, home_directory);
    } else {
        perror("getenv");
        printf("Failed to get home directory\n");
    }
}

void handle_fork(char* argv[MAXARGS]){
    pid_t pid;
    pid = fork();
    if (pid == 0){
        if (execvp(argv[0], argv) < 0){

            if (execv(argv[0], argv) < 0){
                // command not found
                exit(0);
            }
        }
    } 
    else {
        //waiting4pid(pid); // foreground job
    }
}

void waiting4pid(pid_t processID){
    int waitCondition = WUNTRACED | WCONTINUED;
    int currentState;
    pid_t childpid;
    childpid = waitpid(processID, &currentState, waitCondition);
    if(WIFSIGNALED(currentState)){
        printf("\n currentState = child exited\n");
    }
    if (WIFSTOPPED(currentState)){
        printf("\n currentState = Child stopped.\n");
    }
    return;

}

void parseline(char* line, char* arr[]){
    char *token;
    size_t element_count = 0;

    token = strtok((char *)line, " ");

    while (token != NULL && element_count < MAXARGS) {
        arr[element_count] = token;
        token = strtok(NULL, " ");
        element_count++;
    }

    // Fill the rest of the array with NULL
    while (element_count < MAXARGS) {
        arr[element_count] = NULL;
        element_count++;
    }
}

void my_loop(struct Shell* shell){
    while(1){
    // collect path from user
        char path[MAXLINE];
        char *argv[MAXARGS];
        printf("prompt> ");
        fgets(path, MAXLINE, stdin);

    // remove newline characters from input, if not performed, then error
        size_t len = strlen(path);
        if (len > 0 && path[len - 1] == '\n') {
            path[len - 1] = '\0';
        }
        printf("this: %s\t %lu\n", path, strlen(path));

        char copy[MAXLINE];
        strcpy(copy, path);
        parseline(copy, argv);

        if (strlen(path) < 1) { continue;}
        else if (strcmp(argv[0], "pwd") == 0){
            // pwd dir
            handle_pwd();
        }
        else if (strcmp(argv[0], "ls") == 0){
            // ls dir
            handle_ls();
        }
        else if (strcmp(argv[0], "cd") == 0){
            // change dir
            // printf("here: %s\n", argv[1]);
            printf("herehere\n");
            if (strlen(path) <= 3){
                shell->argument = "";
            } else{ shell->argument = argv[1];}
            
            handle_cd(shell);
        }
        else if (strcmp(argv[0], "ps") == 0){
            // displays what is being processed. 
            handle_ps();
        }
        else if (strcmp(argv[0], "quit") == 0){
            // terminate all processes and close.
            printf("terminate everything\n");
            break;
        } 
        else {
            handle_fork(argv);
        }
    }
}
