#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 256

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    // 1. Tạo socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nLoi tao socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 2. Chuyển đổi địa chỉ IP
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nDia chi khong hop le hoac khong duoc ho tro \n");
        return -1;
    }

    // 3. Kết nối tới Server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nKet noi Server that bai \n");
        return -1;
    }

    // 4. Vòng lặp giao tiếp chính
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        
        int valread = read(sock, buffer, BUFFER_SIZE - 1);
        
        if (valread <= 0) {
            printf("\nServer da dong ket noi hoac co loi.\n");
            break;
        }
        
        // In thông điệp ra màn hình
        printf("%s", buffer); 
        
        if (strstr(buffer, "Email cua ban la:") != NULL) {
            break;
        }

        // Nhập dữ liệu từ bàn phím
        if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            // Xử lý nếu người dùng muốn thoát 
            if (strncmp(buffer, "exit", 4) == 0 && (buffer[4] == '\n' || buffer[4] == '\r' || buffer[4] == '\0')) {
                send(sock, buffer, strlen(buffer), 0);
                printf("Ban da chon thoat chuong trinh.\n");
                break;
            }

            // Gửi dữ liệu vừa nhập lên Server
            send(sock, buffer, strlen(buffer), 0);
        }
    }

    close(sock);
    return 0;
}