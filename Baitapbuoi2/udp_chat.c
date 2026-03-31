#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Kiểm tra số lượng tham số đầu vào theo đúng yêu cầu đề bài
    if (argc != 4) {
        printf("Sai cu phap!\n");
        printf("Cach su dung: %s <port_s> <ip_d> <port_d>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Lấy thông tin từ tham số dòng lệnh
    int port_s = atoi(argv[1]);      // Cổng nhận dữ liệu (Server mode)
    char *ip_d = argv[2];            // IP đích cần gửi tới (Client mode)
    int port_d = atoi(argv[3]);      // Cổng đích cần gửi tới (Client mode)

    int sockfd;
    struct sockaddr_in my_addr, dest_addr;
    char buffer[BUFFER_SIZE];

    // 1. Tạo UDP socket (SOCK_DGRAM)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Loi tao socket");
        exit(EXIT_FAILURE);
    }

    // 2. Cấu hình địa chỉ của bản thân (Đóng vai trò như Server nhận dữ liệu)
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(port_s); // Lắng nghe trên port_s

    if (bind(sockfd, (const struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("Loi bind socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 3. Cấu hình địa chỉ của đối tác (Đóng vai trò như Client gửi dữ liệu)
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_d); // Gửi tới port_d
    if (inet_pton(AF_INET, ip_d, &dest_addr.sin_addr) <= 0) {
        printf("Dia chi IP khong hop le.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("--- Ung dung UDP Chat khoi dong ---\n");
    printf("- Dang lang nghe tren cong: %d\n", port_s);
    printf("- San sang gui tin nhan den: %s:%d\n", ip_d, port_d);
    printf("Go 'exit' de thoat.\n\n");
    printf("Ban: ");
    fflush(stdout);

    fd_set readfds;
    int max_fd;

    // 4. Vòng lặp chính sử dụng select() để tạo cơ chế Non-blocking
    while (1) {
        FD_ZERO(&readfds);
        
        // Theo dõi luồng nhập từ bàn phím (Standard Input - File Descriptor là 0)
        FD_SET(STDIN_FILENO, &readfds);
        
        // Theo dõi luồng nhận dữ liệu từ mạng (UDP Socket)
        FD_SET(sockfd, &readfds);

        // Tìm FD lớn nhất để truyền vào select
        max_fd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

        // select() sẽ chờ (block) cho đến khi 1 trong 2 luồng có dữ liệu
        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Loi select");
            break;
        }

        // --- TRƯỜNG HỢP 1: CÓ DỮ LIỆU TỪ MẠNG GỬI ĐẾN (Nhận tin nhắn) ---
        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);
            
            memset(buffer, 0, BUFFER_SIZE);
            int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&sender_addr, &sender_len);
            
            if (n > 0) {
                buffer[n] = '\0';
                
                // Xóa chữ "Ban: " đang chờ nhập hiện tại để in tin nhắn đến cho đẹp
                printf("\rDoi phuong: %s", buffer);
                
                // In lại chữ "Ban: " để sẵn sàng cho lần nhập tiếp theo
                printf("Ban: ");
                fflush(stdout);
            }
        }

        // --- TRƯỜNG HỢP 2: CÓ DỮ LIỆU NHẬP TỪ BÀN PHÍM (Gửi tin nhắn) ---
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
                
                // Thoát chương trình nếu gõ "exit"
                if (strncmp(buffer, "exit", 4) == 0 && (buffer[4] == '\n' || buffer[4] == '\r' || buffer[4] == '\0')) {
                    printf("Da thoat chat.\n");
                    break;
                }

                // Gửi dữ liệu đi tới ip_d và port_d
                sendto(sockfd, (const char *)buffer, strlen(buffer), 0, (const struct sockaddr *)&dest_addr, sizeof(dest_addr));
                
                // In lại chữ "Ban: " cho dòng mới
                printf("Ban: ");
                fflush(stdout);
            }
        }
    }

    close(sockfd);
    return 0;
}