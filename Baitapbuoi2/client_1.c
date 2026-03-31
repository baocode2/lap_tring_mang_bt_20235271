#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\nLoi tao socket \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\nDia chi khong hop le hoac khong duoc ho tro \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nKet noi Server that bai \n");
        return -1;
    }

    // Vòng lặp nhận yêu cầu và gửi dữ liệu
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE);
        
        if (valread <= 0) {
            printf("Server da dong ket noi hoac co loi.\n");
            break;
        }
        
        printf("%s", buffer); // In ra câu hỏi từ Server (Ho tên / MSSV / Kết quả)
        
        // Nếu Server trả về email thì kết thúc
        if (strstr(buffer, "Email cua ban la:") != NULL) {
            break;
        }

        // Nhập dữ liệu từ bàn phím và gửi đi
        fgets(buffer, BUFFER_SIZE, stdin);
        send(sock, buffer, strlen(buffer), 0);
    }

    close(sock);
    return 0;
}