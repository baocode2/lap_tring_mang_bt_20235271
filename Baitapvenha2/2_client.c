#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define BUFFER_SIZE 4096

int main() {
    // 1. Tạo socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Không thể tạo socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9999); // Cùng port với Server
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 2. Kết nối tới Server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Kết nối thất bại");
        exit(1);
    }

    printf("--- DA KET NOI DEN TELNET SERVER ---\n");

    // 3. Thiết lập poll
    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;  // Theo dõi bàn phím
    fds[0].events = POLLIN;

    fds[1].fd = client_fd;     // Theo dõi socket
    fds[1].events = POLLIN;

    char buffer[BUFFER_SIZE];

    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            perror("poll() lỗi");
            break;
        }

        // NHẬN DỮ LIỆU TỪ SERVER
        if (fds[1].revents & POLLIN) {
            int nbytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (nbytes <= 0) {
                printf("\nServer da dong ket noi.\n");
                break;
            }
            buffer[nbytes] = '\0';
            printf("%s", buffer);
            fflush(stdout);
        }

        // GỬI DỮ LIỆU TỪ BÀN PHÍM
        if (fds[0].revents & POLLIN) {
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                // Loại bỏ \n \r
                int len = strlen(buffer);
                while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
                    buffer[--len] = '\0';
                }
                if (len == 0) continue;

                // Kiểm tra lệnh thoát phía client
                if (strcmp(buffer, "quit") == 0) {
                    send(client_fd, "exit", 4, 0);
                    printf("Dang thoat...\n");
                    break;
                }

                send(client_fd, buffer, strlen(buffer), 0);
            }
        }
    }

    close(client_fd);
    return 0;
}
