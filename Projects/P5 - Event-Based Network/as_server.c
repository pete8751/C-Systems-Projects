#include "as_server.h"


int init_server_addr(int port, struct sockaddr_in *addr){
    // Allow sockets across machines.
    addr->sin_family = AF_INET;
    // The port the process will listen on.
    addr->sin_port = htons(port);
    // Clear this field; sin_zero is used for padding for the struct.
    memset(&(addr->sin_zero), 0, 8);

    // Listen on all network interfaces.
    addr->sin_addr.s_addr = INADDR_ANY;

    return 0;
}


int set_up_server_socket(const struct sockaddr_in *server_options, int num_queue) {
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc < 0) {
        perror("socket");
        exit(1);
    }

    printf("Listen socket created\n");

    // Make sure we can reuse the port immediately after the
    // server terminates. Avoids the "address in use" error
    int on = 1;
    int status = setsockopt(soc, SOL_SOCKET, SO_REUSEADDR,
                            (const char *) &on, sizeof(on));
    if (status < 0) {
        perror("setsockopt");
        exit(1);
    }

    // Associate the process with the address and a port
    if (bind(soc, (struct sockaddr *)server_options, sizeof(*server_options)) < 0) {
        // bind failed; could be because port is in use.
        perror("bind");
        exit(1);
    }

    printf("Socket bound to port %d\n", ntohs(server_options->sin_port));

    // Set up a queue in the kernel to hold pending connections.
    if (listen(soc, num_queue) < 0) {
        // listen failed
        perror("listen");
        exit(1);
    }

    printf("Socket listening for connections\n");

    return soc;
}


ClientSocket accept_connection(int listenfd) {
    ClientSocket client;
    socklen_t addr_size = sizeof(client.addr);
    client.socket = accept(listenfd, (struct sockaddr *)&client.addr,
                               &addr_size);
    if (client.socket < 0) {
        perror("accept_connection: accept");
        exit(-1);
    }

    // print out a message that we got the connection
    printf("Server got a connection from %s, port %d\n",
           inet_ntoa(client.addr.sin_addr), ntohs(client.addr.sin_port));

    return client;
}

int response_len(char **file_names, int num_files)
{
    int response_len = 0;
    for (int i = 0; i < num_files; i++)
    {
        response_len += strlen(file_names[i]);
    }

    response_len += (2 * num_files); //take into account network newline characters.
    return response_len;
}
int get_index_strlen(int index)
{
    //chatgpt wrote the structure for this helper.
    int count = 0;

    // Handle the case when number is 0 separately
    if (index== 0) {
        return 1;
    }

    // Count the number of digits
    while (index != 0) {
        index /= 10; // Divide the number by 10
        count++;      // Increment the digit count
    }

    return count;
}

char *file_index_string(int index)
{
    int num_digits = get_index_strlen(index);
    char *index_str= malloc((num_digits + 2) * sizeof(char));
    if (index_str == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Format a string using sprintf
    sprintf(index_str, "%d", index);

    // Add a colon to the end of the string
    strcat(index_str, ":");

    // Ensure the string is null-terminated after the colon
    index_str[strlen(index_str)] = '\0';

    return index_str;
}

char *create_msg_string(char **file_names, int num_files)
{
    //allocating memory for the message, and setting msg values
    char *msg = malloc(sizeof(char));
    if (msg == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    msg[0] = '\0';

    int old_msg_len = 0;
    int curr_msg_len = 0;

    for (int i = (num_files - 1); i >= 0; i--)
    {
        char *curr_index_str = file_index_string(i);
        char *curr_file_name = file_names[i];

        int index_len = strlen(curr_index_str);
        int file_name_len = strlen(curr_file_name); //strlen does not include null terminator

        curr_msg_len += index_len + file_name_len + 3;
        msg = realloc(msg, curr_msg_len * sizeof(char));
        if (msg == NULL)
        {
            perror("realloc");
            exit(EXIT_FAILURE);
        }
        msg[old_msg_len] = '\0';

        strncat(msg, curr_index_str, index_len);
        free(curr_index_str);

        strncat(msg, curr_file_name, file_name_len);
        strncat(msg, "\r\n", 3);

        //strncat(msg, '\r\n');
        old_msg_len = curr_msg_len;
    }

    return msg;
}

int list_request_response(const ClientSocket * client, const Library *library) {
    if (library->num_files == 0)
    {
        printf("No files in library");
        return -1;
    }

    char *msg = create_msg_string(library->files, library->num_files);

    ssize_t bytes_sent =  write_precisely(client->socket, msg, strlen(msg));
    if (bytes_sent < 0) {
        perror("Error sending data to client");
        return -1;
    }

    free(msg);

    return 0;
}


static int _load_file_size_into_buffer(FILE *file, uint8_t *buffer) {
    if (fseek(file, 0, SEEK_END) < 0) {
        ERR_PRINT("Error seeking to end of file\n");
        return -1;
    }
    uint32_t file_size = ftell(file);
    if (fseek(file, 0, SEEK_SET) < 0) {
        ERR_PRINT("Error seeking to start of file\n");
        return -1;
    }
    buffer[0] = (file_size >> 24) & 0xFF;
    buffer[1] = (file_size >> 16) & 0xFF;
    buffer[2] = (file_size >> 8) & 0xFF;
    buffer[3] = file_size & 0xFF;
    return 0;
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

int get_file_index(const ClientSocket * client, uint8_t *post_req, int num_pr_bytes)
{
    int num_to_read = 4 - num_pr_bytes;

    //error handling for if num_pr_bytes > 4
    if (num_to_read < 0)
    {
        printf("number bytes recieved for file index is greater than 4, number recieved: %d\n", num_pr_bytes);
        return -1;
    }

    //the following ensures that the last 4 bytes of post_req is populated by 4 bytes containing file index.
    if (num_to_read != 0)
    {
        int bytes_read = read_precisely(client->socket, post_req + num_pr_bytes, num_to_read);
        if(bytes_read != num_to_read)
        {
            printf("Read Precisely Error in get_file_index");
            return -1;
        }
    }

    uint32_t index = convert_uint8_to_uint32(post_req);
    return index;
}

char *get_filepath_from_index(const Library *library, int file_index)
{
    //Handling for bad index.
    if (file_index < 0 || file_index >= library->num_files )
    {
        printf("ERROR: file index %d is out of range\n", file_index);
        return NULL;
    }

    char *rel_path = _join_path(library->path, library->files[file_index]);
    return rel_path;
}

int send_filesize(int socket, FILE *file_ptr, uint8_t *buffer)
{
    //the following loads the file size into the buffer.
    if (_load_file_size_into_buffer(file_ptr, buffer) < 0)
    {
        printf("Error loading file size into buffer");
        return -1;
    }

    if (write_precisely(socket, buffer, 4) < 4)
    {
      perror("Error sending data");
      return -1;
    }

    if (ferror(file_ptr))
    {
       perror("Error reading from file");
       return -1;
    }

    //return file size
    return convert_uint8_to_uint32(buffer);
}

int send_stream(int socket, FILE *file_ptr, uint8_t *response_buffer, uint32_t num_to_write)
{
    uint32_t next_write_amt = 0;
    while (num_to_write != 0)
    {
        //setting the next write size.
        next_write_amt = STREAM_CHUNK_SIZE;
        if  (num_to_write <= STREAM_CHUNK_SIZE)
        {
            next_write_amt = num_to_write;
        }

        if (fread(response_buffer, 1, next_write_amt, file_ptr) != next_write_amt)
        {
            if (feof(file_ptr)) {
                printf("End of file reached.\n");
            } else {
                perror("Error reading from file");
            }
            return -1;
        }

        if (ferror(file_ptr))
        {
            perror("Error reading from file");
        }

        if (write_precisely(socket, response_buffer, next_write_amt) != next_write_amt)
        {
            perror("Error sending data");
            return -1;
        }

        num_to_write -= next_write_amt;
    }

    return 0;
}

int send_response(const ClientSocket * client, FILE *file_ptr)
{
        //the following initializes a response buffer of size STREAM_CHUNK_SIZE bytes
        uint8_t *response_buffer = (uint8_t *)malloc(STREAM_CHUNK_SIZE);
        if (response_buffer == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        //the following loads the file size into the buffer.
        uint32_t num_to_write = send_filesize(client->socket, file_ptr, response_buffer);
        if (num_to_write == 0){return -1;}

        if (send_stream(client->socket, file_ptr, response_buffer, num_to_write) != 0){return -1;}
        fprintf(stdout, "total size: %d\n", num_to_write);
        free(response_buffer);
        return 0;
}

//num_pr_bytes is number of bytes in post_req.
int stream_request_response(const ClientSocket * client, const Library *library,
                            uint8_t *post_req, int num_pr_bytes) {
    //the following gets the file index.
    int file_index = get_file_index(client, post_req, num_pr_bytes);
    if (file_index == -1){return -1;}

    char *file_path = get_filepath_from_index(library, file_index);
    if (file_path == NULL) {return -1;}

    FILE *file_ptr = fopen(file_path, "r");
    if (file_ptr == NULL)
    {
        perror("Error opening file");
        return -1;
    }

    free(file_path);
    int send_success = send_response(client, file_ptr);
    fclose(file_ptr);

    return send_success;
}


static Library make_library(const char *path){
    Library library;
    library.path = path;
    library.num_files = 0;
    library.files = NULL;
    library.name = "server";

    printf("Initializing library\n");
    printf("Library path: %s\n", library.path);

    return library;
}


static void _wait_for_children(pid_t **client_conn_pids, int *num_connected_clients, uint8_t immediate) {
    int status;
    for (int i = 0; i < *num_connected_clients; i++) {
        int options = immediate ? WNOHANG : 0;
        if (waitpid((*client_conn_pids)[i], &status, options) > 0) {
            if (WIFEXITED(status)) {
                printf("Client process %d terminated\n", (*client_conn_pids)[i]);
                if (WEXITSTATUS(status) != 0) {
                    fprintf(stderr, "Client process %d exited with status %d\n",
                            (*client_conn_pids)[i], WEXITSTATUS(status));
                }
            } else {
                fprintf(stderr, "Client process %d terminated abnormally\n",
                        (*client_conn_pids)[i]);
            }

            for (int j = i; j < *num_connected_clients - 1; j++) {
                (*client_conn_pids)[j] = (*client_conn_pids)[j + 1];
            }

            (*num_connected_clients)--;
            *client_conn_pids = (pid_t *)realloc(*client_conn_pids,
                                                 (*num_connected_clients)
                                                 * sizeof(pid_t));
        }
    }
}

/*
** Create a server socket and listen for connections
**
** port: the port number to listen on.
** 
** On success, returns the file descriptor of the socket.
** On failure, return -1.
*/
static int initialize_server_socket(int port) {
	struct sockaddr_in server_addr;

	//initializing the sockaddr_in struct:
	int socket_fd = init_server_addr(port, &server_addr);

	if (socket_fd == -1)
	{
	    printf("init_server_addr Error");
	    return socket_fd;
	}

	//set up the socket using the sockaddr_in struct with MAX_PENDING num_queue.
	socket_fd = set_up_server_socket(&server_addr, MAX_PENDING);

	if (socket_fd == -1)
	{
	    printf("set_up_server_socket Error");
	}

	return socket_fd;
}

int run_server(int port, const char *library_directory){
    Library library = make_library(library_directory);
    if (scan_library(&library) < 0) {
        ERR_PRINT("Error scanning library\n");
        return -1;
    }

    int num_connected_clients = 0;
    pid_t *client_conn_pids = NULL;

	int incoming_connections = initialize_server_socket(port);
	if (incoming_connections == -1) {
		return -1;	
	}
	
    int maxfd = incoming_connections;
    fd_set incoming;
    SET_SERVER_FD_SET(incoming, incoming_connections);
    int num_intervals_without_scan = 0;

    while(1) {
        if (num_intervals_without_scan >= LIBRARY_SCAN_INTERVAL) {
            if (scan_library(&library) < 0) {
                fprintf(stderr, "Error scanning library\n");
                return 1;
            }
            num_intervals_without_scan = 0;
        }

        struct timeval select_timeout = SELECT_TIMEOUT;
        if(select(maxfd + 1, &incoming, NULL, NULL, &select_timeout) < 0){
            perror("run_server");
            exit(1);
        }

        if (FD_ISSET(incoming_connections, &incoming)) {
            ClientSocket client_socket = accept_connection(incoming_connections);

            pid_t pid = fork();
            if(pid == -1){
                perror("run_server");
                exit(-1);
            }
            // child process
            if(pid == 0){
                close(incoming_connections);
                free(client_conn_pids);
                int result = handle_client(&client_socket, &library);
                _free_library(&library);
                close(client_socket.socket);
                return result;
            }
            close(client_socket.socket);
            num_connected_clients++;
            client_conn_pids = (pid_t *)realloc(client_conn_pids,
                                               (num_connected_clients)
                                               * sizeof(pid_t));
            client_conn_pids[num_connected_clients - 1] = pid;
        }
        if (FD_ISSET(STDIN_FILENO, &incoming)) {
            if (getchar() == 'q') break;
        }

        num_intervals_without_scan++;
        SET_SERVER_FD_SET(incoming, incoming_connections);

        // Immediate return wait for client processes
        _wait_for_children(&client_conn_pids, &num_connected_clients, 1);
    }

    printf("Quitting server\n");
    close(incoming_connections);
    _wait_for_children(&client_conn_pids, &num_connected_clients, 0);
    _free_library(&library);
    return 0;
}


static uint8_t _is_file_extension_supported(const char *filename){
    static const char *supported_file_exts[] = SUPPORTED_FILE_EXTS;

    for (int i = 0; i < sizeof(supported_file_exts)/sizeof(char *); i++) {
        char *files_ext = strrchr(filename, '.');
        if (files_ext != NULL && strcmp(files_ext, supported_file_exts[i]) == 0) {
            return 1;
        }
    }

    return 0;
}


static int _depth_scan_library(Library *library, char *current_path){

    char *path_in_lib = _join_path(library->path, current_path);
    if (path_in_lib == NULL) {
        return -1;
    }

    DIR *dir = opendir(path_in_lib);
    if (dir == NULL) {
        perror("scan_library");
        return -1;
    }
    free(path_in_lib);

    struct dirent *entry;
    while((entry = readdir(dir)) != NULL) {
        if ((entry->d_type == DT_REG) &&
            _is_file_extension_supported(entry->d_name)) {
            library->files = (char **)realloc(library->files,
                                              (library->num_files + 1)
                                              * sizeof(char *));
            if (library->files == NULL) {
                perror("_depth_scan_library");
                return -1;
            }

            library->files[library->num_files] = _join_path(current_path, entry->d_name);
            if (library->files[library->num_files] == NULL) {
                perror("scan_library");
                return -1;
            }
            #ifdef DEBUG
            printf("Found file: %s\n", library->files[library->num_files]);
            #endif
            library->num_files++;

        } else if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char *new_path = _join_path(current_path, entry->d_name);
            if (new_path == NULL) {
                return -1;
            }

            #ifdef DEBUG
            printf("Library scan descending into directory: %s\n", new_path);
            #endif

            int ret_code = _depth_scan_library(library, new_path);
            free(new_path);
            if (ret_code < 0) {
                return -1;
            }
        }
    }

    closedir(dir);
    return 0;
}


// This function is implemented recursively and uses realloc to grow the files array
// as it finds more files in the library. It ignores MAX_FILES.
int scan_library(Library *library) {
    // Maximal flexibility, free the old strings and start again
    // A hash table leveraging inode number would be a better way to do this
    #ifdef DEBUG
    printf("^^^^ ----------------------------------- ^^^^\n");
    printf("Freeing library\n");
    #endif
    _free_library(library);

    #ifdef DEBUG
    printf("Scanning library\n");
    #endif
    int result = _depth_scan_library(library, "");
    #ifdef DEBUG
    printf("vvvv ----------------------------------- vvvv\n");
    #endif
    return result;
}


int handle_client(const ClientSocket * client, Library *library) {
    char *request = NULL;
    uint8_t *request_buffer = (uint8_t *)malloc(REQUEST_BUFFER_SIZE);
    if (request_buffer == NULL) {
        perror("handle_client");
        return 1;
    }
    uint8_t *buff_end = request_buffer;

    int bytes_read = 0;
    int bytes_in_buf = 0;
    while((bytes_read = read(client->socket, buff_end, REQUEST_BUFFER_SIZE - bytes_in_buf)) > 0){
        #ifdef DEBUG
        printf("Read %d bytes from client\n", bytes_read);
        #endif

        bytes_in_buf += bytes_read;

        request = find_network_newline((char *)request_buffer, &bytes_in_buf);

        if (request && strcmp(request, REQUEST_LIST) == 0) {
            if (list_request_response(client, library) < 0) {
                ERR_PRINT("Error handling LIST request\n");
                goto client_error;
            }

        } else if (request && strcmp(request, REQUEST_STREAM) == 0) {
            int num_pr_bytes = MIN(sizeof(uint32_t), (unsigned long)bytes_in_buf);
            if (stream_request_response(client, library, request_buffer, num_pr_bytes) < 0) {
                ERR_PRINT("Error handling STREAM request\n");
                goto client_error;
            }
            bytes_in_buf -= num_pr_bytes;
            memmove(request_buffer, request_buffer + num_pr_bytes, bytes_in_buf);

        } else if (request) {
            ERR_PRINT("Unknown request: %s\n", request);
        }

        free(request); request = NULL;
        buff_end = request_buffer + bytes_in_buf;

    }
    if (bytes_read < 0) {
        perror("handle_client");
        goto client_error;
    }

    printf("Client on %s:%d disconnected\n",
           inet_ntoa(client->addr.sin_addr),
           ntohs(client->addr.sin_port));

    free(request_buffer);
    if (request != NULL) {
        free(request);
    }
    return 0;
client_error:
    free(request_buffer);
    if (request != NULL) {
        free(request);
    }
    return -1;
}


static void print_usage(){
    printf("Usage: as_server [-h] [-p port] [-l library_directory]\n");
    printf("  -h  Print this message\n");
    printf("  -p  Port to listen on (default: " XSTR(DEFAULT_PORT) ")\n");
    printf("  -l  Directory containing the library (default: ./library/)\n");
}


int main(int argc, char * const *argv){
    int opt;
    int port = DEFAULT_PORT;
    const char *library_directory = "library";

    // Check out man 3 getopt for how to use this function
    // The short version: it parses command line options
    // Note that optarg is a global variable declared in getopt.h
    while ((opt = getopt(argc, argv, "hp:l:")) != -1) {
        switch (opt) {
            case 'h':
                print_usage();
                return 0;
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                library_directory = optarg;
                break;
            default:
                print_usage();
                return 1;
        }
    }

    printf("Starting server on port %d, serving library in %s\n",
           port, library_directory);

    return run_server(port, library_directory);
}
