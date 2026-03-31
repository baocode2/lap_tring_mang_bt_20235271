#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void recv_exact(int sock, void *buf, size_t len) {
    size_t total_received = 0;
    while (total_received < len) {
        ssize_t r = recv(sock, (char*)buf + total_received, len - total_received, 0);
        if (r <= 0) {
            perror("Loi hoac mat ket noi khi nhan du lieu");
            exit(1);
        }
        total_received += r;
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed"); exit(EXIT_FAILURE);
    }
    
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    printf("Server dang lang nghe tren cong %d...\n", PORT);

    while(1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        uint16_t dir_len_net;
        recv_exact(new_socket, &dir_len_net, 2);
        uint16_t dir_len = ntohs(dir_len_net);

        char dir_name[1024] = {0};_
        recv_exact(new_socket, dir_name, dir_len);
        printf("%s\n", dir_name);

        uint32_t file_count_net;
        recv_exact(new_socket, &file_count_net, 4);
        uint32_t file_count = ntohl(file_count_net);

        for (uint32_t i = 0; i < file_count; i++) {
            uint16_t name_len_net;
            recv_exact(new_socket, &name_len_net, 2);
            uint16_t name_len = ntohs(name_len_net);

            char file_name[256] = {0};
            recv_exact(new_socket, file_name, name_len);

            uint32_t file_size_net;
            recv_exact(new_socket, &file_size_net, 4);
            uint32_t file_size = ntohl(file_size_net);

            printf("%s - %u bytes\n", file_name, file_size);
        }
        printf("----------------------------------\n");
        close(new_socket);
    }
    return 0;
}