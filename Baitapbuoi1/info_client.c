#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORT 8080

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Loi tao Socket \n"); return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n Dia chi khong hop le \n"); return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\n Loi ket noi \n"); return -1;
    }

    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    uint16_t dir_len = strlen(cwd);

    DIR *dir;
    struct dirent *ent;
    struct stat st;
    uint32_t file_count = 0;
    size_t total_buffer_size = 2 + dir_len + 4;
    dir = opendir(cwd);
    if (dir != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            stat(ent->d_name, &st);
            if (S_ISREG(st.st_mode)) {
                file_count++;
                total_buffer_size += 2 + strlen(ent->d_name) + 4; 
            }
        }
        rewinddir(dir); 
    }

    char *buffer = (char*)malloc(total_buffer_size);
    char *ptr = buffer;

    uint16_t dir_len_net = htons(dir_len);
    memcpy(ptr, &dir_len_net, 2); ptr += 2;
    memcpy(ptr, cwd, dir_len);    ptr += dir_len;

    uint32_t file_count_net = htonl(file_count);
    memcpy(ptr, &file_count_net, 4); ptr += 4;

    if (dir != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            stat(ent->d_name, &st);
            if (S_ISREG(st.st_mode)) {
                uint16_t name_len = strlen(ent->d_name);
                uint16_t name_len_net = htons(name_len);
                uint32_t file_size_net = htonl((uint32_t)st.st_size);

                memcpy(ptr, &name_len_net, 2); ptr += 2;
                memcpy(ptr, ent->d_name, name_len); ptr += name_len;
                memcpy(ptr, &file_size_net, 4); ptr += 4;
            }
        }
        closedir(dir);
    }

    send(sock, buffer, total_buffer_size, 0);
    printf("Da gui %zu bytes du lieu thong tin file thanh cong.\n", total_buffer_size);

    free(buffer);
    close(sock);
    return 0;
}