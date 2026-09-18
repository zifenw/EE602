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

#define LISTEN_BACKLOG 10

int main(int argc, char *argv[])
{
    // Listening socket
    int listenfd;
    struct sockaddr_in server_addr;

    // echos <port>
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Create TCP socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));

    // IPv4
    server_addr.sin_family = AF_INET;

    // Listen on any local network interface
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Convert port number to network byte order
    server_addr.sin_port = htons(atoi(argv[1]));

    // Bind socket to the specified port
    if (bind(listenfd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {

        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Server bound to port %s\n", argv[1]);

    // Start listening for incoming connections
    if (listen(listenfd, LISTEN_BACKLOG) < 0) {

        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %s\n", argv[1]);

    // Connected socket
    int connfd;
    struct sockaddr_in client_addr;
    socklen_t client_len;

    while (1) {

        client_len = sizeof(client_addr);

        // Accept a new client connection
        connfd = accept(listenfd,
                        (struct sockaddr *)&client_addr,
                        &client_len);

        if (connfd < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected!\n");

        // Create a child process to handle the client
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            close(connfd);
            continue;
        }

        if (pid == 0) {

            // Child process
            close(listenfd);

            printf("Child process handling client\n");

            char buf[4096];
            ssize_t nread;

            // Read data from the client
            while ((nread = read(connfd, buf, sizeof(buf))) > 0) {

                // Echo the received data back to the client
                if (writen(connfd, buf, nread) < 0) {
                    perror("writen");
                    close(connfd);
                    exit(EXIT_FAILURE);
                }
            }

            // Check for read error
            if (nread < 0) {
                perror("read");
            }

            close(connfd);
            exit(0);
        }

        // Parent process closes its copy of the connected socket
        close(connfd);
    }

    close(listenfd);

    return 0;
}