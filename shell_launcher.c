#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


#define MAXLINE 80
#define MAXARGS 80
#define MAXJOB 5

struct Job {
    int job_id;
    int j_pid;
    char* status;  // STATUS CAN BE EITHER "RUNNING"/"Stopped"
    char* command;
    char* type; // this will be either background or foreground
};

struct Shell{
    char* home;
    char* user_str;
    size_t tok_len;
    // struct Job jobs[MAXJOB];
    // size_t num_jobs;
    char* argv[MAXARGS]; // probably throwaway
};


struct Shell shell;
struct Job jobs[MAXJOB];
size_t num_jobs;
char* c_path;


    
// signal handlers
void handle_sigint(int signum);
void handle_sigtstp(int signum);
void sigchld_handler(int signum);

// MOVING BETWEEN STATES FUNCTIONS
void move_to_foreground();
void move_to_background();
void kill_job(char* arr[]);
void add_job(pid_t child_pid, char * t);
int check_redir(char* argv[]);

// built-in func handling also commands
void handle_pwd();
int handle_cd();
void handle_ls();
void handle_fork(char* args[MAXARGS]);
void handle_ps();
void handle_jobs();
void handle_quit();
void handle_redirection();

// misc
void set_home_dir();
void waiting4pid(pid_t processID);
void parseline(char* line, char* arr[]);
void my_loop();
void rearrange_array(int del_pid);
void redirect_output_to_file(char* filename, void (*func_redir()));
// int check_redir(char* argv[]);




int main() {

    signal(SIGINT, handle_sigint);
    signal(SIGTSTP, handle_sigtstp);
    signal(SIGCHLD, sigchld_handler);
    
    // initialize_shell();
    set_home_dir();
    shell.tok_len = 0;
    // printf("this is home: %s\n", s->home);
    shell.user_str = NULL;
    num_jobs = 0;
    my_loop();
    free(shell.home);
        
    return 0;
}


// main signal handling, set default to do nothing
void handle_sigint(int signum) {
    // Terminate
    // handle sigint called when Ctrl+C is pressed

    // printf("Inside handle_sigint Ctrl+C\n");

    int found = 0;
    for (size_t i = 0; i< num_jobs; ++i){
        if (found == 1){
            // move everything up.
            jobs[i-1] = jobs[i];
        }
        if (strcmp(jobs[i].type, "FOREGROUND") == 0 &&
            strcmp(jobs[i].status, "Running") == 0) {
            found = 1;
            // when found we know the spot of the job that must be terminated
            // after finding this, we know that we can just move the elements
            // after can be shifted up by 1
        }
    }
    if (found == 1){
        num_jobs--;
    }
    
}


void handle_sigtstp(int signum) {
    // STOP/PAUSE
    // called when Ctrl+Z is pressed
    // stops only foreground process immediately
    
    // printf("Inside handle_sigtstp Ctrl+Z\n");

    // printf("\nCtrl-Z (SIGTSTP) is ignored. Press Enter to continue.\n");
    // printf("num jobs: %zu", num_jobs);
    for (size_t i = 0; i < num_jobs; ++i) {
        // printf("inside\n");
        // printf("this: %s\n", jobs[i].type);
        
        if (strcmp(jobs[i].type, "FOREGROUND") == 0) {
            jobs[i].status = "Stopped";
            //jobs[i].type = "FOREGROUND";
        } 
    }

    // printf("outside of forloop\n");
}


void sigchld_handler(int signum) {
    // sends a signal when a process in the background dies
    // have to rearrange the array
    int status;
    pid_t child_pid;
    
    while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            // printf("prompt > ");
            rearrange_array(child_pid);
        }
        
    }
}


// rearrange_array()
void rearrange_array(int del_pid) {
    int found = 0;
    for (size_t i = 0; i < num_jobs; ++i){
        // printf("i: %zu\n", i);
        if (found == 1){
            // move everything up.
            // printf("after: %zu\n", i);
            jobs[i-1] = jobs[i];
            jobs[i-1].job_id = jobs[i-1].job_id - 1;
        }
        if (jobs[i].j_pid == del_pid){
            // printf("found index: %zu\n", i);
            // printf("found pid: %d\n", jobs[i].j_pid);
            found = 1;
            // when found we know the spot of the job that must be terminated
            // after finding this, we know that we can just move the elements
            // after can be shifted up by 1
        }
    }
    if (found == 1){
        num_jobs--;
    }
    
}


int handle_cd(){
    if (strcmp(shell.user_str, "") == 0){
        // printf("its empty.");
        shell.user_str = shell.home;
    }
    if (chdir(shell.user_str) == 0) {
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
        // printf("Failed to get PWD\n");
    }
}


void handle_ls(){
    int exit_status = system("ls");

    if (exit_status == 0){
        // printf("success.\n");
    } else {
        // printf("failed\n");
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


void handle_fork(char* argv[MAXARGS]){
    if (num_jobs > 4) { return;}

    pid_t pid;
    pid = fork();

    if (pid == 0){
        if (execvp(argv[0], argv) < 0){
            if (execv(argv[0], argv) < 0){
                // command not found
                exit(1);
            }
        }
    } 
    else if (pid > 0) {
        if ((strcmp(argv[shell.tok_len - 1], "&") == 0)){
            // printf("background working here\n");
            if (setpgid(pid, pid) == -1) {}
            add_job(pid, "BACKGROUND");

        } else {
            add_job(pid, "FOREGROUND");
            // printf("before waiting\n");
            waiting4pid(pid);          // foreground job
            // printf("after waiting\n");
            // rearrange_array(pid);      // PUTTING THIS HERE SO FOREGROUND PROCESS GETS OUT OF JOBS
            // printf("after rearrange\n");
        }
    }
}


void handle_jobs() {
    // SEGFAULT CORE DUMPED
    for (size_t i = 0; i < num_jobs; ++i){
        if (jobs[i].type == "BACKGROUND") {
            printf("[%d] (%d) %s %s\n",jobs[i].job_id, 
                                        jobs[i].j_pid, 
                                        jobs[i].status, 
                                        jobs[i].command);
        }
        else {
        printf("[%d] (%d) %s %s\n",jobs[i].job_id, 
                                        jobs[i].j_pid, 
                                        jobs[i].status, 
                                        jobs[i].command);
        }
    }
}


void handle_redirection(char* argv[]) {
    int input_redirect = 0;
    int output_redirect = 0; // 1 for overwrite (">"), 2 for append (">>")
    char* input_file = NULL;
    char* output_file = NULL;

    // Detect and set redirection flags and filenames
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "<") == 0) {
            input_redirect = 1;
            input_file = argv[i + 1];
        } else if (strcmp(argv[i], ">") == 0) {
            output_redirect = 1;
            output_file = argv[i + 1];
        } else if (strcmp(argv[i], ">>") == 0) {
            output_redirect = 2;
            output_file = argv[i + 1];
        }
    }

    // Fork a child process
    pid_t child_pid = fork();
    if (child_pid == -1) {
        perror("Fork failed");
        exit(1);
    }

    if (child_pid == 0) {
        // This is the child process

        // Set up file descriptor variables
        int saved_stdin = 0;
        int saved_stdout = 0;

        if (input_redirect) {
            int input_fd = open(input_file, O_RDONLY);
            if (input_fd == -1) {
                perror("Input redirection failed");
                exit(1);
            }
            saved_stdin = dup(STDIN_FILENO); // Save current stdin
            if (dup2(input_fd, STDIN_FILENO) == -1) {
                perror("Input redirection failed");
                exit(1);
            }
            close(input_fd); // Close the file descriptor
        }

        if (output_redirect) {
            int output_fd;
            if (output_redirect == 1) {
                // Overwrite mode
                output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            } else {
                // Append mode
                output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
            }
            if (output_fd == -1) {
                perror("Output redirection failed");
                exit(1);
            }
            saved_stdout = dup(STDOUT_FILENO); // Save current stdout
            if (dup2(output_fd, STDOUT_FILENO) == -1) {
                perror("Output redirection failed");
                exit(1);
            }
            close(output_fd); // Close the file descriptor
        }

        // Execute the command
        if (execvp(argv[0], argv) < 0) {
            perror("Command execution error");
            exit(1);
        }

        // Restore original file descriptors after command execution
        if (input_redirect) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (output_redirect) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
    } else {
        // parent process

        int status;
        //waitpid(child_pid, &status, 0);
    }
}

void handle_redirection1(char* argv[]) {
    int input_redirect = 0;
    int output_redirect = 0; // 1 for overwrite (">"), 2 for append (">>")
    char *input_file = NULL;
    char *output_file = NULL;

    // Detect and set redirection flags and filenames
    for (int i = 0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "<") == 0) {
            input_redirect = 1;
            input_file = argv[i + 1];
        } else if (strcmp(argv[i], ">") == 0) {
            output_redirect = 1;
            output_file = argv[i + 1];
        } else if (strcmp(argv[i], ">>") == 0) {
            output_redirect = 2;
            output_file = argv[i + 1];
        }
    }

    // Set up file descriptor variables
    int saved_stdin = 0;
    int saved_stdout = 0;

    if (input_redirect) {
        int input_fd = open(input_file, O_RDONLY);
        if (input_fd == -1) {
            perror("Input redirection failed");
            exit(1);
        }
        saved_stdin = dup(STDIN_FILENO); // Save current stdin
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            perror("Input redirection failed");
            exit(1);
        }
        close(input_fd); // Close the file descriptor
    }

    if (output_redirect) {
        int output_fd;
        if (output_redirect == 1) {
            // Overwrite mode
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        } else {
            // Append mode
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
        }
        if (output_fd == -1) {
            perror("Output redirection failed");
            exit(1);
        }
        saved_stdout = dup(STDOUT_FILENO); // Save current stdout
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            perror("Output redirection failed");
            exit(1);
        }
        close(output_fd); // Close the file descriptor
    }

    // Execute the command
    if (execvp(argv[0], argv) < 0) {
        perror("Command execution error");
        exit(1);
    }
}


void move_to_foreground(char * argv[]) {
    // printf("here1\n");
    int fg_pid = 0;
    int i = 0;
     
    for (; i < num_jobs; i++) {
        char c = '%';
        char str[10];
        
        sprintf(str, "%c%d", c, i+1);

        // printf("%s", str);
        // if there's a percentage in front, check if it matches the job_id
        if ((argv[1])[0] == '%') {
            if (strcmp(str, argv[1]) == 0) {
                fg_pid = jobs[i].j_pid;
                break;
            }
        }
        // else see if it matches the j_pid
        else {
            if (jobs[i].j_pid == atoi(argv[1])) {
                fg_pid = jobs[i].j_pid;
                break;
            }
        }
    }
    // printf("here2, i = %d, PID = %d\n", i, fg_pid);

    if (fg_pid > 0) {
        // printf("here3");
        if (kill(fg_pid, SIGCONT) == 0) {
            // printf("here4");
            jobs[i].status = "Running"; // Update the status
            jobs[i].type = "FOREGROUND"; // Update the type
            // printf("Resumed job with PID %d\n", fg_pid);
        }
    }
}


void move_to_background() {

}


void kill_job(char* argv[]) {
    
    int kill_pid = 0;
     
    for (int i = 0; i < num_jobs; i++) {

        char c = '%';
        char str[10];
        sprintf(str, "%c%d", c, i+1);
        // printf("outside\n");
        // creating a string that starts %, just add i
        // printf("this: %s\n", argv[1]);
        if ((argv[1])[0] == '%') {
            // printf("here\n");
            if (strcmp(str, argv[1]) == 0) {
                // printf("here1\n");
                kill_pid = jobs[i].j_pid;
                break;
            }
        }
        else {
            // printf("inside non-percentage\n");
            if (jobs[i].j_pid == atoi(argv[1])) {
                // printf("inside atoi\n");
                kill_pid = jobs[i].j_pid;
                break;
            }
        }

    }

    if (kill_pid > 0){
        // printf("before kill function\n");
        kill(kill_pid, SIGKILL);
        // printf("after kill function\n");
        rearrange_array(kill_pid);
    }
}


void set_home_dir() {
    char *home_directory = getenv("HOME");

    if (home_directory != NULL) {
        // printf("Home directory: %s\n", home_directory);
        shell.home = (char *)malloc(strlen(home_directory) + 1);

        strcpy(shell.home, home_directory);
    } else {
        perror("getenv");
        // printf("Failed to get home directory\n");
    }
}


void add_job(pid_t child_pid, char * t) {
    if (num_jobs < MAXJOB) {
        char c[80];
        jobs[num_jobs].job_id = num_jobs + 1;  // updating job_id
        jobs[num_jobs].j_pid = child_pid;
        jobs[num_jobs].status = "Running";
        //jobs[num_jobs].command = (char *)malloc(strlen(shell.user_str) + 1);
        //jobs[num_jobs].command = c_path;
        
        jobs[num_jobs].command = (char *)malloc(strlen(shell.user_str) + 1);
        strcpy(jobs[num_jobs].command, shell.user_str);

        
        jobs[num_jobs].type = t;

        // (s->num_jobs)++;
        num_jobs = num_jobs + 1;
        // printf("num of jobs: %zu\n", num_jobs);
    }
    else{
        // printf("Cannot add more than 5 jobs");
    }
}


void waiting4pid(pid_t processID) {
    int waitCondition = WUNTRACED | WCONTINUED;
    int currentState;
    pid_t childpid;
    childpid = waitpid(processID, &currentState, waitCondition);
    // add_job(childpid, "FOREGROUND");
    if(WIFSIGNALED(currentState)){
        // printf("\n currentState = child exited\n");
        rearrange_array(childpid);
    }
    if (WIFSTOPPED(currentState)){
        // printf("\n currentState = Child stopped.\n");
    }

    return;
}


void parseline(char* line, char* arr[]) {
    char *token;
    size_t element_count = 0;

    token = strtok((char *)line, " \t");

    while (token != NULL && element_count < MAXARGS) {
        arr[element_count] = token;
        token = strtok(NULL, " ");
        element_count++;
    }
    shell.tok_len = element_count;

    // Fill the rest of the array with NULL
    while (element_count < MAXARGS) {
        arr[element_count] = NULL;
        element_count++;
    }
}


void handle_quit() { 
    for (int i = 0; i < num_jobs; ++i) {
        kill(jobs[i].j_pid, SIGKILL);
    }
}


int check_redir(char* argv[]) {
    // printf("Token length: %ld\n", shell.tok_len);
    for (size_t i = 0; i < shell.tok_len; ++i){
        if (strcmp(argv[i],"<") == 0 || strcmp(argv[i],">") == 0 || strcmp(argv[i],">>") == 0){
            return 1;
        }
    }
    return 0;
}

void redirect_output_to_file(char* filename, void (*func_redir())){
    FILE* original_stdout = stdout;

    FILE* file = fopen(filename, "w");
    if (file == NULL) {return;}

    // Redirect stdout to the file
    if (freopen(filename, "w", stdout) == NULL) {return;}

    // Call the function that you want to redirect
    func_redir();

    // Close the file and restore the original stdout
    fclose(stdout);
    stdout = original_stdout;
}


void my_loop() {
    while(1){
        // collect path from user
        char path[MAXLINE];
        char *arr[MAXARGS];
        printf("prompt > ");
        fgets(path, MAXLINE, stdin);

        // remove newline characters from input, if not performed, then error
        size_t len = strlen(path);
        if (len > 0 && path[len - 1] == '\n') {
            path[len - 1] = '\0';
        }
        
        // printf("this: %s\t %lu\n", path, strlen(path));
        shell.user_str = path;
        c_path = path;
        char copy[MAXLINE];
        strcpy(copy, path);
        parseline(copy, arr);

        if (check_redir(arr) == 1){
            // make a redirection handler here
            handle_redirection(arr);
            continue;
        }


        if (strlen(path) < 1) { continue;}
        else if (strcmp(arr[0], "pwd") == 0){
            // pwd dir
            handle_pwd();
        }
        else if (strcmp(arr[0], "ls") == 0){
            // ls dir
            handle_ls();
        }
        else if (strcmp(arr[0], "cd") == 0){
            // change dir
            // printf("here: %s\n", argv[1]);
            if (strlen(path) <= 3){
                shell.user_str = "";
            } else{ shell.user_str = arr[1];}
            
            handle_cd();
        }
        else if (strcmp(arr[0], "ps") == 0){
            // displays what is being processed. 
            handle_ps();
        }
        else if (strcmp(arr[0], "jobs") == 0){
            // displays what is being processed. 
            handle_jobs();
        }
        else if (strcmp(arr[0], "fg") == 0) {
            // MOVE THE JOB TO FOREGROUND
            move_to_foreground(arr);
        }
        else if (strcmp(arr[0], "bg") == 0) {
            // MOVE THE JOB TO BACKGROUND
            move_to_background(arr);
        }
        else if (strcmp(arr[0], "kill") == 0) {
            kill_job(arr);
        }
        else if (strcmp(arr[0], "quit") == 0){
            // terminate all processes and close.
            // dont forget to kill all processes before quiting
            handle_quit();
            break;
        }
        // else if (check_redirection()) {
        //     printf("Yes\n");
        // }
        else {
            c_path = shell.user_str;
            //printf("path: %s\n", shell.user_str);
            handle_fork(arr);
        }
    }
}