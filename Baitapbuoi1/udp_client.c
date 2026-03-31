#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFF_SIZE 1024

int main() {
    int client_sock;
    struct sockaddr_in server_addr;
    char buff[BUFF_SIZE];
    socklen_t server_addr_len = sizeof(server_addr);

   
    client_sock = socket(AF_INET, SOCK_DGRAM, 0);

   
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

   

    while(1) {
        printf("Nhap tin nhan (hoac 'exit' de thoat): ");
        fgets(buff, BUFF_SIZE, stdin);
        
        buff[strcspn(buff, "\n")] = 0; 

        if (strcmp(buff, "exit") == 0) break;

        sendto(client_sock, buff, strlen(buff), 0, 
              (struct sockaddr*)&server_addr, server_addr_len);
     
        int bytes_recv = recvfrom(client_sock, buff, BUFF_SIZE - 1, 0, NULL, NULL);
        if (bytes_recv > 0) {
            buff[bytes_recv] = '\0';
            printf("Server tra ve: %s\n\n", buff);
        }
    }
    
    close(client_sock);
    return 0;
}