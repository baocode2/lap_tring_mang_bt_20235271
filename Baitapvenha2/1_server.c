#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <time.h>

#define MAX_CLIENTS 64
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    char name[50];
    int authenticated;
} Client;

// Hàm lấy thời gian hệ thống
void get_current_time(char *buffer) {
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, 25, "%Y/%m/%d %I:%M:%S%p", timeinfo);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Không thể tạo socket");
        exit(1);
    }

    // Cho phép tái sử dụng port nhanh khi restart server
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind thất bại");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen thất bại");
        exit(1);
    }

    // Mảng quản lý File Descriptors cho poll
    struct pollfd fds[MAX_CLIENTS + 1];
    // Mảng lưu thông tin Client tương ứng
    Client clients[MAX_CLIENTS + 1];
    int num_clients = 1; // Bắt đầu với server_fd

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("Server đang chạy tại port 8888...\n");

    while (1) {
        int poll_count = poll(fds, num_clients, -1);
        if (poll_count < 0) {
            perror("poll() lỗi");
            break;
        }

        for (int i = 0; i < num_clients; i++) {
            if (fds[i].revents & POLLIN) {
                
                // 1. CHẤP NHẬN KẾT NỐI MỚI
                if (fds[i].fd == server_fd) {
                    int client_fd = accept(server_fd, NULL, NULL);
                    if (client_fd < 0) {
                        perror("accept() lỗi");
                        continue;
                    }
                    if (num_clients < MAX_CLIENTS + 1) {
                        fds[num_clients].fd = client_fd;
                        fds[num_clients].events = POLLIN;
                        clients[num_clients].fd = client_fd;
                        clients[num_clients].authenticated = 0;
                        memset(clients[num_clients].name, 0, sizeof(clients[num_clients].name));
                        num_clients++;
                        char *welcome_msg = "Vui long nhap 'client_id: ten_cua_ban' de bat dau:\n";
                        send(client_fd, welcome_msg, strlen(welcome_msg), 0);
                        printf("[Server] Client mới kết nối (fd=%d). Tổng: %d\n", client_fd, num_clients - 1);
                    } else {
                        char *full_msg = "Server day! Khong the ket noi.\n";
                        send(client_fd, full_msg, strlen(full_msg), 0);
                        close(client_fd);
                    }
                } 
                
                // 2. NHẬN DỮ LIỆU TỪ CLIENT
                else {
                    char buf[BUFFER_SIZE];
                    int nbytes = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);
                    
                    if (nbytes <= 0) { // Client ngắt kết nối
                        printf("[Server] Client '%s' (fd=%d) đã ngắt kết nối.\n",
                               clients[i].authenticated ? clients[i].name : "unknown",
                               fds[i].fd);

                        // Thông báo cho các client khác
                        if (clients[i].authenticated) {
                            char leave_msg[BUFFER_SIZE];
                            snprintf(leave_msg, sizeof(leave_msg), "[Server] %s đã rời phòng chat.\n", clients[i].name);
                            for (int j = 1; j < num_clients; j++) {
                                if (j != i && clients[j].authenticated) {
                                    send(fds[j].fd, leave_msg, strlen(leave_msg), 0);
                                }
                            }
                        }

                        close(fds[i].fd);
                        // Hoán đổi với phần tử cuối cùng
                        fds[i] = fds[num_clients - 1];
                        clients[i] = clients[num_clients - 1];
                        num_clients--;
                        i--; // Kiểm tra lại vị trí i vì phần tử mới được swap vào
                        continue;
                    }

                    buf[nbytes] = '\0';
                    // Loại bỏ ký tự xuống dòng và carriage return nếu có
                    int len = strlen(buf);
                    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
                        buf[--len] = '\0';
                    }

                    // Bỏ qua tin nhắn rỗng
                    if (len == 0) continue;

                    // XỬ LÝ LOGIC XÁC THỰC
                    if (!clients[i].authenticated) {
                        char name[50];
                        if (sscanf(buf, "client_id: %49s", name) == 1) {
                            strcpy(clients[i].name, name);
                            clients[i].authenticated = 1;
                            char *ok_msg = "Xac thuc thanh cong! Ban da co the chat.\n";
                            send(fds[i].fd, ok_msg, strlen(ok_msg), 0);
                            printf("[Server] Client fd=%d xác thực thành công: '%s'\n", fds[i].fd, name);

                            // Thông báo cho các client khác
                            char join_msg[BUFFER_SIZE];
                            snprintf(join_msg, sizeof(join_msg), "[Server] %s đã tham gia phòng chat.\n", name);
                            for (int j = 1; j < num_clients; j++) {
                                if (j != i && clients[j].authenticated) {
                                    send(fds[j].fd, join_msg, strlen(join_msg), 0);
                                }
                            }
                        } else {
                            char *err_msg = "Sai cu phap! Nhap lai: 'client_id: ten_cua_ban'\n";
                            send(fds[i].fd, err_msg, strlen(err_msg), 0);
                        }
                    } 
                    
                    // XỬ LÝ LOGIC BROADCAST
                    else {
                        char time_str[25];
                        char out_msg[BUFFER_SIZE + 100];
                        get_current_time(time_str);
                        
                        snprintf(out_msg, sizeof(out_msg), "%s %s: %s\n", time_str, clients[i].name, buf);
                        printf("[Broadcast] %s", out_msg);
                        
                        for (int j = 1; j < num_clients; j++) {
                            // Gửi cho tất cả trừ chính mình và những người chưa xác thực
                            if (i != j && clients[j].authenticated) {
                                send(fds[j].fd, out_msg, strlen(out_msg), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}