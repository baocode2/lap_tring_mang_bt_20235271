#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <time.h>

#define MAX_CLIENTS 64
#define BUFFER_SIZE 1024

// Cấu trúc để quản lý thông tin client
typedef struct {
    int fd;
    char name[64];
    int registered; // 0: chưa đăng ký, 1: đã gửi đúng cú pháp client_id
} ClientInfo;

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    struct pollfd fds[MAX_CLIENTS];
    ClientInfo clients[MAX_CLIENTS];
    char buffer[BUFFER_SIZE];

    // 1. Khởi tạo Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    // 2. Khởi tạo mảng pollfd
    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        clients[i].registered = 0;
    }

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    printf("Chat Server đang chạy trên cổng 8888...\n");

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        // Kiểm tra kết nối mới
        if (fds[0].revents & POLLIN) {
            client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
            if (nfds < MAX_CLIENTS) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                clients[nfds].fd = client_fd;
                clients[nfds].registered = 0;
                nfds++;
                send(client_fd, "Vui lòng nhập tên theo cú pháp: client_id: client_name\n", 55, 0);
            } else {
                close(client_fd);
            }
        }

        // Kiểm tra dữ liệu từ các client
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                int n = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);
                if (n <= 0) { // Client ngắt kết nối
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    clients[i] = clients[nfds - 1];
                    nfds--;
                    i--;
                    continue;
                }

                buffer[n] = '\0';
                // Xử lý đăng ký tên 
                if (!clients[i].registered) {
                    char name[64];
                    if (sscanf(buffer, "client_id: %s", name) == 1) {
                        strcpy(clients[i].name, name);
                        clients[i].registered = 1;
                        send(fds[i].fd, "Đăng ký thành công!\n", 21, 0);
                    } else {
                        send(fds[i].fd, "Sai cú pháp! Nhập lại: client_id: client_name\n", 47, 0);
                    }
                } else {
                    // Gửi tin nhắn cho các client khác 
                    time_t rawtime;
                    struct tm *timeinfo;
                    char time_str[25];
                    time(&rawtime);
                    timeinfo = localtime(&rawtime);
                    strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%S%p", timeinfo);

                    char send_buffer[BUFFER_SIZE + 100];
                    sprintf(send_buffer, "%s %s: %s", time_str, clients[i].name, buffer);

                    for (int j = 1; j < nfds; j++) {
                        if (j != i && clients[j].registered) {
                            send(fds[j].fd, send_buffer, strlen(send_buffer), 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}