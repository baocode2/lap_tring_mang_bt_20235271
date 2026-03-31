#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include<arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080
#define BUFF_SIZE 1024

int main() {
    int listen_sock, conn_sock;
    struct sockaddr_in server, client;
    char buff[BUFF_SIZE];
    int bytes_received;
    socklen_t sin_size;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(listen_sock, (struct sockaddr *)&server, sizeof(server));
    listen(listen_sock, 5);

    printf("Server is listening on port %d...\n", PORT);

    while(1) {
        sin_size = sizeof(struct sockaddr_in);
        conn_sock = accept(listen_sock, (struct sockaddr *)&client, &sin_size);
        printf("Client connected.\n");

        int total_occurrences = 0;
        int match_idx = 0; 
        char target[] = "0123456789";

        while(1) {
            bytes_received = recv(conn_sock, buff, BUFF_SIZE, 0);
            if (bytes_received <= 0) {
                printf("Client disconnected.\n");
                break;
            }

            // Xử lý dữ liệu nhận được từng ký tự một
            for (int i = 0; i < bytes_received; i++) {
                if (buff[i] == target[match_idx]) {
                    match_idx++; // Tăng chỉ số lên
                    
                    if (match_idx == 10) {
                        total_occurrences++;
                        match_idx = 0; 
                    }
                } else {
                    if (buff[i] == '0') {
                        match_idx = 1;
                    } else {
                        match_idx = 0; // Reset hoàn toàn
                    }
                }
            }

            // In ra số lần xuất hiện sau mỗi lần nhận dữ liệu
            printf("Đã nhận thêm dữ liệu. Tổng số lần xuất hiện hiện tại: %d\n", total_occurrences);
        }
        close(conn_sock);
    }
    close(listen_sock);
    return 0;
}