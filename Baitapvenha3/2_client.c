#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Sử dụng: %s <IP Server> <Port>\n", argv[0]);
        return 1;
    }

    int client_fd;
    struct sockaddr_in server_addr;
    struct pollfd fds[2];
    char buffer[BUFFER_SIZE];

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Lỗi kết nối");
        return 1;
    }

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = client_fd;
    fds[1].events = POLLIN;

    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) break;

        // Bàn phím có dữ liệu
        if (fds[0].revents & POLLIN) {
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0; 
                send(client_fd, buffer, strlen(buffer), 0);
                if (strcmp(buffer, "exit") == 0) break;
            }
        }

        // Server gửi dữ liệu về
        if (fds[1].revents & POLLIN) {
            int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) {
                printf("\nĐã ngắt kết nối với Server.\n");
                break;
            }
            buffer[n] = '\0';
            printf("%s", buffer); 
            fflush(stdout); 
        }
    }

    close(client_fd);
    return 0;
}