#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <utils.h>

#define PORT 8080
#define CMD_UNMOUNT "unmount"

void read_cmd(char *buffer) {
    int err;
    if (err = get_line(buffer, BUFFER_SIZE)) {
        switch (err) {
            case READING_NO_INPUT:
                fprintf(stderr, "No input for command.\n");
                break;
            case READING_TOO_LONG:
                fprintf(stderr, "Command line is too long.\n");
                break;
            default:
                fprintf(stderr, "Wrong command format.\n");
        }
    }
}

void client_fs(struct sockaddr_in *addr, char *msg) {
    int sock;
    char buffer[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        return;
    }
    if ((connect(sock, (struct sockaddr *)addr, sizeof(*addr))) < 0) {
        fprintf(stderr, "Connection failed\n");
        return;
    }
    send(sock, msg, BUFFER_SIZE, 0);

    if (strcmp(msg, CMD_UNMOUNT) != 0) {
        recv(sock, buffer, BUFFER_SIZE, 0);

        // Getting command output.
        printf("%s", buffer);
        fflush(stdout);

        // Getting current directory for prompt.
        printf(ANSI_COLOR_BLUE);
        printf("%s", buffer + (strlen(buffer) + 1));
        printf(ANSI_COLOR_RESET);
        printf("$ ");
        fflush(stdout);
    }

    close(sock);
}

int main() {
    char msg[BUFFER_SIZE];
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // List available commands & get initial path.
    strcpy(msg, "help");
    client_fs(&addr, msg);

    // Read commands one by one.
    while (1) {
        read_cmd(msg);
        client_fs(&addr, msg);
        if (strcmp(msg, CMD_UNMOUNT) == 0) break;
    }

    return 0;
}

