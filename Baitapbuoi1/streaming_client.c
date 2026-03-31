#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_ADDR "127.0.0.1"
#define PORT 8080
#define BUFF_SIZE 2048

int main() {
    int client_sock;
    struct sockaddr_in server_addr;
    char buff[BUFF_SIZE];

    client_sock = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Connection failed\n");
        return 0;
    }
    printf("Connected to server. Nhập 'exit' để thoát:\n");

    while(1) {
        fgets(buff, BUFF_SIZE, stdin);
        
        // Loại bỏ ký tự newline do fgets tạo ra (nếu không muốn gửi kèm)
        buff[strcspn(buff, "\n")] = 0;

        if (strcmp(buff, "exit") == 0) break;

        // Gửi dữ liệu (không bao gồm ký tự null terminator)
        send(client_sock, buff, strlen(buff), 0);
        printf("-> Đã gửi %lu bytes.\n", strlen(buff));
    }

    close(client_sock);
    return 0;
}