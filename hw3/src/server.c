#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <fs.h>

#define PORT 8080
#define SERVER_BUFFER_SIZE 1024

// pwd
// ls
// mkdir DIRECTORY
// rmdir DIRECTORY
// cd DIRECTORY
// touch FILE
// rm FILE
// cat FILE
// upload FILE_LOCAL FILE_FS
// download FILE_FS FILE_LOCAL
// unmount
// help

// SuperBlock
// Blocks Bitmap Area
// inodes Bitmap Area
// inodes table
// blocks

// # SuperBlock Size = 8 Bytes
//      4 Bytes (unsigned int) - Magic Number
//      4 Bytes (unsigned int) - Block Size
// # Block Size = 1 KB
// # Block Pointer = 4 Bytes (unsigned int (2^32))
// # inode Size = 64 Bytes:
//      4*1  Bytes - file type
//      4*1  Bytes - file size
//      4*11 Bytes - blocks pointers
//      4*3  Bytes - indirect addressing
// # inode Pointer = 2 Bytes (unsigned short (2^16))

// Blocks Bitmap Area Size = 2^32 Bits = 512 MB
// inodes Bitmap Area Size = 2^16 Bits = 8 KB
// inode Area Size = 2^16 * 64 Bytes = 4 MB
// max FS size = 2^32 KB = 4096 GB
// max Blocks/File = 12+(1+256)+(1+256+256^2)+(1+256+256^2+256^3) = 16 909 071
// max File Size = 16 843 020 Blocks = 16 843 020 KB ~ 16 GB

static void daemonize_fs(int fd) {
    pid_t pid;

    pid = fork();                       // Fork off the parent process
    if (pid < 0) exit(EXIT_FAILURE);    // An error occurred
    if (pid > 0) exit(EXIT_SUCCESS);    // Success: Let the parent terminate

    /* On success: The child process becomes session leader */
    if (setsid() < 0) exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();                       // Fork off for the second time
    if (pid < 0) exit(EXIT_FAILURE);    // An error occurred
    if (pid > 0) exit(EXIT_SUCCESS);    // Success: Let the parent terminate

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--) {
        if (x != fd) close(x);
    }

    /* Open the log file */
    openlog("fs_virtual", LOG_PID, LOG_DAEMON);
}

FILE* get_fs_file(char *file_path) {
    FILE *file;
    if (access(file_path, F_OK) != -1) {
        // Filesystem file already exists.
        printf("Loading filesystem...");
        fflush(stdout);
        file = open_fs_file(file_path);
        printf(" OK!\n");
    } else {
        // Filesystem file doesn't exist, create a new one.
        printf("Generating a new filesystem...");
        fflush(stdout);
        file = generate_fs_file(file_path);
        printf(" OK!\n");
    }
    return file;
}

void server_fs(FILE* fs) {
    inode_pointer_t inode_cur_dir = 0;
    int listener, sock, bytes_read;
    struct sockaddr_in address; 
    int opt = 1;
    char cmd[BUFFER_SIZE] = {0}; 
    char buffer[BUFFER_SIZE] = {0}; 
    char *hello = "Hello from server";

    // Creating socket file descriptor 
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
        syslog(LOG_ERR, "Socket creation failed!");
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    }

    // Forcefully attaching socket to the port 8080 
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, 
                                                  &opt, sizeof(opt))) { 
        syslog(LOG_ERR, "setsockopt failed!");
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    } 
    address.sin_family = AF_INET; 
    address.sin_port = htons(PORT); 
    address.sin_addr.s_addr = htonl(INADDR_ANY); 

    // Forcefully attaching socket to the port 8080 
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) < 0) { 
        syslog(LOG_ERR, "bind failed");
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    if (listen(listener, 1) < 0) { 
        syslog(LOG_ERR, "listen failed");
        perror("listen failed");
        exit(EXIT_FAILURE); 
    }

    syslog(LOG_NOTICE, "Virtual FS started.");

    while (1) {
        if ((sock = accept(listener, NULL, NULL)) < 0) {
            syslog(LOG_ERR, "accept failed");
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        if ((bytes_read = recv(sock, cmd, BUFFER_SIZE, 0)) >= 0) {
            // Process input command and get its output.
            cmd[bytes_read] = '\0';
            if (get_cmd(fs, &inode_cur_dir, cmd, buffer))
                break;

            // Append current directory to command output.
            buffer[strlen(buffer) + 1] = '\0';
            cmd_pwd(fs, inode_cur_dir, buffer + (strlen(buffer) + 1), 0);

            send(sock, buffer, BUFFER_SIZE, 0);
        }

        close(sock);
    }
}

int main(int argc, char *argv[]) {
    FILE *fs;
    int fd;

    if (argc != 2) {
        fprintf(stderr, "Expected single argument, got %d.\n", argc - 1);
        return EXIT_FAILURE;
    }
    fs = get_fs_file(argv[1]);

    fd = fileno(fs);
    daemonize_fs(fd);

    server_fs(fs);

    fclose(fs);
    syslog(LOG_NOTICE, "Virtual FS terminated.");
    closelog();

    return EXIT_SUCCESS;
}

