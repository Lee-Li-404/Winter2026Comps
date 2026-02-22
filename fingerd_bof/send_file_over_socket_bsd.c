#include "worm_bsd.h"

#define NUM_CLIENTS 5
#define SERVER_PORT 4444
#define IO_BUF_SIZE 2048
#define FILE_BUF_SIZE 256

int main(argc, argv)
	int argc; 
	char *argv[]; 
{
	int i, n, num_files;

	char **file_names; 
	struct file_object *files; 
	char file_buf[FILE_BUF_SIZE];

	fd_set read_fd; 	
    int server_fd, max_fd, accept_code, sd;
	int client_sock[NUM_CLIENTS];
	char io_buf[IO_BUF_SIZE];

	char *success = "Files recieved";

    struct sockaddr_in address;
    int addrlen = sizeof(address);

	if (argc < 2) {
		printf("No files to transfer\n");
		return 1;
	}	

	num_files = argc - 1;
	file_names = argv + 1;
	files = (struct file_object *)malloc(num_files * sizeof(struct file_object));
	if (load_files(file_names, num_files, files) < 0) return 1;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(SERVER_PORT);       

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));

    listen(server_fd, NUM_CLIENTS + 1);
	printf("Listening for connections at port %d\n", SERVER_PORT);

	for (i = 0; i < NUM_CLIENTS; i++) {
        client_sock[i] = -1;
    }

	while (1) {
		FD_ZERO(&read_fd);
		FD_SET(server_fd, &read_fd);

		max_fd = server_fd;

		for (i = 0; i < NUM_CLIENTS; i++) {
			if (client_sock[i] > 0) {
				FD_SET(client_sock[i], &read_fd);	
				if (client_sock[i] > server_fd) max_fd = client_sock[i];
			}	
		}	

		/* block until there is activity */
		if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) < 0) continue;

		/* accept oncoming connections  */
		if (FD_ISSET(server_fd, &read_fd)) {
			accept_code = accept(server_fd, (struct sockaddr *)&address, &addrlen);
			if (accept_code < 0) continue;

			for (i = 0; i < NUM_CLIENTS; i++) {
				if (client_sock[i] == -1) { 
					printf("New connection received! FD: %d\n", accept_code);
					client_sock[i] = accept_code;
					break;
				}	
			}	
		}	

		/* communicating with the client sockets  */
		for (i = 0; i < NUM_CLIENTS; i++) {
			sd = client_sock[i];
			if (sd > 0 && FD_ISSET(sd, &read_fd)) {
				n = read(sd, io_buf, IO_BUF_SIZE - 1);

				if (n == 0) {
					printf("Client disconnected\n");
					close(sd);
					client_sock[i] = -1;
				} else {	
					io_buf[n] = '\0';
					printf("Recieved from %d: %s", sd, io_buf);
					/* no need to send over ifles when client says it recieved the files */
					if (strncmp(success, io_buf, strlen(success)) == 0) continue;

					if (send_files(sd, files, num_files, file_buf, FILE_BUF_SIZE) < 0)
						printf("Failed to send all files\n");
				}	
			}	
		}	
	}	

	close(server_fd);
	free(files);
	
	return 0;	
}	
