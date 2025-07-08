#include "cscshell.h"

#define CONTINUE_SEARCH NULL


char *resolve_executable(const char *command_name, Variable *path){

    if (command_name == NULL || path == NULL){
        return NULL;
    }

    if (strcmp(command_name, CD) == 0){
        return strdup(CD);
    }

    if (strcmp(path->name, PATH_VAR_NAME) != 0){
        ERR_PRINT(ERR_NOT_PATH);
        return NULL;
    }

    char *exec_path = NULL;

    if (strchr(command_name, '/')){
        exec_path = strdup(command_name);
        if (exec_path == NULL){
            perror("resolve_executable");
            return NULL;
        }
        return exec_path;
    }

    // we create a duplicate so that we can mess it up with strtok
    char *path_to_toke = strdup(path->value);
    if (path_to_toke == NULL){
        perror("resolve_executable");
        return NULL;
    }
    char *current_path = strtok(path_to_toke, ":");

    do {
        DIR *dir = opendir(current_path);
        if (dir == NULL){
            ERR_PRINT(ERR_BAD_PATH, current_path);
            closedir(dir);
            continue;
        }

        struct dirent *possible_file;

        while (exec_path == NULL) {
            // rare case where we should do this -- see: man readdir
            errno = 0;
            possible_file = readdir(dir);
            if (possible_file == NULL) {
                if (errno > 0){
                    perror("resolve_executable");
                    closedir(dir);
                    goto res_ex_cleanup;
                }
                // end of files, break
                break;
            }

            if (strcmp(possible_file->d_name, command_name) == 0){
                // +1 null term, +1 possible missing '/'
                size_t buflen = strlen(current_path) +
                    strlen(command_name) + 1 + 1;
                exec_path = (char *) malloc(buflen);
                // also sets remaining buf to 0
                strncpy(exec_path, current_path, buflen);
                if (current_path[strlen(current_path)-1] != '/'){
                    strncat(exec_path, "/", 2);
                }
                strncat(exec_path, command_name, strlen(command_name)+1);
            }
        }
        closedir(dir);

        // if this isn't null, stop checking paths
        if (possible_file) break;

    } while ((current_path = strtok(CONTINUE_SEARCH, ":")));

res_ex_cleanup:
    free(path_to_toke);
    return exec_path;
}

//Finds index of next non whitespace character in a char array.
int next_non_whitespace(const char *line, int curr_index)
{
    int i = 0;
    while (isspace(line[curr_index + i])){i++;}
    return curr_index + i;
}

//Checks if character marks end of line
bool end_of_line(char c)
{
    return (c == '\0' || c == '#');
}

//Finds index of next redirect operator (or end of line)
int next_redirect(char *line, int curr_index)
{
    int i = 0;
    while (!end_of_line(line[curr_index + i]))
    {
        if (line[curr_index + i] == '>' || line[curr_index + i] == '<')
        {
            break;
        }
        i++;
    }
    return curr_index + i;
}

//Returns pointer to a copy of a substring, when given start and end points of the substring.
char *substring_extract(const char *line, int start_index, int end_index)
{
    //start index is exact index of first char, end index is exact index of last character of substring.
    int substring_length = (end_index - start_index) + 1;
    char *substring_copy = malloc((substring_length + 1) * sizeof(char));
    if (substring_copy == NULL)
    {
        perror("Memory allocation failure substring_extract");
        return NULL;
    }

    //Copy substring into malloced string
    strncpy(substring_copy, line + start_index, substring_length); //TODO: CAN THIS FAIL?
    substring_copy[substring_length] = '\0'; // Add null terminator to terminate the string

    //This should return \0 null terminator if end_index = start_index

    return substring_copy;
}

//Check if input is text followed by = (without spaces): Variable Assignment
//This function will not cause system errors.
int is_var_assignment(char *line, int curr_index)
{
    //input is always index to first non-empty character in the line
    //function returns index of equal sign if variable assignment is found, -1 otherwise, -2 if error
    int i = curr_index;
    bool equals_found = false;
    bool acceptable_chars = true;
    while (!end_of_line(line[i]) && !isspace(line[i]))
    {
        //breaks at equal equal sign, '\0' or '#', returns early if any characters are not alphabetical or underscores.
        if (line[i] ==  '=')
        {
            equals_found = true;
            break;
        }

        if (!isalpha(line[i]) && !(line[i] == '_'))
        {
            acceptable_chars = false;
        }

        i++;
    }

    //Returns if no equals were found in while loop (we got to end of aspect)
    if (!equals_found){return -1;}

    //After this point, we assume that the input is a variable assignment

    //equals sign was first character in the aspect
    if(i == curr_index)
    {
        ERR_PRINT(ERR_VAR_START);
        return -2;
    }

    //Checks if variable name is alphabetical or underscore:
    if(!acceptable_chars)
    {
        ERR_PRINT(ERR_VAR_NAME, line);
        return -2;
    }

    //Following checks if the value of the variable assignment are all ascii. if not returns early.
    int value_index = i + 1;
    while(!end_of_line(line[value_index]))
    {
        if (!isascii(line[value_index]))
        {
            ERR_PRINT("VARIABLE VALUES MUST BE ASCII\n")
            return -2;
        }
        value_index++;
    }

    //If all checks are passed, and input is variable assignment then we return index of "=" in the variable assignment.
    return i;
}

//should *line be const?
int handle_var_assignment(char *line, int start, int equals_ind, Variable **variables)
{
    //Extract copy of variable name
    char *var_name = substring_extract(line, start, equals_ind - 1);

    //Find index of last character (not '\0' or '#' in line:
    int end = equals_ind;
    while(!end_of_line(line[end + 1])){end++;}
    char *var_value = substring_extract(line, equals_ind + 1, end);

    Variable *curr = *variables;
    Variable *last = curr;

    //traversing the linked list until we find a variable with the same name, or get to the end:
    while(curr != NULL)
    {
        if (strcmp(curr->name, var_name) == 0)
        {
            //set the variable name pointer to the pointer to the new value
            curr->value = var_value;
            return 1;
        }

        //Save the last node:
        if (curr->next == NULL)
        {
            last = curr;
        }

        curr = curr->next;
    }

    //initialize a new Variable object, and make the last variable in the linked list point to it
    Variable *new_var = malloc(sizeof(Variable));
    if (new_var == NULL) {
        free(var_name);
        free(var_value);
        perror("malloc failure handle_var_assignment");
        exit(EXIT_FAILURE);
    }

    new_var->name = var_name;
    new_var->value = var_value;
    new_var->next = NULL;

    if(last == NULL) //this means we are inserting the first variable.
    {
        *variables = new_var;
    } else
    {
        last->next = new_var;
    }

    return 1;
}

//CHATGPT
int count_separated_aspects(char *line)
{
    //handle case where line starts at non white space.
    int count = 0;

    // Tokenize the line based on spaces
    char *token = strtok(line, " ");

    // Loop through each token and count them
    while (token != NULL) {
        count++;
        token = strtok(NULL, " ");
    }

    return count;
}

void clean_args(char **args)
{
    int i = 0;
    while(args[i] != NULL)
    {
        free(args[i]);
        i++;
    }
    free(args);
    return;
}

void clean_command(Command *command)
{
    if (command == NULL) {
        return;  // Base case: Stop recursion if command is NULL
    }

    if (command->next != NULL)
    {
        clean_command(command->next);
    }

    if(command->args != NULL){clean_args(command->args);}
    if(command->exec_path != NULL){free(command->exec_path);}
    if(command->redir_in_path != NULL){free(command->redir_in_path);}
    if(command->redir_out_path != NULL){free(command->redir_out_path);}
    command->args = NULL;
    command->exec_path = NULL;
    command->redir_in_path = NULL;
    command->redir_out_path = NULL;
    free(command);
}

Command *command_initialize(char* command, int end_of_cmd)
{
    //Count arguments:
    char *command_copy = substring_extract(command, 0, end_of_cmd);
    if (command_copy == NULL){return (Command *) -1;}

    int num_args = count_separated_aspects(command_copy);
    free(command_copy);

    //Initialize Command:
    Command *new_command = malloc(sizeof(Command));
    if (new_command == NULL) {
        perror("malloc failure command_initialize new_command");
        return (Command *) -1;
    }

    //initialize and fill argument array TODO: LEADING WHITESPACE ISSUE FOR FIRST ARGUMENT
    char **args = malloc((num_args + 1) * sizeof(char *));
    if (args == NULL) {
        free(new_command);
        perror("malloc failure command_initialize args");
        return (Command *) -1;
    }
    //I do this so we easily know the last arg.
    args[num_args] = NULL;

    int index = 0;
    char *token = strtok(command, " ");
    while (token != NULL) {
        args[index] = strdup(token);
        if (args[index] == NULL) {
            clean_args(args);
            perror("Memory allocation failed new_command tokenization");
            return (Command *) -1;
        }
        index++;
        token = strtok(NULL, " ");
    }

    new_command->args = args;
    new_command->exec_path = NULL;
    new_command->next = NULL;
    new_command->redir_in_path = NULL;
    new_command->redir_out_path = NULL;
    new_command->redir_append = false;
    new_command->stdin_fd = STDIN_FILENO;
    new_command->stdout_fd = STDOUT_FILENO;

    return new_command;
}

int handle_input_output(Command *command, char *command_str, int start, int end, int k)
{
    //move curr_index to first nonwhitespace character in the file name copy.
    char *subcopy = substring_extract(command_str, start, end);
    int curr_index = start + next_non_whitespace(subcopy, 0);

    //count tokens (space separated) TODO: CAN FILES BE SEPARATED BY NONSPACE WHITESPACES? CAN THE FOLLOWING HAVE TOO FEW TOKENS?
    int tokens = count_separated_aspects(subcopy);
    if (tokens != 1)
    {
        ERR_PRINT("TOO MANY OR TOO FEW ARGUMENTS PROVIDED AS FILE NAME\n")
        return -1;
    }
    free(subcopy);

    subcopy = substring_extract(command_str, curr_index, end);
    char *token = strtok(subcopy, " ");
    char *file_name = strdup(token);
    free(subcopy);

    if(k == 0)
    {
        if(command->redir_in_path != NULL)
        {
            ERR_PRINT("INCORRECT COMBINATION OF INPUT OUTPUT REDIRECTION OPERATORS (<, >, >>)\n")
            return -1;
        }
        command->redir_in_path = file_name;
    }

    if(k > 0)
    {
        if(command->redir_out_path != NULL)
        {
            ERR_PRINT("INCORRECT COMBINATION OF INPUT OUTPUT REDIRECTION OPERATORS (<, >, >>)\n")
            return -1;
        }
        command->redir_out_path = file_name;
    }

    if(k == 2)
    {
        command->redir_append = true;
    }

    return 1;
}

//recursive function that resolves input/output for each command
int input_output_resolve(Command *command, char *cmd_line, int start)
{
    //input should be Command, char array, along with index of operator in the char array
    int curr_index = start;
    int next;
    int k;

    if (cmd_line[curr_index] == '<')
    {
        curr_index = curr_index + 1;
        k = 0;
    }

    if (cmd_line[curr_index] == '>' && cmd_line[curr_index + 1] == '>')
    {
        curr_index = curr_index + 2;
        k = 2;
    }
    else if (cmd_line[curr_index] == '>')
    {
        curr_index = curr_index + 1;
        k = 1;
    }

    next = next_redirect(cmd_line, curr_index);
    if (next == (curr_index + 1))
    {
        ERR_PRINT("INVALID COMBINATION OF REDIRECTION OPERATORS (<>, <<, ><)\n");
        return -1;
    }

    //If index of next redirection is end of line, then we have handle last input/output, and we return
    if(end_of_line(cmd_line[next]))
    {
        if (handle_input_output(command, cmd_line, curr_index, next, k) == -1)
        {
            return -1;
        }
        return 1;
    }

    //Case where only whitespace or null terminator follows operator is handled in helper:
    if (handle_input_output(command, cmd_line, curr_index, next - 1, k) == -1)
    {
        return -1;
    }

    //Otherwise, we handle the next input/output redirection (pass in same arguments, but index of next redirection).
    return input_output_resolve(command, cmd_line, next);
}

Command *command_resolve(char *command)
{
    bool redirect = false;
    //After this loop, end is index of end of line, or redirection operator.
    int end = next_redirect(command, 0);

    if (!end_of_line(command[end]))
    {
        redirect = true;
    }

    if (end == 0)
    {
        ERR_PRINT("COMMAND IS A REDIRECTION OPERATOR\n");
        return NULL;
    }

    //initialize Command with arguments
    char *command_only = substring_extract(command, 0, end - 1);
    if (command_only == NULL){return (Command *) -1;}
    Command *new_command = command_initialize(command_only, end - 1);
    free(command_only);
    if(new_command == (Command *) -1){return (Command *) -1;}

    //Resolve input/output redirections.
    if (redirect)
    {
        char *command_copy = strdup(command);
        int return_val = input_output_resolve(new_command, command_copy, end);
        free(command_copy);

        if (return_val != 1)
        {
            clean_command(new_command);
            return NULL;
        }
    }

    return new_command;
}

//Recursive function that splits the line at pipes, and resolves each section, before returning a command pointer
Command *command_parse(char *line, int start)
{
    bool pipe = false;
    int end = start;
    while(!end_of_line(line[end]))
    {
        if (line[end] == '|')
        {
            pipe = true;
            break;
        }
        end++; //this should finish at the index of the first end of line character, or the index at the pipe
    }
    //TODO: FIRST COMMAND PIPE, AND BACK TO BACK PIPE CASES?
    //command resolving and error handling
    char *command_copy = substring_extract(line, start, end - 1);
    if (command_copy == NULL){return (Command *) -1;}
    Command *latest_command = command_resolve(command_copy); //returns pointer to command.
    if(latest_command == (Command *) -1){return (Command *) -1;}

    free(command_copy);

    if (latest_command == NULL)
    {
        return NULL;
    }

    if (!pipe)
    {
        return latest_command;
    }

    //if pipe is true, this means that a pipe was detected.
    //Now we recurse through the command list, ensuring all output related attributes are correct.
    Command *next_cmd = command_parse(line, end + 1);

    //Error handling
    if(next_cmd == NULL || next_cmd == (Command *) -1)
    {
        free(latest_command->args);
        free(latest_command);
        return next_cmd;
    }

    latest_command->next = next_cmd;
    return latest_command;
}

int resolve_exec_paths(Command *command, Variable *path)
{
    //This sets the correct exec_path for each path in the linkedlist, and ensures the inputs are calibrated correctly.
    Command *head = command;
    Command *curr = head;
    while(curr != NULL)
    {
        //OUTPUT IS NOT EQUAL CURR NEXT NAME AND IT IS NOT NULL, CAUSE ERROR, ELSE SET IT TO CURR NEXT NAME
        char *curr_exec_path = resolve_executable(curr->args[0], path);
        if (curr_exec_path == (char *) -1 || curr_exec_path == NULL)
        {
            ERR_PRINT(ERR_NO_EXECU, curr->args[0]);
            clean_command(head);
            return -1;
        }

        curr->exec_path = curr_exec_path;

        //if CD is the head, immediately return the command with just the CD command struct.
        if (strcmp(curr_exec_path, CD) == 0)
        {
            if (curr == head)
            {
                clean_command(head->next);
                head->next = NULL;
                return 1;
            }
            ERR_PRINT("ATTEMPTED TO REDIRECT INTO CD\n")
            return -1;
        }
        curr = curr->next;
    }

    return 1;
}

int input_backward_pass(Command *command)
{
    Command *head = command;
    Command *curr = head;
    while(curr->next != NULL)
    {
        //OUTPUT IS NOT EQUAL CURR NEXT NAME AND IT IS NOT NULL, CAUSE ERROR, ELSE SET IT TO CURR NEXT NAME
        if (curr->redir_out_path != NULL || curr->next->redir_in_path != NULL)
        {
            clean_command(head);
            ERR_PRINT("INCORRECT COMBINATION OF PIPES AND REDIRECTION OPERATORS (<, >, >>, |)\n");
            return -1;
        }
        curr = curr->next;
    }

    return 1;
}

Command *parse_line(char *line, Variable **variables){

    //Navigate to first non-white-space character (if there is one).
    int char_index = 0;

    char_index = next_non_whitespace(line, char_index);

    //Early return for empty line or Comments:
    if (end_of_line(line[char_index]))
    {
        return NULL;
    }

    //Check if first input is a variable assignment:
    int is_assignment = is_var_assignment(line, char_index);
    if (is_assignment == -2)
    {
        return (Command *) -1;
    }

    if (is_assignment > -1)
    {
        //Handles variable assignment if input is determined to be one. (no non-system errors should arise).
        handle_var_assignment(line, char_index, is_assignment, variables);
        return NULL;
    }

    //At this point, we are certain line isn't variable assignment, so we replace variables in the line, and
    //handle it as if it were a command.
    char *replaced_line = replace_variables_mk_line(line, *variables);
    if(replaced_line == NULL)
    {
        return (Command *) -1;
    }

    if(replaced_line == (char *) -1)
    {
        exit(EXIT_FAILURE);
    }

    //Navigate to first non-white-space character (if there is one).
    char_index = 0;
    char_index = next_non_whitespace(replaced_line, char_index);

    Command *head = command_parse(replaced_line, char_index);
    free(replaced_line);

    if (head == NULL){return (Command *) -1;}
    if (head == (Command *) -1)
    {
        free_variable(*variables, true); //UNCLEAR IF WE NEED TO DO THIS.
        exit(EXIT_FAILURE);
    }
    int valid_exec_paths = resolve_exec_paths(head, *variables);

    if (valid_exec_paths == -1)
    {
        return (Command *) -1;
    }

    int valid_pipes = input_backward_pass(head);

    if (valid_pipes == -1)
    {
        return (Command *) -1;
    }

    return head;
}

//self explanatory
Variable *find_variable_by_name(const char *var_name, Variable *variables)
{
    Variable *curr_variable = variables;
    while(curr_variable!= NULL)
    {
        if (strcmp(var_name, curr_variable->name) == 0)
        {
            return curr_variable;
        }
        curr_variable = curr_variable->next;
    }
    //Variable not found:
    ERR_PRINT(ERR_VAR_NOT_FOUND, var_name);
    return NULL;
}

//returns length of var_usage if specified string is var_usage, and -1 otherwise.
int verify_var_usage(const char*line, int var_start, int var_end)
{
    if (var_start == var_end)
    {
        ERR_PRINT("VARIABLE PARSE MARKER $ USED WITH NO VARIABLE\n");
        return -1;
    }

    int i = 1;
    if (line[var_start + i] == '{')
    {
        while(var_start + i <= var_end)
        {
            if (line[var_start + i] == '}')
            {
                return i + 1;
            }
            i++;
        }

        ERR_PRINT("OPEN BRACKETS USED FOR VARIABLE ASSIGNMENT WITH NO CLOSING BRACKETS\n");
        return -1;
    }

    return (var_end - var_start) + 1;
}
//returns length of variable usage
int var_usage_length(const char *line, int var_start)
{
    int i = 1;
    while (!end_of_line(line[var_start + i]) && !isspace(line[var_start + i]))
    {
        i++;
    }
    i = verify_var_usage(line, var_start, var_start + i - 1);
    return i;
}

char *resolve_var_name(const char *line, int start, int end)
{
    //start is index of variable usage after $
    if (line[start] == '{')
    {
        return substring_extract(line, start + 1, end - 1);
    }

    return substring_extract(line, start, end);
}

Variable *target_variables_copy(const char *line, int *markers, int markers_len, Variable *variables)
{
    //initialize pointer to replacements.
    Variable *head = malloc(sizeof(Variable));
    if (head == NULL)
    {
        perror("memory allocation failure target_variables_copy head");
        return (Variable *) -1;
    }
    head->next = NULL;
    head->name = NULL;
    head->value = NULL;

    Variable *curr_variable = head;

    int curr_index = 0;
    while(!end_of_line(line[curr_index]))
    {
        int usage_len = markers[curr_index];
        if (usage_len != 0)
        {
            char *var_name = resolve_var_name(line, curr_index + 1,  (curr_index + usage_len) - 1);
            Variable *target_var = find_variable_by_name(var_name, variables);
            if (target_var == NULL)
            {
                free(var_name);
                free_variable(head, true);
                return NULL;
            }

            curr_variable->next = malloc(sizeof(Variable));
            if (curr_variable->next == NULL)
            {
                perror("memory allocation failure target_variables_copy curr_variable->next");
                free(var_name);
                free_variable(head, true);
                return (Variable *) -1;
            }

            curr_variable->name = var_name;
            curr_variable->value = target_var->value;
            curr_variable = curr_variable->next;

            curr_index += usage_len;
            continue;
        }
        curr_index++;
    }

    return head;
}

int length_differences(int *markers, int len_markers, Variable *replacements)
{
    int i = 0;
    int len_diff = 0;
    Variable *curr_usage = replacements;
    while (i < len_markers)
    {
        int usage_len = markers[i];
        if (usage_len != 0)
        {
            int usage_diff = strlen(curr_usage->value) - usage_len;
            len_diff += usage_diff;
            i += usage_len;
            curr_usage = curr_usage->next;
        } else
        {
            i++;
        }
    }
    return len_diff;
}

//returns length of contiguous zeroes from starting point (including starting point)
int zero_length(int *markers, int len_markers, int start)
{
    int len = 1;
    while (markers[start + len] == 0 && (start + len) < len_markers)
    {
        len++;
    }
    return len;
}

int fill_new_line(const char *org_line, char *new_line, int *markers, int len_markers, Variable *replacements)
{
    int i = 0;
    int new_line_ind = 0;
    Variable *curr_usage = replacements;
    while (i < len_markers)
    {
        int usage_len = markers[i];
        if (usage_len != 0)
        {
            char *value = curr_usage->value;
            strncat(new_line, value, strlen(value));

            new_line_ind += strlen(value);
            i += usage_len;
            curr_usage = curr_usage->next;
        } else
        {
            new_line[new_line_ind] = org_line[i];
            new_line_ind++;
            i++;
        }
    }
    return 1;
}

/*
** This function is partially implemented for you, but you may
** scrap the implementation as long as it produces the same result.
**
** Creates a new line on the heap with all named variable *usages*
** replaced with their associated values.
**
** Returns NULL if replacement parsing had an error, or (char *) -1 if
** system calls fail and the shell needs to exit.
*/
char *replace_variables_mk_line(const char *line,
                                Variable *variables){
    // FOLLOWING DOES NOT INCLUDE NULL TERMINATOR
    size_t curr_line_length = strlen(line);
    int markers[curr_line_length];

    //After the following while loop, we should have num_replacements, as well as a markers array, which tracks
    //where variable usages occur, and how long the string is for these variable usages.
    int curr_index = 0;
    int num_replacements = 0;
    while (!end_of_line(line[curr_index]))
    {
        if(line[curr_index] == '$')
        {
            int usage_length = var_usage_length(line, curr_index);

            if (usage_length == -1)
            {
                //ALTERNATIVELY, IF WE COULD NOT RESOLVE VARIABLE, WE COULD SIMPLY CONTINUE AS IF VARIABLE IS A LITERAL
                return NULL;
            }
            num_replacements++;
            markers[curr_index] = usage_length;
            curr_index += usage_length;
            continue;
        }
        markers[curr_index] = 0;
        curr_index++;
    }

    if (num_replacements == 0)
    {
        return strdup(line);
    }
    // Code to determine new length
    // and list of replacements in order
    Variable *ordered_replacements = target_variables_copy(line, markers, curr_line_length, variables);
    if (ordered_replacements == NULL)
    {
        return NULL;
    }

    if (ordered_replacements == (Variable *) -1)
    {
        free_variable(variables, true);
        return (char *) -1;
    }

    int length_diff = length_differences(markers, curr_line_length, ordered_replacements);
    int new_length = curr_line_length + length_diff;

    char *new_line = (char *)malloc(new_length + 1); //Add 1 to take into account null terminator.
    if (new_line == NULL)
    {
        free_variable(ordered_replacements, true);
        free_variable(variables, true);
        perror("replace_variables_mk_line new_line malloc error");
        return (char *) -1;
    }
    memset(new_line, '\0', new_length);

    if (fill_new_line(line, new_line, markers, curr_line_length, ordered_replacements) != 1)
    {
        free_variable(ordered_replacements, true);
        ERR_PRINT("ERROR OCURRED WHILE REPLACING THE NEW LINE\n")
        return NULL;
    }
    //null terminate the new_line
    new_line[new_length] = '\0';
    return new_line;
}

void free_variable(Variable *var, bool recursive){
    if (recursive)
    {
        if(var->next != NULL)
        {
            free_variable(var->next, true);
        }
    }
    if(var->value != NULL){free(var->value);}
    if(var->name != NULL)free(var->name);
    free(var);
}
