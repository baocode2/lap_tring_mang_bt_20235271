#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Kiểm tra tham số đầu vào (Địa chỉ IP và Cổng của Server)
    if (argc < 3) {
        printf("Sử dụng: %s <IP Server> <Port>\n", argv[0]);
        return 1;
    }

    int client_fd;
    struct sockaddr_in server_addr;
    struct pollfd fds[2];
    char buffer[BUFFER_SIZE];

    // 1. Tạo socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    // 2. Kết nối tới server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Kết nối thất bại");
        return 1;
    }

    printf("Đã kết nối tới Server. Đang đợi yêu cầu đăng ký...\n");

    // 3. Thiết lập poll để theo dõi:
    // fds[0]: Dữ liệu từ bàn phím (Standard Input - stdin)
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // fds[1]: Dữ liệu nhận được từ Server qua socket
    fds[1].fd = client_fd;
    fds[1].events = POLLIN;

    while (1) {
        int ret = poll(fds, 2, -1);
        if (ret < 0) {
            perror("Lỗi poll");
            break;
        }

        // Trường hợp 1: Có dữ liệu từ bàn phím (Người dùng nhập tin nhắn)
        if (fds[0].revents & POLLIN) {
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                // Xóa ký tự xuống dòng ở cuối xâu
                buffer[strcspn(buffer, "\n")] = 0;
                
                // Gửi dữ liệu tới server
                send(client_fd, buffer, strlen(buffer), 0);
                
                if (strcmp(buffer, "exit") == 0) break;
            }
        }

        // Trường hợp 2: Có dữ liệu từ Server gửi về (Tin nhắn từ người khác hoặc thông báo)
        if (fds[1].revents & POLLIN) {
            int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (n <= 0) {
                printf("Server đã ngắt kết nối.\n");
                break;
            }
            buffer[n] = '\0';
            printf("%s\n", buffer); // In tin nhắn nhận được ra màn hình
        }
    }

    close(client_fd);
    return 0;
}