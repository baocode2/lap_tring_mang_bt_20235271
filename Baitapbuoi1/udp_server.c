#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFF_SIZE 1024

int main() {
    int server_sock;
    struct sockaddr_in server_addr, client_addr;
    char buff[BUFF_SIZE];
    socklen_t client_addr_len = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    printf("UDP Echo Server dang chay tren port %d...\n", PORT);

    
    while(1) {
        int bytes_recv = recvfrom(server_sock, buff, BUFF_SIZE - 1, 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if (bytes_recv > 0) {
            buff[bytes_recv] = '\0'; 
            printf("Nhan duoc tu client: %s\n", buff);
    
            sendto(server_sock, buff, bytes_recv, 0, 
                  (struct sockaddr*)&client_addr, client_addr_len);
        }
    }
    
    close(server_sock);
    return 0;
}