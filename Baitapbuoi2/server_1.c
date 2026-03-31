#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ctype.h>

#define PORT 8080
#define MAX_CLIENTS 64
#define BUFFER_SIZE 256

typedef struct {
    int fd;                 
    int state;              // 0: Trống, 1: Chờ nhập Tên, 2: Chờ nhập MSSV
    char name[BUFFER_SIZE]; // Lưu trữ Họ Tên để ghép với MSSV sau
} ClientState;


void generate_hust_email(char *fullname, char *mssv, char *email_out) {
    char name_copy[BUFFER_SIZE];
    strncpy(name_copy, fullname, BUFFER_SIZE - 1);
    name_copy[BUFFER_SIZE - 1] = '\0';
    
    // Xóa ký tự xuống dòng (\n, \r)
    name_copy[strcspn(name_copy, "\r\n")] = 0;
    mssv[strcspn(mssv, "\r\n")] = 0;

    // Chuyển toàn bộ chuỗi sang in thường
    for(int i = 0; name_copy[i]; i++) name_copy[i] = tolower(name_copy[i]);
    
    char *words[20];
    int count = 0;
    char *token = strtok(name_copy, " ");
    
    while(token != NULL && count < 20) {
        words[count++] = token;
        token = strtok(NULL, " ");
    }
    
    if (count == 0) {
        strcpy(email_out, "Loi_dinh_dang\n");
        return;
    }
    
    char initials[20] = "";
    for (int i = 0; i < count - 1; i++) {
        int len = strlen(initials);
        initials[len] = words[i][0];
        initials[len+1] = '\0';
    }
    
    snprintf(email_out, BUFFER_SIZE, "%s.%s%s@sis.hust.edu.vn\n", words[count-1], initials, mssv);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server non-blocking dang chay tren cong %d...\n", PORT);

    ClientState clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].state = 0;
    }

    char buf[BUFFER_SIZE];
    int len;

    while (1) {
        
        // 1. KIỂM TRA KẾT NỐI MỚI (ACCEPT)
        int client_fd = accept(listener, NULL, NULL);
        if (client_fd != -1) {
            // Chuyển client socket mới sang Non-blocking
            ul = 1;
            ioctl(client_fd, FIONBIO, &ul);
            
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    clients[i].fd = client_fd;
                    clients[i].state = 1; // State 1: Bắt đầu hỏi Tên
                    
                    printf("Client moi ket noi: FD %d\n", client_fd);
                    char *msg = "Vui long nhap Ho ten (khong dau): ";
                    send(client_fd, msg, strlen(msg), 0);
                    break;
                }
            }
        } 

        // 2. KIỂM TRA DỮ LIỆU TỪ CÁC CLIENT ĐANG KẾT NỐI (RECV)
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                len = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
                
                if (len > 0) {
                    buf[len] = 0; 
                    
                    // Xử lý lệnh exit để thoát ngang
                    if (strncmp(buf, "exit", 4) == 0) {
                        printf("Client FD %d chu dong thoat.\n", clients[i].fd);
                        close(clients[i].fd);
                        clients[i].fd = -1;
                        clients[i].state = 0;
                        continue;
                    }

                    // Xử lý theo trạng thái State Machine
                    if (clients[i].state == 1) {
                        strncpy(clients[i].name, buf, sizeof(clients[i].name) - 1);
                        clients[i].state = 2; // Chuyển sang State 2: Hỏi MSSV
                        
                        char *msg = "Vui long nhap MSSV: ";
                        send(clients[i].fd, msg, strlen(msg), 0);
                    } 
                    else if (clients[i].state == 2) {
                        char email[BUFFER_SIZE];
                        generate_hust_email(clients[i].name, buf, email);
                        
                        char response[BUFFER_SIZE + 64];
                        snprintf(response, sizeof(response), "Email cua ban la: %s", email);
                        
                        send(clients[i].fd, response, strlen(response), 0);
                        printf("Da xu ly xong va dong ket noi Client FD %d\n", clients[i].fd);
                        
                        close(clients[i].fd);
                        clients[i].fd = -1;
                        clients[i].state = 0;
                    }
                } 
                else if (len == 0) {
                    printf("Client FD %d ngat ket noi\n", clients[i].fd);
                    close(clients[i].fd);
                    clients[i].fd = -1;
                    clients[i].state = 0;
                } 
                else { 
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        close(clients[i].fd);
                        clients[i].fd = -1;
                        clients[i].state = 0;
                    }
                }
            }
        }
        
        usleep(10000); 
    }

    close(listener);
    return 0;
}