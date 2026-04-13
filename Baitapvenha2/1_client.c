#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#define BUFFER_SIZE 1024

int main() {
    // 1. Khởi tạo socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Không thể tạo socket");
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888); // Cùng port với Server
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // 2. Kết nối tới Server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Kết nối thất bại");
        exit(1);
    }

    printf("--- DA KET NOI DEN CHAT SERVER ---\n");

    // 3. Thiết lập poll để theo dõi đồng thời stdin và socket
    struct pollfd fds[2];
    
    // Theo dõi bàn phím (stdin - File descriptor 0)
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    // Theo dõi socket kết nối với server
    fds[1].fd = client_fd;
    fds[1].events = POLLIN;

    char buffer[BUFFER_SIZE];

    while (1) {
        int ret = poll(fds, 2, -1); // Đợi vô thời hạn cho đến khi có sự kiện
        if (ret < 0) {
            perror("poll() lỗi");
            break;
        }

        // KIỂM TRA SỰ KIỆN TỪ SERVER (Dữ liệu đến)
        if (fds[1].revents & POLLIN) {
            int nbytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
            if (nbytes <= 0) {
                printf("\nServer đã đóng kết nối.\n");
                break;
            }
            buffer[nbytes] = '\0';
            printf("%s", buffer); // Hiển thị tin nhắn từ server/người khác
            fflush(stdout);
        }

        // KIỂM TRA SỰ KIỆN TỪ BÀN PHÍM (Người dùng nhập tin nhắn)
        if (fds[0].revents & POLLIN) {
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                // Loại bỏ ký tự xuống dòng
                int len = strlen(buffer);
                while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
                    buffer[--len] = '\0';
                }
                // Bỏ qua tin nhắn rỗng
                if (len == 0) continue;
                // Gửi dữ liệu sang server (không kèm \n)
                send(client_fd, buffer, strlen(buffer), 0);
            }
        }
    }

    close(client_fd);
    return 0;
}