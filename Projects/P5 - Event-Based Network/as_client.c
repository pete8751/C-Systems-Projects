#include "as_client.h"


static int connect_to_server(int port, const char *hostname) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("connect_to_server");
        return -1;
    }

    struct sockaddr_in addr;

    // Allow sockets across machines.
    addr.sin_family = AF_INET;
    // The port the server will be listening on.
    // htons() converts the port number to network byte order.
    // This is the same as the byte order of the big-endian architecture.
    addr.sin_port = htons(port);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(addr.sin_zero), 0, 8);

    // Lookup host IP address.
    struct hostent *hp = gethostbyname(hostname);
    if (hp == NULL) {
        ERR_PRINT("Unknown host: %s\n", hostname);
        return -1;
    }

    addr.sin_addr = *((struct in_addr *) hp->h_addr);

    // Request connection to server.
    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return -1;
    }

    return sockfd;
}


/*
** Helper for: list_request
** This function reads from the socket until it finds a network newline.
** This is processed as a list response for a single library file,
** of the form:
**                   <index>:<filename>\r\n
**
** returns index on success, -1 on error
** filename is a heap allocated string pointing to the parsed filename
*/
static int get_next_filename(int sockfd, char **filename) {
    static int bytes_in_buffer = 0;
    static char buf[RESPONSE_BUFFER_SIZE];

    while((*filename = find_network_newline(buf, &bytes_in_buffer)) == NULL) {
        int num = read(sockfd, buf + bytes_in_buffer,
                       RESPONSE_BUFFER_SIZE - bytes_in_buffer);
        if (num < 0) {
            perror("list_request");
            return -1;
        }
        bytes_in_buffer += num;
        if (bytes_in_buffer == RESPONSE_BUFFER_SIZE) {
            ERR_PRINT("Response buffer filled without finding file\n");
            ERR_PRINT("Bleeding data, this shouldn't happen, but not giving up\n");
            memmove(buf, buf + BUFFER_BLEED_OFF, RESPONSE_BUFFER_SIZE - BUFFER_BLEED_OFF);
        }
    }

    char *parse_ptr = strtok(*filename, ":");
    int index = strtol(parse_ptr, NULL, 10);
    parse_ptr = strtok(NULL, ":");
    // moves the filename to the start of the string (overwriting the index)
    memmove(*filename, parse_ptr, strlen(parse_ptr) + 1);

    return index;
}

int populate_filenames(int sockfd, char ***filenames_ptr)
{
    //set up filename pointer to filename string, this will store each filename one by one
    char **filename = malloc(sizeof(char *));
    int len = get_next_filename(sockfd, filename) + 1;
    if (len == 0) {return -1;} //NO FILE ERROR (HOW TO HANDLE?)

    //resizing filenames array, and begin reverse populating process
    *filenames_ptr = realloc(*filenames_ptr, len * sizeof(char *));
    if (*filenames_ptr == NULL)
    {
        perror("realloc");
        exit(EXIT_FAILURE);
    }

    (*filenames_ptr)[len - 1] = strdup(filename[0]);
    free(filename[0]);

    //populate filename pointer array in reverse.
    for (int i = len - 2; i >= 0; i--)
    {
        int ret = get_next_filename(sockfd, filename);
        if (ret == -1) {return -1;}
        (*filenames_ptr)[i] = strdup(filename[0]);
        free(filename[0]);
    }

    free(filename);
    return len;
}

int list_request(int sockfd, Library *library) {
    //send the list request:
    const char *message = "LIST\r\n";

    if (write_precisely(sockfd, message, 6) != 6) {
        perror("write failed");
        exit(EXIT_FAILURE);
    }

    //first we reset all parameters of library.
    _free_library(library);

    //create and populate the filename pointer array, and then populate library.
    char **filenames = malloc(sizeof(char *));
    int len = populate_filenames(sockfd, &filenames);
    if (len == -1){return len;}

    library->files = filenames;
    library->num_files = (uint32_t) len;

    //print files in list.
    for (int i = 0; i < len; i++)
    {
        printf("%d: %s\n", i, library->files[i]);
    }

    return 0;
}

/*
** Get the permission of the library directory. If the library 
** directory does not exist, this function shall create it.
**
** library_dir: the path of the directory storing the audio files
** perpt:       an output parameter for storing the permission of the 
**              library directory.
**
** returns 0 on success, -1 on error
*/
static int get_library_dir_permission(const char *library_dir, mode_t * perpt) {
    struct stat dir_info;

    DIR *dir = opendir(library_dir);
    if (dir)
    {
        closedir(dir);
    }
    else
    {
        int error = mkdir(library_dir, 0777);
        if (error == -1)
        {
            printf("failed to create directory: %s", library_dir);
            return -1;
        }
    }

    int status = stat(library_dir, &dir_info);
    if (status == -1)
    {
        // Failed to obtain the file status
        perror("stat");
        return -1;
    }

    *perpt = dir_info.st_mode;

    return 0;
}

/*
** Creates any directories needed within the library dir so that the file can be
** written to the correct destination. All directories will inherit the permissions
** of the library_dir.
**
** This function is recursive, and will create all directories needed to reach the
** file in destination.
**
** Destination shall be a path without a leading /
**
** library_dir can be an absolute or relative path, and can optionally end with a '/'
**
*/
static void create_missing_directories(const char *destination, const char *library_dir) {
    // get the permissions of the library dir
    mode_t permissions;
    if (get_library_dir_permission(library_dir, &permissions) == -1) {
        exit(1);
    }

    char *str_de_tokville = strdup(destination);
    if (str_de_tokville == NULL) {
        perror("create_missing_directories");
        return;
    }

    char *before_filename = strrchr(str_de_tokville, '/');
    if (!before_filename){
        goto free_tokville;
    }

    char *path = malloc(strlen(library_dir) + strlen(destination) + 2);
    if (path == NULL) {
        goto free_tokville;
    } *path = '\0';

    char *dir = strtok(str_de_tokville, "/");
    if (dir == NULL){
        goto free_path;
    }
    strcpy(path, library_dir);
    if (path[strlen(path) - 1] != '/') {
        strcat(path, "/");
    }
    strcat(path, dir);

    while (dir != NULL && dir != before_filename + 1) {
        #ifdef DEBUG
        printf("Creating directory %s\n", path);
        #endif
        if (mkdir(path, permissions) == -1) {
            if (errno != EEXIST) {
                perror("create_missing_directories");
                goto free_path;
            }
        }
        dir = strtok(NULL, "/");
        if (dir != NULL) {
            strcat(path, "/");
            strcat(path, dir);
        }
    }
free_path:
    free(path);
free_tokville:
    free(str_de_tokville);
}


/*
** Helper for: get_file_request
*/
static int file_index_to_fd(uint32_t file_index, const Library * library){
    create_missing_directories(library->files[file_index], library->path);

    char *filepath = _join_path(library->path, library->files[file_index]);
    if (filepath == NULL) {
        return -1;
    }

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    #ifdef DEBUG
    printf("Opened file %s\n", filepath);
    #endif
    free(filepath);
    if (fd < 0 ) {
        perror("file_index_to_fd");
        return -1;
    }

    return fd;
}


int get_file_request(int sockfd, uint32_t file_index, const Library * library){
    #ifdef DEBUG
    printf("Getting file %s\n", library->files[file_index]);
    #endif

    int file_dest_fd = file_index_to_fd(file_index, library);
    if (file_dest_fd == -1) {
        return -1;
    }

    int result = send_and_process_stream_request(sockfd, file_index, -1, file_dest_fd);
    if (result == -1) {
        return -1;
    }

    return 0;
}


int start_audio_player_process(int *audio_out_fd) {
     char *args[] = AUDIO_PLAYER_ARGS;
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return -1;
    }

    int pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Child process
        close(pipefd[1]); // Close the write end of the pipe

        // set stdin of child to read from pipe, and close read end of pipe.
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        close(pipefd[0]);

        execvp(AUDIO_PLAYER, args);
        exit(EXIT_FAILURE);
    }
    // Parent process
    close(pipefd[0]); // Close the read end of the pipe
    sleep(AUDIO_PLAYER_BOOT_DELAY); //sleep while child boots up mp4 process

    //set input argument to point to write end of pipe to child.
    *audio_out_fd = pipefd[1];

    return pid;
}


static void _wait_on_audio_player(int audio_player_pid) {
    int status;
    if (waitpid(audio_player_pid, &status, 0) == -1) {
        perror("_wait_on_audio_player");
        return;
    }
    if (WIFEXITED(status)) {
        fprintf(stderr, "Audio player exited with status %d\n", WEXITSTATUS(status));
    } else {
        printf("Audio player exited abnormally\n");
    }
}


int stream_request(int sockfd, uint32_t file_index) {
    int audio_out_fd;
    int audio_player_pid = start_audio_player_process(&audio_out_fd);

    int result = send_and_process_stream_request(sockfd, file_index, audio_out_fd, -1);
    if (result == -1) {
        ERR_PRINT("stream_request: send_and_process_stream_request failed\n");
        return -1;
    }

    _wait_on_audio_player(audio_player_pid);

    return 0;
}


int stream_and_get_request(int sockfd, uint32_t file_index, const Library * library) {
    int audio_out_fd;
    int audio_player_pid = start_audio_player_process(&audio_out_fd);

    #ifdef DEBUG
    printf("Getting file %s\n", library->files[file_index]);
    #endif

    int file_dest_fd = file_index_to_fd(file_index, library);
    if (file_dest_fd == -1) {
        ERR_PRINT("stream_and_get_request: file_index_to_fd failed\n");
        return -1;
    }

    int result = send_and_process_stream_request(sockfd, file_index,
                                                 audio_out_fd, file_dest_fd);
    if (result == -1) {
        ERR_PRINT("stream_and_get_request: send_and_process_stream_request failed\n");
        return -1;
    }

    _wait_on_audio_player(audio_player_pid);

    return 0;
}

int create_and_send_streamreq(int sockfd, uint32_t file_index)
{
    //chatgpt wrote this helper to create and send the stream request.
    const char *message = "STREAM\r\n";
    uint32_t network_index = htonl(file_index);

    // Calculate the total size of the message
    size_t message_length = strlen(message);
    size_t total_length = message_length + sizeof(uint32_t);

    // Allocate a buffer to hold the entire message
    char buffer[total_length];

    // Copy the message "STREAM\r\n" to the buffer
    memcpy(buffer, message, message_length);

    // Copy the network-order file index to the buffer
    memcpy(buffer + message_length, &network_index, sizeof(uint32_t));

    // Send the message
    if (write_precisely(sockfd, buffer, total_length) != total_length) {
        perror("write_precisely");
        return -1;
    }

    return 0; // Success
}

//Following helper was written by chatgpt to convert uint8 arrays to uint32 values.
uint32_t convert_uint8_to_uint32(uint8_t *array) {
    // Assuming the array contains 4 bytes (uint32_t size)
    // Convert bytes to a 32-bit integer using bitwise operations
    uint32_t result = 0;
    result |= (uint32_t)array[0] << 24;
    result |= (uint32_t)array[1] << 16;
    result |= (uint32_t)array[2] << 8;
    result |= (uint32_t)array[3];
    return result;
}

uint32_t read_file_size(int sockfd)
{
    uint8_t file_size_buf[4];
    if (read_precisely(sockfd, file_size_buf, 4) != 4)
    {
        printf("READ PRECISE ERROR");
        return -1;
    }

    uint32_t network_index = convert_uint8_to_uint32(file_size_buf);
    return network_index;
}

 //   buf_to_buf(read_buf, write_buf, write_buf_cap, bytes_read);

void buf_to_buf(uint8_t *out_buf, uint8_t *in_buf, ssize_t in_start, ssize_t num)
{
    //this assumes that the output buffer has enough space for num more bytes to be appended.
    for (int i = 0; i < num; i++)
    {
        in_buf[in_start + i] = out_buf[i];
    }
}
int write_one(int *pos, uint8_t **write_buf, int write_buf_cap)
{
    int curr_pos = *pos;
    if (curr_pos != 0)
    {
        *pos = 0;
        return curr_pos;
    }

    return 0;
}

int write_both(int *file_pos, int *audio_pos, uint8_t **write_buf, int write_buf_cap)
{
    int curr_file_pos = *file_pos;
    int curr_audio_pos = *audio_pos;
    int num_written = 0;
    if (curr_file_pos - curr_audio_pos > 0)
    {
        *file_pos -= curr_audio_pos;
        *audio_pos = 0;
        num_written = curr_audio_pos;
    }
    else
    {
        *audio_pos -= curr_file_pos;
        *file_pos = 0;
        num_written = curr_file_pos;
    }

    return num_written;
}
//adjust_readwrite_pos(&file_pos, &audio_pos, &write_buf, write_buf_cap, out_no);
int adjust_readwrite_pos(int *file_pos, int *audio_pos, uint8_t **write_buf, int write_buf_cap, int out_no)
{
    int ret = 0; //this will be number of bytes removed from write buffer.
    int curr_file_pos = *file_pos;
    int curr_audio_pos = *audio_pos;
    if (out_no == 0)
    {
        if (curr_file_pos != 0 && curr_audio_pos != 0)
        {
            ret = write_both(file_pos, audio_pos, write_buf, write_buf_cap);
        }
    }

    if (out_no == 1)
    {
        ret = write_one(audio_pos, write_buf, write_buf_cap);
    }

    if (out_no == 2)
    {
        ret = write_one(file_pos, write_buf, write_buf_cap);
    }

    return ret;
}

int get_max(int i, int j, int k)
{
    int max;
    if (j >= i) {max = j;}
    if (i > j) {max = i;}
    if (k > max) {max = k;}
    return max;
}

int process_stream_request(uint32_t file_size, int out_no, int audio_out_fd, int file_dest_fd, int sockfd)
{
    uint8_t read_buf[NETWORK_PRE_DYNAMIC_BUFF_SIZE];
    uint8_t *write_buf = malloc(sizeof(uint8_t));

    uint32_t total_bytes_out = 0;
    int write_buf_cap = 0;
    int file_pos = 0;
    int audio_pos = 0;
    int max = get_max(audio_out_fd, file_dest_fd, sockfd);

    fd_set readfds_reset;
    fd_set writefds_reset;
    struct timeval timeout;
    timeout.tv_sec = SELECT_TIMEOUT_SEC;
    timeout.tv_usec = SELECT_TIMEOUT_USEC;

    FD_ZERO(&readfds_reset);
    FD_ZERO(&writefds_reset);

    //Set the write/reads fd sets.
    FD_SET(sockfd, &readfds_reset);
    if (out_no == 0)
    {
        FD_SET(audio_out_fd, &writefds_reset);
        FD_SET(file_dest_fd, &writefds_reset);
    }
    if (out_no == 1)
    {
        FD_SET(audio_out_fd, &writefds_reset);
    }
    if (out_no == 2)
    {
        FD_SET(file_dest_fd, &writefds_reset);
    }

    // Create file descriptor sets, and set them to zero.
    while (total_bytes_out != file_size)
    {
        fd_set readfds = readfds_reset;
        fd_set writefds = writefds_reset;

        //SET TO SELECT TO MAXIMUM
        int activity = select(max + 1, &readfds, &writefds, NULL, &timeout);
        if (activity == -1)
        {
            perror("select");
            exit(EXIT_FAILURE);
        } else if (activity == 0) {
            continue;
        }

        if (FD_ISSET(sockfd, &readfds))
        {
            int bytes_read = read(sockfd, read_buf, NETWORK_PRE_DYNAMIC_BUFF_SIZE);
            write_buf = realloc(write_buf, (write_buf_cap + bytes_read) * sizeof(uint8_t));
            if (write_buf == NULL)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            //move bytes from read_buf to end of write_buf
            buf_to_buf(read_buf, write_buf, write_buf_cap, bytes_read);
            write_buf_cap += bytes_read;
        }

        if (out_no != 2 && FD_ISSET(audio_out_fd, &writefds) && audio_pos != write_buf_cap)
        {
            int bytes_played = write(audio_out_fd, write_buf + audio_pos, write_buf_cap - audio_pos);
            audio_pos += bytes_played;
        }

        if (out_no != 1 && FD_ISSET(file_dest_fd, &writefds) && file_pos != write_buf_cap)
        {
            int bytes_written = write(file_dest_fd, write_buf + file_pos, write_buf_cap - file_pos);
            file_pos += bytes_written;
        }

        int bytes_out = adjust_readwrite_pos(&file_pos, &audio_pos, &write_buf, write_buf_cap, out_no);

        if (bytes_out != 0)
        {
            memmove(write_buf, write_buf + bytes_out, write_buf_cap - bytes_out);
            write_buf_cap -= bytes_out;
            write_buf = realloc(write_buf, write_buf_cap * sizeof(uint8_t));
            if (write_buf_cap != 0 && write_buf == NULL)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
        }

        total_bytes_out += bytes_out;
    }

    free(write_buf);
    return total_bytes_out;
}

int send_and_process_stream_request(int sockfd, uint32_t file_index,
                                    int audio_out_fd, int file_dest_fd) {
    //send the stream request:
    int ret = create_and_send_streamreq(sockfd, file_index);
    if (ret == -1){return -1;}

    int out_no = 0;
    if (file_dest_fd == -1){out_no = 1;}
    if (audio_out_fd == -1){out_no = 2;}

    uint32_t file_size = read_file_size(sockfd);
    ret = process_stream_request(file_size, out_no, audio_out_fd, file_dest_fd, sockfd);
    if (ret == -1){return -1;}

    //close fds
    if (out_no == 0)
    {
        close(audio_out_fd);
        close(file_dest_fd);
    }
    if (out_no == 1){close(audio_out_fd);}
    if (out_no == 2){close(file_dest_fd);}

    return 0;
}


static void _print_shell_help(){
    printf("Commands:\n");
    printf("  list: List the files in the library\n");
    printf("  get <file_index>: Get a file from the library\n");
    printf("  stream <file_index>: Stream a file from the library (without saving it)\n");
    printf("  stream+ <file_index>: Stream a file from the library\n");
    printf("                        and save it to the local library\n");
    printf("  help: Display this help message\n");
    printf("  quit: Quit the client\n");
}


/*
** Shell to handle the client options
** ----------------------------------
** This function is a mini shell to handle the client options. It prompts the
** user for a command and then calls the appropriate function to handle the
** command. The user can enter the following commands:
** - "list" to list the files in the library
** - "get <file_index>" to get a file from the library
** - "stream <file_index>" to stream a file from the library (without saving it)
** - "stream+ <file_index>" to stream a file from the library and save it to the local library
** - "help" to display the help message
** - "quit" to quit the client
*/
static int client_shell(int sockfd, const char *library_directory) {
    char buffer[REQUEST_BUFFER_SIZE];
    char *command;
    int file_index;

    Library library = {"client", library_directory, NULL, 0};

    while (1) {
        if (library.files == 0) {
            printf("Server library is empty or not retrieved yet\n");
        }

        printf("Enter a command: ");
        if (fgets(buffer, REQUEST_BUFFER_SIZE, stdin) == NULL) {
            perror("client_shell");
            goto error;
        }

        command = strtok(buffer, " \n");
        if (command == NULL) {
            continue;
        }

        // List Request -- list the files in the library
        if (strcmp(command, CMD_LIST) == 0) {
            if (list_request(sockfd, &library) == -1) {
                goto error;
            }


        // Get Request -- get a file from the library
        } else if (strcmp(command, CMD_GET) == 0) {
            char *file_index_str = strtok(NULL, " \n");
            if (file_index_str == NULL) {
                printf("Usage: get <file_index>\n");
                continue;
            }
            file_index = strtol(file_index_str, NULL, 10);
            if (file_index < 0 || file_index >= library.num_files) {
                printf("Invalid file index\n");
                continue;
            }

            if (get_file_request(sockfd, file_index, &library) == -1) {
                goto error;
            }

        // Stream Request -- stream a file from the library (without saving it)
        } else if (strcmp(command, CMD_STREAM) == 0) {
            char *file_index_str = strtok(NULL, " \n");
            if (file_index_str == NULL) {
                printf("Usage: stream <file_index>\n");
                continue;
            }
            file_index = strtol(file_index_str, NULL, 10);
            if (file_index < 0 || file_index >= library.num_files) {
                printf("Invalid file index\n");
                continue;
            }

            if (stream_request(sockfd, file_index) == -1) {
                goto error;
            }

        // Stream and Get Request -- stream a file from the library and save it to the local library
        } else if (strcmp(command, CMD_STREAM_AND_GET) == 0) {
            char *file_index_str = strtok(NULL, " \n");
            if (file_index_str == NULL) {
                printf("Usage: stream+ <file_index>\n");
                continue;
            }
            file_index = strtol(file_index_str, NULL, 10);
            if (file_index < 0 || file_index >= library.num_files) {
                printf("Invalid file index\n");
                continue;
            }

            if (stream_and_get_request(sockfd, file_index, &library) == -1) {
                goto error;
            }

        } else if (strcmp(command, CMD_HELP) == 0) {
            _print_shell_help();

        } else if (strcmp(command, CMD_QUIT) == 0) {
            printf("Quitting shell\n");
            break;

        } else {
            printf("Invalid command\n");
        }
    }

    _free_library(&library);
    return 0;
error:
    _free_library(&library);
    return -1;
}


static void print_usage() {
    printf("Usage: as_client [-h] [-a NETWORK_ADDRESS] [-p PORT] [-l LIBRARY_DIRECTORY]\n");
    printf("  -h: Print this help message\n");
    printf("  -a NETWORK_ADDRESS: Connect to server at NETWORK_ADDRESS (default 'localhost')\n");
    printf("  -p  Port to listen on (default: " XSTR(DEFAULT_PORT) ")\n");
    printf("  -l LIBRARY_DIRECTORY: Use LIBRARY_DIRECTORY as the library directory (default 'as-library')\n");
}


int main(int argc, char * const *argv) {
    int opt;
    int port = DEFAULT_PORT;
    const char *hostname = "localhost";
    const char *library_directory = "saved";

    while ((opt = getopt(argc, argv, "ha:p:l:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'a':
                hostname = optarg;
                break;
            case 'p':
                port = strtol(optarg, NULL, 10);
                if (port < 0 || port > 65535) {
                    ERR_PRINT("Invalid port number %d\n", port);
                    return 1;
                }
                break;
            case 'l':
                library_directory = optarg;
                break;
            default:
                print_usage();
                return 1;
        }
    }

    printf("Connecting to server at %s:%d, using library in %s\n",
           hostname, port, library_directory);

    int sockfd = connect_to_server(port, hostname);
    if (sockfd == -1) {
        return -1;
    }

    int result = client_shell(sockfd, library_directory);
    if (result == -1) {
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}
