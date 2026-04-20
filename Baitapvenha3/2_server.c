#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>

#define MAX_CLIENTS 64
#define BUFFER_SIZE 1024

// Các trạng thái của Client
#define STATE_WAIT_USER 0
#define STATE_WAIT_PASS 1
#define STATE_LOGGED_IN 2

typedef struct {
    int fd;
    int state;
    char username[64];
} ClientInfo;

// Hàm kiểm tra tài khoản từ file database.txt
int check_login(const char *user, const char *pass) {
    FILE *file = fopen("database.txt", "r");
    if (!file) {
        perror("Không thể mở file database.txt");
        return 0; 
    }

    char file_user[64], file_pass[64];
    while (fscanf(file, "%63s %63s", file_user, file_pass) != EOF) {
        if (strcmp(user, file_user) == 0 && strcmp(pass, file_pass) == 0) {
            fclose(file);
            return 1; // Đúng tài khoản
        }
    }
    fclose(file);
    return 0; // Sai tài khoản
}

// Hàm loại bỏ ký tự xuống dòng
void strip_newline(char *str) {
    str[strcspn(str, "\r\n")] = 0;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    struct pollfd fds[MAX_CLIENTS];
    ClientInfo clients[MAX_CLIENTS];
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Thêm tùy chọn SO_REUSEADDR để tránh lỗi "Address already in use" khi chạy lại server liên tục
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9000); 

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi bind");
        return 1;
    }
    listen(server_fd, 5);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
    }

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    int nfds = 1;

    printf("Telnet Server đang chạy trên cổng 9000...\n");

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        // Xử lý kết nối mới
        if (fds[0].revents & POLLIN) {
            client_fd = accept(server_fd, NULL, NULL);
            if (nfds < MAX_CLIENTS) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                clients[nfds].fd = client_fd;
                clients[nfds].state = STATE_WAIT_USER; 
                nfds++;
                
                char *msg = "Yêu cầu đăng nhập.\nUsername: ";
                send(client_fd, msg, strlen(msg), 0);
            } else {
                char *msg_full = "Server đã đầy!\n";
                send(client_fd, msg_full, strlen(msg_full), 0);
                close(client_fd);
            }
        }

        // Xử lý dữ liệu từ Client
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                int n = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);
                if (n <= 0) { 
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    clients[i] = clients[nfds - 1];
                    nfds--;
                    i--;
                    continue;
                }

                buffer[n] = '\0';
                strip_newline(buffer);

                // State Machine
                if (clients[i].state == STATE_WAIT_USER) {
                    strncpy(clients[i].username, buffer, 63);
                    clients[i].username[63] = '\0';
                    clients[i].state = STATE_WAIT_PASS;
                    
                    char *msg = "Password: ";
                    send(fds[i].fd, msg, strlen(msg), 0);

                } else if (clients[i].state == STATE_WAIT_PASS) {
                    if (check_login(clients[i].username, buffer)) {
                        clients[i].state = STATE_LOGGED_IN;
                        char *msg = "Đăng nhập thành công! Nhập lệnh: ";
                        send(fds[i].fd, msg, strlen(msg), 0);
                    } else {
                        char *msg = "Lỗi đăng nhập! Đóng kết nối.\n";
                        send(fds[i].fd, msg, strlen(msg), 0);
                        close(fds[i].fd);
                        fds[i] = fds[nfds - 1];
                        clients[i] = clients[nfds - 1];
                        nfds--;
                        i--;
                    }

                } else if (clients[i].state == STATE_LOGGED_IN) {
                    char cmd_sys[BUFFER_SIZE + 20];
                    sprintf(cmd_sys, "%s > out.txt", buffer); 
                    system(cmd_sys); 

                    FILE *out_file = fopen("out.txt", "r");
                    if (out_file) {
                        char file_buf[BUFFER_SIZE];
                        size_t bytes_read;
                        while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), out_file)) > 0) {
                            send(fds[i].fd, file_buf, bytes_read, 0);
                        }
                        fclose(out_file);
                    } else {
                        char *msg = "Lỗi khi đọc kết quả lệnh.\n";
                        send(fds[i].fd, msg, strlen(msg), 0);
                    }
                    
                    char *msg_next = "\nNhập lệnh tiếp theo: ";
                    send(fds[i].fd, msg_next, strlen(msg_next), 0);
                }
            }
        }
    }
    return 0;
}