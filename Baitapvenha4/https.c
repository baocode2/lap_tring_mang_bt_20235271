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

#define BACKLOG 20
#define BUF_SIZE 4096
#define WORKER_COUNT 4

void handle_client(int client) {
    char buf[BUF_SIZE];

    int ret = recv(client, buf, sizeof(buf) - 1, 0);
    if (ret <= 0) {
        close(client);
        return;
    }

    buf[ret] = '\0';

    printf("Process %d received request:\n%s\n", getpid(), buf);

    char body[1024];

    snprintf(body, sizeof(body),
        "<html>"
        "<head><title>Preforking HTTP Server</title></head>"
        "<body>"
        "<h1>Xin chao cac ban</h1>"
        "<p>This response is handled by process PID: %d</p>"
        "<p>Server architecture: preforking</p>"
        "</body>"
        "</html>",
        getpid()
    );

    char response[2048];

    snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: %lu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        strlen(body),
        body
    );

    send(client, response, strlen(response), 0);
    close(client);
}

void worker_loop(int listener) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client = accept(listener, (struct sockaddr *)&client_addr, &client_len);

        if (client < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        printf("Worker PID %d accepted client from %s:%d\n",
               getpid(),
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        handle_client(client);
    }
}

void reap_child(int signo) {
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int create_listener(int port) {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        exit(1);
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
        exit(1);
    }

    if (listen(listener, BACKLOG) < 0) {
        perror("listen");
        close(listener);
        exit(1);
    }

    return listener;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    signal(SIGCHLD, reap_child);

    int listener = create_listener(port);

    printf("HTTP preforking server is running on port %d...\n", port);
    printf("Creating %d worker processes...\n", WORKER_COUNT);

    for (int i = 0; i < WORKER_COUNT; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            printf("Worker process started. PID = %d\n", getpid());
            worker_loop(listener);
            close(listener);
            exit(0);
        }
    }

    while (1) {
        pause();
    }

    close(listener);
    return 0;
}