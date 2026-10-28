#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in server_addr;

    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    /* Create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    /* Set server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return 1;
    }

    /* Connect to server */
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to server.\n");

    while (1) {
        fd_set readfds;

        /* Rebuild the set every loop */
        FD_ZERO(&readfds);

        /* stdin */
        FD_SET(STDIN_FILENO, &readfds);

        /* TCP socket */
        FD_SET(sockfd, &readfds);

        /* Wait for either stdin or socket */
        int maxfd = (sockfd > STDIN_FILENO) ? sockfd : STDIN_FILENO;

        int ret = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (ret < 0) {
            perror("select");
            break;
        }

        /* Keyboard input */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            char buffer[1024];

            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }

            if (write(sockfd, buffer, strlen(buffer)) < 0) {
                perror("write");
                break;
            }
        }

        /* Data from server */
        if (FD_ISSET(sockfd, &readfds)) {
            char buffer[1024];

            int n = read(sockfd, buffer, sizeof(buffer) - 1);

            if (n == 0) {
                printf("Server disconnected.\n");
                break;
            }

            if (n < 0) {
                perror("read");
                break;
            }

            buffer[n] = '\0';
            printf("Server: %s", buffer);
            fflush(stdout);
        }
    }

    close(sockfd);

    return 0;
}