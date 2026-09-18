#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "mp1.h"

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in server_addr;

    // echo <ip_addr> <port>
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ip_addr> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    // IPv4
    server_addr.sin_family = AF_INET;

    // Convert IP address from text to binary form
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address: %s\n", argv[1]);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Convert port number to network byte order
    server_addr.sin_port = htons(atoi(argv[2]));

    // Connect to server
    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server!\n");

    char sendbuf[4096];
    char recvbuf[4096];

    ssize_t n;
    ssize_t nread;
    
    // Read input from stdin and send it to the server
    while ((n = readline(STDIN_FILENO,
                        sendbuf,
                        sizeof(sendbuf))) > 0) {

        // Send the input line to the server
        if (writen(sockfd, sendbuf, n) < 0) {
            perror("writen");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Read the echoed data from the server
        nread = read(sockfd, recvbuf, sizeof(recvbuf));

        if (nread < 0) {
            perror("read");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Print the echoed data
        if (write(STDOUT_FILENO, recvbuf, nread) < 0) {
            perror("write");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
    }

    close(sockfd);

    return 0;
}