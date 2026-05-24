#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 10
#define BUF_SIZE 1024
#define USER_FILE "user.txt"

void reap_child(int signo) {
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int send_all(int sock, const char *msg) {
    int len = strlen(msg);
    int sent = 0;

    while (sent < len) {
        int n = send(sock, msg + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += n;
    }

    return 0;
}

int recv_line(int sock, char *buf, int size) {
    int i = 0;
    char c;

    while (i < size - 1) {
        int n = recv(sock, &c, 1, 0);

        if (n <= 0) {
            return n;
        }

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            break;
        }

        buf[i++] = c;
    }

    buf[i] = '\0';
    return i;
}

int check_login(const char *username, const char *password) {
    FILE *f = fopen(USER_FILE, "r");
    if (f == NULL) {
        perror("fopen user.txt");
        return 0;
    }

    char file_user[128];
    char file_pass[128];

    while (fscanf(f, "%127s %127s", file_user, file_pass) == 2) {
        if (strcmp(username, file_user) == 0 &&
            strcmp(password, file_pass) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

void handle_client(int client) {
    char username[128];
    char password[128];
    char command[BUF_SIZE];

    send_all(client, "Username: ");
    if (recv_line(client, username, sizeof(username)) <= 0) {
        close(client);
        return;
    }

    send_all(client, "Password: ");
    if (recv_line(client, password, sizeof(password)) <= 0) {
        close(client);
        return;
    }

    if (!check_login(username, password)) {
        send_all(client, "Login failed.\n");
        close(client);
        return;
    }

    send_all(client, "Login successful.\n");
    send_all(client, "Type command to execute, or type exit to quit.\n");

    while (1) {
        send_all(client, "telnet_server> ");

        int ret = recv_line(client, command, sizeof(command));
        if (ret <= 0) {
            break;
        }

        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            send_all(client, "Goodbye.\n");
            break;
        }

        if (strlen(command) == 0) {
            continue;
        }

        char output_file[256];
        snprintf(output_file, sizeof(output_file), "/tmp/telnet_output_%d.txt", getpid());

        char system_cmd[BUF_SIZE + 300];
        snprintf(system_cmd, sizeof(system_cmd), "%s > %s 2>&1", command, output_file);

        int status = system(system_cmd);

        FILE *f = fopen(output_file, "r");
        if (f == NULL) {
            send_all(client, "Cannot read command output.\n");
            continue;
        }

        char line[BUF_SIZE];
        int has_output = 0;

        while (fgets(line, sizeof(line), f) != NULL) {
            has_output = 1;
            send_all(client, line);
        }

        fclose(f);
        remove(output_file);

        if (!has_output) {
            send_all(client, "[No output]\n");
        }

        if (status != 0) {
            send_all(client, "[Command finished with error]\n");
        }
    }

    close(client);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    signal(SIGCHLD, reap_child);

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

    printf("Telnet multiprocessing server is running on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);

        if (client < 0) {
            if (errno == EINTR) continue;
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
            handle_client(client);
            exit(0);
        }

        close(client);
    }

    close(listener);
    return 0;
}