#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 10
#define BUF_SIZE 1024

void remove_newline(char *s) {
    int len = strlen(s);

    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

void signal_handler(int signo) {
    (void)signo;

    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void send_message(int client, const char *message) {
    send(client, message, strlen(message), 0);
}

int get_strftime_format(const char *client_format, char *strftime_format, int size) {
    if (strcmp(client_format, "dd/mm/yyyy") == 0) {
        snprintf(strftime_format, size, "%%d/%%m/%%Y");
        return 1;
    }

    if (strcmp(client_format, "dd/mm/yy") == 0) {
        snprintf(strftime_format, size, "%%d/%%m/%%y");
        return 1;
    }

    if (strcmp(client_format, "mm/dd/yyyy") == 0) {
        snprintf(strftime_format, size, "%%m/%%d/%%Y");
        return 1;
    }

    if (strcmp(client_format, "mm/dd/yy") == 0) {
        snprintf(strftime_format, size, "%%m/%%d/%%y");
        return 1;
    }

    return 0;
}

void handle_client(int client) {
    char buffer[BUF_SIZE];

    send_message(client, "Time server is ready.\n");
    send_message(client, "Command: GET_TIME [format]\n");
    send_message(client, "Formats: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy\n");
    send_message(client, "Example: GET_TIME dd/mm/yyyy\n\n");

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        int ret = recv(client, buffer, sizeof(buffer) - 1, 0);

        if (ret <= 0) {
            printf("Client disconnected in process %d\n", getpid());
            break;
        }

        buffer[ret] = '\0';
        remove_newline(buffer);

        printf("Process %d received: %s\n", getpid(), buffer);

        if (strcmp(buffer, "exit") == 0 || strcmp(buffer, "quit") == 0) {
            send_message(client, "Goodbye.\n");
            break;
        }

        char command[64];
        char format[64];
        char extra[64];

        int count = sscanf(buffer, "%63s %63s %63s", command, format, extra);

        if (count != 2) {
            send_message(client, "ERROR: Invalid command syntax. Use: GET_TIME [format]\n");
            continue;
        }

        if (strcmp(command, "GET_TIME") != 0) {
            send_message(client, "ERROR: Invalid command. Command must be GET_TIME\n");
            continue;
        }

        char strftime_format[64];

        if (!get_strftime_format(format, strftime_format, sizeof(strftime_format))) {
            send_message(client, "ERROR: Unsupported format\n");
            send_message(client, "Supported formats: dd/mm/yyyy, dd/mm/yy, mm/dd/yyyy, mm/dd/yy\n");
            continue;
        }

        time_t now = time(NULL);
        struct tm *time_info = localtime(&now);

        char result[128];
        strftime(result, sizeof(result), strftime_format, time_info);

        strcat(result, "\n");
        send_message(client, result);
    }

    close(client);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    signal(SIGCHLD, signal_handler);

    int listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(listener);
        return 1;
    }

    if (listen(listener, BACKLOG) < 0) {
        perror("listen");
        close(listener);
        return 1;
    }

    printf("Time server is running on port %d...\n", port);
    printf("Main process PID: %d\n", getpid());

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);

        if (client < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        printf("New client from %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            close(client);
            continue;
        }

        if (pid == 0) {
            close(listener);
            printf("Child process %d handles client\n", getpid());
            handle_client(client);
            exit(0);
        }

        close(client);
    }

    close(listener);
    return 0;
}