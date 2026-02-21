#include "worm.h"

#define NUM_CLIENTS 5
#define SERVER_PORT 4444
#define IO_BUF_SIZE 2048

int main() {
    int server_fd;
	int NUM_FILES = 2;
	char *file_names[2] = {"worm.h", "demo.sh"};
	FILE *files[2];
	char file_buf[256];
	char *success = "Files recieved";

	for (int i = 0; i < NUM_FILES; i++) {
		printf("%s\n", file_names[i]);
		files[i] = fopen(file_names[i], "r"); 
		if (files[i] == NULL) return 1;
	}	

    struct sockaddr_in address;
    int addrlen = sizeof(address);

	char *hello = "Hello from the server!\n";

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(SERVER_PORT);       

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));

    listen(server_fd, NUM_CLIENTS + 1);
	printf("Listening for connections at port %d\n", SERVER_PORT);

	// int client_sock[NUM_CLIENTS] = {-1}; this does not work, for it only sets the first cell to -1!
	int client_sock[NUM_CLIENTS];
	for (int i = 0; i < NUM_CLIENTS; i++) {
        client_sock[i] = -1;
    }

	fd_set read_fd; 	

	char io_buf[IO_BUF_SIZE];

	while (1) {
		FD_ZERO(&read_fd);
		FD_SET(server_fd, &read_fd);

		int max_fd = server_fd;

		for (int i = 0; i < NUM_CLIENTS; i++) {
			if (client_sock[i] > 0) {
				FD_SET(client_sock[i], &read_fd);	
				if (client_sock[i] > server_fd) max_fd = client_sock[i];
			}	
		}	

		// block until there is activity
		int activity = select(max_fd + 1, &read_fd, NULL, NULL, NULL);

		if (activity < 0) continue;

		// accept oncoming connections 
		if (FD_ISSET(server_fd, &read_fd)) {
			int accept_code = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
			if (accept_code < 0) continue;

			for (int i = 0; i < NUM_CLIENTS; i++) {
				if (client_sock[i] == -1) { 
					printf("New connection received! FD: %d\n", accept_code);
					client_sock[i] = accept_code;
					break;
				}	
			}	
		}	

		// communicating with the client sockets 
		for (int i = 0; i < NUM_CLIENTS; i++) {
			int sd = client_sock[i];
			if (sd > 0 && FD_ISSET(sd, &read_fd)) {
				int n = read(sd, io_buf, IO_BUF_SIZE - 1);

				if (n == 0) {
					printf("Client disconnected\n");
					close(sd);
					client_sock[i] = -1;
				} else {	
					io_buf[n] = '\0';
					printf("Recieved from %d: %s", sd, io_buf);
					// no need to send over ifles when client says it recieved the files
					if (strncmp(success, io_buf, strlen(success)) == 0) continue;

					send_files(sd, files, file_names, NUM_FILES, file_buf, 256);
				}	
			}	
		}	
	}	

	close(server_fd);
	
	return 0;	
}	
