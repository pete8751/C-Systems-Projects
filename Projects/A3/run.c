/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"

//TESTING CHATGPT
//int main() {
//    char input[] = "PATH=/local/bin:/usr/bin:/bin/";
//    fprintf(stdout, "debugging");
//
//    // Assuming 'parse_line' is the function to be tested
//    Variable *variable = NULL;
////    Variable *variable = malloc(sizeof(Variable));
////    variable->name = malloc(sizeof(char) * 20);
////    variable->value = malloc(sizeof(char) * 20);
////    strcpy(variable->name, "arg4");
////    strcpy(variable->value, "penelope");
////    variable->next = NULL;
//    Command *result = parse_line(input, &variable);
////    char *resec = resolve_executable("stty", variable);
//    char next_input[] = "ls 2 4";
//    result = parse_line(next_input, &variable);
//    int *exit_code = execute_line(result);
//    if (exit_code != (int *) -1){free(exit_code);}
//    int fire = 7;
//    return fire;
//}


// COMPLETE
int cd_cscshell(const char *target_dir){
    if (target_dir == NULL) {
        char user_buff[MAX_USER_BUF];
        if (getlogin_r(user_buff, MAX_USER_BUF) != 0) {
           perror("run_command");
           return -1;
        }
        struct passwd *pw_data = getpwnam((char *)user_buff);
        if (pw_data == NULL) {
           perror("run_command");
           return -1;
        }
        target_dir = pw_data->pw_dir;
    }

    if(chdir(target_dir) < 0){
        perror("cd_cscshell");
        return -1;
    }
    return 0;
}

int linked_list_len(Command *head)
{
    int i = 0;
    Command *curr = head;
    while(curr != NULL)
    {
        i++;
        curr = curr->next;
    }
    return i;
}

//void kill_processes(int *pid_list, int len)
//{
//    for (int i = 0; i < len; i++)
//    {
//        if (pid_list[i] == -1)
//        {
//            return;
//        }
//
//        if (kill(pid, SIGTERM) == 0)
//        {
//            return;
//        }
//
//        perror("kill process : %d failed", pid_list[i]);
//        system(EXIT_FAILURE);
//    }
//}
//
//int await_processes(int num_processes)
//{
//    int i = 0;
//    int exit_code;
//    while(i < num_processes)
//    {
//        int exit_val;
//        int pid = waitpid(-1, &exit_val, WNOHANG);
//        if (pid == -1)
//        {
//            perror("waitpid error");
//            exit(EXIT_FAILURE); //TODO: HOW TO HANDLE?
//        }
//
//        if (pid == 0) //process hasn't returned yet.
//        {
//            continue;
//        }
//
//        if (WIFEXITED(exit_val))
//        {
//            i++;
//            int exit_code = WEXITSTATUS(exit_val);
//        }
//        if (WIFSIGNALED(exit_val))
//        {
//            return -1; //TODO: HOW TO HANDLE?
//
//        }
//        //if code reaches here then abnormal termination (error).
//    }
//    return exit_code;
//}

int await_processes(int *pids, int num_processes)
{
    int exit_code;
    for (int i = 0; i < num_processes; i++)
    {
        int status;
        if (waitpid(pids[i], &status, 0) == -1)
        {
            perror("waitpid");
            exit(EXIT_FAILURE); // Or handle the error according to your needs
        }

        // Check if the child process terminated normally
        if (WIFEXITED(status))
        {
            // Retrieve the exit status of the child process
            exit_code = WEXITSTATUS(status);
        } else
        {
            printf("Process with PID %d terminated abnormally\n", pids[i]);
            ERR_PRINT("ABNORMAL TERMINATION OF COMMAND\n");
            exit(EXIT_FAILURE);
        }
    }

    return exit_code;
}


int *execute_line(Command *head){
    #ifdef DEBUG
    printf("\n***********************\n");
    printf("BEGIN: Executing line...\n");
    #endif

    int command_num = linked_list_len(head);
    if (command_num == 0)
    {
       return NULL;
    }

    int *pid_list = malloc(sizeof(int) * command_num);
    int *output = malloc(sizeof(int));

    //check for CD command (with my implementation it will always be at the head).
    if (strcmp(head->exec_path, CD) == 0)
    {
        free(pid_list);

        *output = cd_cscshell(head->args[1]);
        return output;
    }

    //execute commands if the first one wasn't CD.
    bool startup_error = false;
    int i = 0;
    Command *curr = head;
    while(curr != NULL)
    {
        if(!startup_error)
        {
            pid_list[i] = run_command(curr);
            if (pid_list[i] == -1) //error starting commands
            {
                startup_error = true;
                free(output);
                output = (int *) -1;
            }
        }
        //free the command
        Command *old = curr;
        curr = curr->next;
        free_command(old);

        i++;
    }

    if (startup_error)
    {
        free(pid_list);
        return output;
    }

    #ifdef DEBUG
    printf("All children created\n");
    #endif

    *output = await_processes(pid_list, command_num);
    free(pid_list);

    #ifdef DEBUG
    printf("All children finished\n");
    #endif

    #ifdef DEBUG
    printf("END: Executing line...\n");
    printf("***********************\n\n");
    #endif

    return output;
}

int get_fd_outputfiles(Command *command)
{
    int fd;
    if (command->redir_append)
    {
        fd = open(command->redir_out_path, O_WRONLY | O_CREAT | O_APPEND, 0600); //Write Only Append Mode (CHATGPT)
        if (fd == -1) {
            perror("open file in append mode");
            return -1;
        }
    } else
    {
        fd = open(command->redir_out_path, O_WRONLY | O_CREAT | O_TRUNC, 0600); // TODO: SHOULD I OVERWRITE FOR OUTUT??
//        fd = open(command->redir_out_path, O_WRONLY);
        if (fd == -1)
        {
            perror("writing out to file error");
            return -1;
        }
    }
    return fd;
}

//handles all output cases except piped output
int manage_cmd_output(Command *command)
{
    //system errors: return -2;

    if (command->redir_out_path != NULL)
    {
        return get_fd_outputfiles(command);
    }

    //At this point we know redir_out_path is null, and stdout is default, because we never change this attribute
    //Hence we use STDOUT as fd, this will be overwritten if this command pipes to another one.

    return STDOUT_FILENO;
}

int get_fd_inputfiles(Command *command)
{
    int fd;
    fd = open(command->redir_in_path, O_RDONLY); //overwrite or recreate (CHATGPT)
    if (fd == -1)
    {
        perror("reading from file error");
        return -1;
    }

    return fd;
}

//handles all input cases including piped input
int manage_cmd_input(Command *command)
{
    //if stdin_fd is not NULL, we know it was modified by previous cmd, and hence is output of prev pipe.
    if (command->stdin_fd != STDIN_FILENO)
    {
        return command->stdin_fd;
    }

    //Read from a file if redir_in_path is not NULL
    if (command->redir_in_path != NULL)
    {
        return get_fd_inputfiles(command);
    }

    //The only case left is if stdin_fd is default, and redir_in_path is NULL, hence we use STDIN.
    return STDIN_FILENO;
}

int execute_child(Command *command, int err_pipe[2], int stdin_fd, int stdout_fd)
{
    close(err_pipe[0]);
    //redirecting standard error to write end of error pipe.
//    if (dup2(err_pipe[1], STDERR_FILENO) == -1)
//    {
//        perror("dup2 stderror redirection error in child\n");
//        exit(EXIT_FAILURE);
//    }
//    close(err_pipe[1]);

    // Child process replace STDIN/STDOUT
    if (dup2(stdin_fd, STDIN_FILENO) == -1)
    {
        perror("dup2 error in child");
        exit(EXIT_FAILURE);
    }

    //close duplicate fd if it is not STDIN
    if (stdin_fd != STDIN_FILENO){close(stdin_fd);}

    if (dup2(stdout_fd, STDOUT_FILENO) == -1)
    {
        perror("dup2 error in child");
        exit(EXIT_FAILURE);
    }

    //close duplicate fd if it is not STDIN
    if (stdout_fd != STDOUT_FILENO){close(stdout_fd);}

    execv(command->exec_path, command->args); //TODO: FIRST ARG MAY NOT BE EXEC_PATH.

    if (dup2(err_pipe[1], STDERR_FILENO) == -1)
    {
        perror("dup2 stderror redirection error in child\n");
        exit(EXIT_FAILURE);
    }
    close(err_pipe[1]);

    //at this point we know there was some issue with execvp. change output to err_pipe write
    perror("execv error");
    exit(EXIT_FAILURE);
}

/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
int run_command(Command *command){

    //the following ensures the input descriptors of this command are set up properly.
    int stdin_fd = manage_cmd_input(command);
    if (stdin_fd == -1){return -1;} //TODO: SHOULD SYSTEM EXIT HERE?

    //the following ensures the output descriptors of this command are set up properly.
    int stdout_fd = manage_cmd_output(command);
    if (stdout_fd  == -1)
    {
        if (stdin_fd != STDIN_FILENO){close(stdin_fd);}
        return -1;
    }

    //Creates a pipe if command->next is not NULL
    bool piping_out = false;
    int pipefd[2];
    if (command->next != NULL)
    {
        piping_out = true;
        if (pipe(pipefd) == -1) {
            perror("pipe error when connecting commands, run_command");
            exit(EXIT_FAILURE);
        }

        //a little extra error checking
        if (stdout_fd != STDIN_FILENO)
        {
            ERR_PRINT("SIMULTANEOUS PIPE AND REDIRECTION\n");
        }

        //sets current command stdout to pipe write, and next command stdin to pipe read fd
        command->next->stdin_fd = pipefd[0];
        stdout_fd = pipefd[1];
    }

    #ifdef DEBUG
    printf("Running command: %s\n", command->exec_path);
    printf("Argvs: ");
    if (command->args == NULL){
        printf("NULL\n");
    }
    else if (command->args[0] == NULL){
        printf("Empty\n");
    }
    else {
        for (int i=0; command->args[i] != NULL; i++){
            printf("%d: [%s] ", i+1, command->args[i]);
        }
    }
    printf("\n");
    printf("Redir out: %s\n Redir in: %s\n",
           command->redir_out_path, command->redir_in_path);
    printf("Stdin fd: %d | Stdout fd: %d\n",
           stdin_fd, stdout_fd);
    #endif

    //now that all file descriptors have been found, we fork, and set input/output streams accordingly.

    //setup an error pipe:
    int err_pipe[2];
    if (pipe(err_pipe) == -1)
    {
        perror("err_pipe");
        exit(EXIT_FAILURE);
    }

    // Set close on exec, so that the error pipe closes in the child process when it execs.
    if (fcntl(err_pipe[1], F_SETFD, fcntl(err_pipe[1], F_GETFD) | O_CLOEXEC) == -1) {
        perror("fcntl");
        close(err_pipe[0]);
        close(err_pipe[1]);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork error run_command");
        exit(EXIT_FAILURE);
    } else if (pid == 0)
    {
        if(piping_out){close(pipefd[0]);} //close read end of pipe if process has one.
        execute_child(command, err_pipe, stdin_fd, stdout_fd); //child does not get past this point
    }

    //close fds:
    if (stdin_fd != STDIN_FILENO){close(stdin_fd);}
    if (stdout_fd != STDOUT_FILENO){close(stdout_fd);} //this is write end of pipe if there was one.

    // Parent process
    #ifdef DEBUG
    printf("Parent process created child PID [%d] for %s\n", pid, command->exec_path);
    #endif

    //read errors from child
    close(err_pipe[1]);
    bool error = false;
    char err_msg[MAX_SINGLE_LINE];
    int bytes_read;
    while ((bytes_read = read(err_pipe[0], err_msg, sizeof(err_msg))) > 0) {
        err_msg[bytes_read] = '\0';
        printf("%s", err_msg); // Print the line
        error = true;
    }
    close(err_pipe[0]);

    if (error){return -1;}
    return pid;
}


int run_script(char *file_path, Variable **root){

    // Open the script file
    FILE *script_file = fopen(file_path, "r");
    if (script_file == NULL) {
        perror("Error opening script file");
        return -1; //NO SYSTEN EXUIT???
    }

    char line[MAX_SINGLE_LINE]; // Buffer to store each line of the script

    // Read each line of the script file
    while (fgets(line, sizeof(line), script_file) != NULL) {
        // Execute each line of the script

        //the following replaces the new line character with the null terminator for each line.
        line[strlen(line) - 1] = '\0';
        Command *head = parse_line(line, root);
        if (head == (Command *) -1)
        {
            //error in parsing.
            ERR_PRINT(ERR_PARSING_LINE);
            return -1;
        }
        int *exit_code = execute_line(head);
        if (exit_code == NULL) {
            // If execute_line returns NULL, there are no commands to execute
            free(exit_code);
            continue;
        } else if (exit_code == (int *) -1) {
            // If execute_line returns -1, there was an error executing the line
            ERR_PRINT(ERR_EXECUTE_LINE);
            free(exit_code);
            fclose(script_file); // Close the script file
            return -1;
        }
        free(exit_code);
    }
    printf("\n");

    fclose(script_file); // Close the script file
    return 0; // All lines executed successfully
}

void free_command(Command *command){
    if(command->args != NULL){clean_args(command->args);}
    if(command->exec_path != NULL){free(command->exec_path);}
    if(command->redir_in_path != NULL){free(command->redir_in_path);}
    if(command->redir_out_path != NULL){free(command->redir_out_path);}
    free(command);
}
