#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <ctype.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Trạng thái của từng Client
typedef struct {
    int fd;
    int state; // 0: Trống, 1: Đợi nhập Tên, 2: Đợi nhập MSSV
    char name[BUFFER_SIZE];
} Client;

// Hàm tạo email theo chuẩn ĐHBK
void generate_hust_email(char *fullname, char *mssv, char *email_out) {
    char name_copy[BUFFER_SIZE];
    strcpy(name_copy, fullname);
    
    // Xóa ký tự xuống dòng
    name_copy[strcspn(name_copy, "\r\n")] = 0;
    mssv[strcspn(mssv, "\r\n")] = 0;

    // Chuyển tất cả về chữ thường
    for(int i = 0; name_copy[i]; i++) name_copy[i] = tolower(name_copy[i]);
    
    char *words[20];
    int count = 0;
    char *token = strtok(name_copy, " ");
    
    // Tách các từ trong tên
    while(token != NULL) {
        words[count++] = token;
        token = strtok(NULL, " ");
    }
    
    if (count == 0) {
        strcpy(email_out, "Loi_dinh_dang_ten");
        return;
    }
    
    char initials[20] = "";
    // Lấy chữ cái đầu của Họ và Tên đệm
    for (int i = 0; i < count - 1; i++) {
        int len = strlen(initials);
        initials[len] = words[i][0];
        initials[len+1] = '\0';
    }
    
    // Sử dụng snprintf ở đây để an toàn tuyệt đối
    snprintf(email_out, BUFFER_SIZE, "%s.%s%s@sis.hust.edu.vn\n", words[count-1], initials, mssv);
}

int main() {
    int master_socket, new_socket, max_sd, sd, activity;
    struct sockaddr_in address;
    fd_set readfds; 
    Client clients[MAX_CLIENTS];
    
    // Khởi tạo mảng clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].state = 0;
    }

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(master_socket, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Server dang lang nghe tren cong %d (Non-blocking multiplexing)\n", PORT);

    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            printf("Select error\n");
        }

        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }
            
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].state = 1; 
                    char *msg = "Vui long nhap Ho Ten (Khong dau): ";
                    send(new_socket, msg, strlen(msg), 0);
                    printf("Client moi ket noi, duoc gan vao vi tri %d\n", i);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;

            if (FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, BUFFER_SIZE - 1);
                
                if (valread == 0) {
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Client ngat ket noi.\n");
                    close(sd);
                    clients[i].fd = 0;
                    clients[i].state = 0;
                } else {
                    buffer[valread] = '\0'; 
                    
                    if (clients[i].state == 1) {
                        strcpy(clients[i].name, buffer);
                        clients[i].state = 2; 
                        char *msg = "Vui long nhap MSSV: ";
                        send(sd, msg, strlen(msg), 0);
                    } 
                    else if (clients[i].state == 2) {
                        // SỬA LẠI CHUẨN KÍCH THƯỚC Ở ĐÂY
                        char email[BUFFER_SIZE];
                        generate_hust_email(clients[i].name, buffer, email);
                        
                        char response[BUFFER_SIZE + 64]; // Đảm bảo luôn to hơn email
                        snprintf(response, sizeof(response), "Email cua ban la: %s", email);
                        send(sd, response, strlen(response), 0);
                        
                        close(sd);
                        clients[i].fd = 0;
                        clients[i].state = 0;
                    }
                }
            }
        }
    }
    return 0;
}