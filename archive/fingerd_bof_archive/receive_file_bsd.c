#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>

#define SERVER_IP "172.17.0.1"
#define SERVER_PORT 4444
#define IO_BUF_SIZE 2048

int send_int(num, fd)
	int num; 
	int fd; 
{
    unsigned long conv = htonl((unsigned long)num);
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

int receive_int(num, fd)
	int *num; 
	int fd;
{
    unsigned long ret;
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
	int i, j, n;
    int server_sock;
	int file_size, len, num_files;
	struct timeval tv;
    struct sockaddr_in serv_addr;
	char io_buf[IO_BUF_SIZE];
	fd_set read_fds;
	FILE *outfile;
	char *file_name;
	char *msg = "Files recieved\n";
	char *hello = "Hello from client!\n";

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock < 0) {
        perror("socket");
        return 1;
    }

	bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
	serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	
	/* block until connection is established */
    while (connect(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
		printf("Connection failed, retrying...\n"); 
		sleep(1);
	}	

	while (1) {
		write(server_sock, hello, strlen(hello));
		printf("waiting for server...\n");

		FD_ZERO(&read_fds);
		FD_SET(server_sock, &read_fds);

		tv.tv_sec = 5;
		tv.tv_usec = 0;
			
		if (select(server_sock + 1, &read_fds, (fd_set *)0, (fd_set *)0, &tv) > 0) {
			 
			if (receive_int(&num_files, server_sock) < 0) continue;	
			if (send_int(num_files, server_sock) < 0) continue;
			printf("number of files expecting: %d\n", num_files);

			for (i = 0; i < num_files; i++) {
				if (receive_int(&len, server_sock) < 0) continue;	
				file_name = (char *)malloc((len + 1) * sizeof(char));

				n = read(server_sock, file_name, len);
				file_name[n] = '\0';
				if (n <= 0) break;

				outfile = fopen(file_name, "w"); 
				if (outfile == NULL) break;

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
				free(file_name);
				printf("recieved %dth file\n", i);
				send_int(i, server_sock);
			}	

			/* i need to have proper checking of whether correct number of files have been successfully downloaded */
			write(server_sock, msg, strlen(msg)); 
			break;
		}	
	}	

    close(server_sock);
    return 0;
}
