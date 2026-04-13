#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>

#define MAX_CLIENTS 64
#define BUFFER_SIZE 4096
#define USER_FILE "users.txt"
#define OUTPUT_FILE "out.txt"

// Trạng thái của mỗi client
typedef struct {
    int fd;
    int state;  // 0: chờ user, 1: chờ pass, 2: đã đăng nhập
    char username[50];
} Client;

// Hàm kiểm tra tài khoản trong file users.txt
// Trả về 1 nếu đúng, 0 nếu sai
int check_login(const char *user, const char *pass) {
    FILE *fp = fopen(USER_FILE, "r");
    if (!fp) {
        perror("Không thể mở file users.txt");
        return 0;
    }

    char file_user[50], file_pass[50];
    while (fscanf(fp, "%49s %49s", file_user, file_pass) == 2) {
        if (strcmp(file_user, user) == 0 && strcmp(file_pass, pass) == 0) {
            fclose(fp);
            return 1; // Tìm thấy tài khoản hợp lệ
        }
    }

    fclose(fp);
    return 0; // Không tìm thấy
}

// Hàm thực hiện lệnh và trả kết quả
// Dùng system("command > out.txt") rồi đọc file out.txt
void execute_command(int client_fd, const char *command) {
    char sys_cmd[BUFFER_SIZE];
    // Tạo lệnh: command > out.txt 2>&1 (gồm cả stderr)
    snprintf(sys_cmd, sizeof(sys_cmd), "%s > %s 2>&1", command, OUTPUT_FILE);

    int ret = system(sys_cmd);

    // Đọc kết quả từ file out.txt
    FILE *fp = fopen(OUTPUT_FILE, "r");
    if (!fp) {
        char *err = "[Server] Khong the doc ket qua lenh.\n";
        send(client_fd, err, strlen(err), 0);
        return;
    }

    char buf[BUFFER_SIZE];
    int total_sent = 0;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        send(client_fd, buf, strlen(buf), 0);
        total_sent += strlen(buf);
    }
    fclose(fp);

    // Nếu không có output nào
    if (total_sent == 0) {
        if (ret != 0) {
            char *err = "[Server] Lenh thuc thi that bai hoac khong co ket qua.\n";
            send(client_fd, err, strlen(err), 0);
        } else {
            char *ok = "[Server] Lenh thuc thi thanh cong (khong co output).\n";
            send(client_fd, ok, strlen(ok), 0);
        }
    }

    // Gửi dấu hiệu kết thúc output
    char *end_marker = "\n[END_OUTPUT]\n";
    send(client_fd, end_marker, strlen(end_marker), 0);

    // Xóa file tạm
    remove(OUTPUT_FILE);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Không thể tạo socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(9999);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind thất bại");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen thất bại");
        exit(1);
    }

    struct pollfd fds[MAX_CLIENTS + 1];
    Client clients[MAX_CLIENTS + 1];
    int num_clients = 1;

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("=== Telnet Server dang chay tai port 9999 ===\n");

    while (1) {
        int poll_count = poll(fds, num_clients, -1);
        if (poll_count < 0) {
            perror("poll() lỗi");
            break;
        }

        for (int i = 0; i < num_clients; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            // ====== CHẤP NHẬN KẾT NỐI MỚI ======
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
                    clients[num_clients].state = 0; // Chờ username
                    memset(clients[num_clients].username, 0, sizeof(clients[num_clients].username));
                    num_clients++;

                    char *msg = "=== CHAO MUNG DEN TELNET SERVER ===\nNhap username: ";
                    send(client_fd, msg, strlen(msg), 0);
                    printf("[Server] Client moi ket noi (fd=%d). Tong: %d\n", client_fd, num_clients - 1);
                } else {
                    char *full = "Server day!\n";
                    send(client_fd, full, strlen(full), 0);
                    close(client_fd);
                }
            }

            // ====== XỬ LÝ DỮ LIỆU TỪ CLIENT ======
            else {
                char buf[BUFFER_SIZE];
                int nbytes = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

                // Client ngắt kết nối
                if (nbytes <= 0) {
                    printf("[Server] Client '%s' (fd=%d) ngat ket noi.\n",
                           clients[i].state == 2 ? clients[i].username : "unknown",
                           fds[i].fd);
                    close(fds[i].fd);
                    fds[i] = fds[num_clients - 1];
                    clients[i] = clients[num_clients - 1];
                    num_clients--;
                    i--;
                    continue;
                }

                buf[nbytes] = '\0';
                // Loại bỏ \n \r
                int len = strlen(buf);
                while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
                    buf[--len] = '\0';
                }
                if (len == 0) continue;

                // --- STATE 0: CHỜ USERNAME ---
                if (clients[i].state == 0) {
                    strncpy(clients[i].username, buf, sizeof(clients[i].username) - 1);
                    clients[i].state = 1; // Chuyển sang chờ password
                    char *msg = "Nhap password: ";
                    send(fds[i].fd, msg, strlen(msg), 0);
                    printf("[Server] Client fd=%d nhap username: '%s'\n", fds[i].fd, buf);
                }

                // --- STATE 1: CHỜ PASSWORD ---
                else if (clients[i].state == 1) {
                    printf("[Server] Client fd=%d nhap password cho user '%s'\n",
                           fds[i].fd, clients[i].username);

                    if (check_login(clients[i].username, buf)) {
                        clients[i].state = 2; // Đăng nhập thành công
                        char msg[BUFFER_SIZE];
                        snprintf(msg, sizeof(msg),
                                 "Dang nhap thanh cong! Xin chao %s.\n"
                                 "Nhap lenh de thuc thi (VD: ls, pwd, whoami, date...):\n"
                                 "Nhap 'exit' de thoat.\n$ ",
                                 clients[i].username);
                        send(fds[i].fd, msg, strlen(msg), 0);
                        printf("[Server] User '%s' dang nhap thanh cong!\n", clients[i].username);
                    } else {
                        // Đăng nhập thất bại, reset về state 0
                        clients[i].state = 0;
                        memset(clients[i].username, 0, sizeof(clients[i].username));
                        char *msg = "Sai tai khoan hoac mat khau! Vui long thu lai.\nNhap username: ";
                        send(fds[i].fd, msg, strlen(msg), 0);
                        printf("[Server] Dang nhap that bai cho client fd=%d\n", fds[i].fd);
                    }
                }

                // --- STATE 2: ĐÃ ĐĂNG NHẬP → THỰC THI LỆNH ---
                else if (clients[i].state == 2) {
                    // Kiểm tra lệnh exit
                    if (strcmp(buf, "exit") == 0) {
                        char *bye = "Tam biet! Ket noi se duoc dong.\n";
                        send(fds[i].fd, bye, strlen(bye), 0);
                        printf("[Server] User '%s' thoat.\n", clients[i].username);
                        close(fds[i].fd);
                        fds[i] = fds[num_clients - 1];
                        clients[i] = clients[num_clients - 1];
                        num_clients--;
                        i--;
                        continue;
                    }

                    printf("[Server] User '%s' thuc thi lenh: '%s'\n", clients[i].username, buf);
                    execute_command(fds[i].fd, buf);

                    // Gửi prompt
                    char *prompt = "$ ";
                    send(fds[i].fd, prompt, strlen(prompt), 0);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
