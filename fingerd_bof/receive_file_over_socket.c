#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 4444
#define IO_BUF_SIZE 2048

int send_int(int num, int fd) {
    int32_t conv = htonl(num);
    char *data = (char*)&conv;
    int left = sizeof(conv);
    int rc;
    do {
        rc = write(fd, data, left);
        if (rc < 0) {
			return -1;
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    return 0;
}

int receive_int(int *num, int fd)
{
    int32_t ret;
    char *data = (char*)&ret;
    int left = sizeof(ret);
    int rc;
    do {
        rc = read(fd, data, left);
        if (rc <= 0) { /* instead of ret */
			return -1;
        }
        else {
            data += rc;
            left -= rc;
        }
    }
    while (left > 0);
    *num = ntohl(ret);
    return 0;
}

int main() {
    int server_sock = 0;
    struct sockaddr_in serv_addr;
	char io_buf[IO_BUF_SIZE];
	char *hello = "Hello from client!\n";

    server_sock = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("Failed to connect to server!\n");
		return 1;
	}	

	fd_set read_fd;

	int file_size = 0;
	while (1) {
		send(server_sock, hello, strlen(hello), 0);
		printf("waiting for server...\n");
		FD_ZERO(&read_fd);
		FD_SET(server_sock, &read_fd);

		struct timeval tv = {5, 0};
			
		int activity = select(server_sock + 1, &read_fd, NULL, NULL, &tv);
		if (activity < 0) continue;
		printf("%d\n", activity);

		if (FD_ISSET(server_sock, &read_fd)) {
			int num_files;
			if (receive_int(&num_files, server_sock) < 0) continue;	
			if (send_int(num_files, server_sock) < 0) continue;
			printf("number of files expecting: %d\n", num_files);

			int n;
			for (int i = 0; i < num_files; i++) {
				int len;
				if (receive_int(&len, server_sock) < 0) continue;	

				char *file_name;
				n = read(server_sock, file_name, len);
				if (n <= 0) break;

				FILE *outfile = fopen(file_name, "w"); 
				if (outfile == NULL) break;

				int file_size;
				if (receive_int(&file_size, server_sock) < 0) continue; 	
				printf("%d\n", file_size);

				while (file_size > 0) {
					n = read(server_sock, io_buf, IO_BUF_SIZE -1); 
					if (n > 0) {
						io_buf[n] = '\0'; 
						fputs(io_buf, outfile);
					} 
					file_size -= n;
				}	
				fclose(outfile);
				printf("recieved %dth file\n", i);
				send_int(i, server_sock);
			}	

			// i need to have proper checking of whether correct number of files have been successfully downloaded
			char *msg = "Files recieved\n";
			send(server_sock, msg, strlen(msg), 0); 
			break;
		}	
	}	

    close(server_sock);
    return 0;
}
