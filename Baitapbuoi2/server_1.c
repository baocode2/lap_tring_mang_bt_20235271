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
    
    // words[count-1] là Tên chính
    sprintf(email_out, "%s.%s%s@sis.hust.edu.vn\n", words[count-1], initials, mssv);
}

int main() {
    int master_socket, new_socket, max_sd, sd, activity;
    struct sockaddr_in address;
    fd_set readfds; // Tập hợp các socket để dùng cho select()
    Client clients[MAX_CLIENTS];
    
    // Khởi tạo mảng clients bằng 0
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].state = 0;
    }

    // 1. Tạo master socket
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 2. Bind và Listen
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

    // 3. Vòng lặp chính xử lý nhiều client
    while(1) {
        // Xóa tập socket và thêm master_socket vào
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Thêm các socket của clients vào tập readfds
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        // Đợi một hoạt động trên một trong các socket (Hàm này block cho đến khi có I/O)
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            printf("Select error");
        }

        // Xử lý kết nối mới (nếu master_socket có tín hiệu)
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }
            
            // Thêm client mới vào mảng và bắt đầu hỏi tên
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].state = 1; // Chuyển sang trạng thái đợi nhập Tên
                    char *msg = "Vui long nhap Ho Ten (Khong dau): ";
                    send(new_socket, msg, strlen(msg), 0);
                    printf("Client moi ket noi, duoc gan vao vi tri %d\n", i);
                    break;
                }
            }
        }

        // Xử lý dữ liệu từ các client đã kết nối
        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;

            if (FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, BUFFER_SIZE);
                
                if (valread == 0) {
                    // Client ngắt kết nối
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Client ngat ket noi.\n");
                    close(sd);
                    clients[i].fd = 0;
                    clients[i].state = 0;
                } else {
                    buffer[valread] = '\0'; // Kết thúc chuỗi
                    
                    if (clients[i].state == 1) {
                        // Nhận được Họ Tên, yêu cầu nhập MSSV
                        strcpy(clients[i].name, buffer);
                        clients[i].state = 2; // Chuyển sang trạng thái đợi nhập MSSV
                        char *msg = "Vui long nhap MSSV: ";
                        send(sd, msg, strlen(msg), 0);
                    } 
                    else if (clients[i].state == 2) {
                        // Nhận được MSSV, tạo email và trả về
                        char email[BUFFER_SIZE+64];
                        generate_hust_email(clients[i].name, buffer, email);
                        
                        char response[BUFFER_SIZE+64];
                        snprintf(response, sizeof(response), "Email cua ban la: %s", email);
                        send(sd, response, strlen(response), 0);
                        
                        // Đóng kết nối sau khi hoàn thành
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