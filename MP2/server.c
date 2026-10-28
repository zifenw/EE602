#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    int listenfd;
    int connfd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len;

    char buffer[BUFFER_SIZE];

    /*
     * Check command-line arguments
     */
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*
     * Create socket
     */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    if (listenfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /*
     * Initialize server address
     */
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    /*
     * Bind socket to port
     */
    if (bind(listenfd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    /*
     * Start listening
     */
    if (listen(listenfd, 10) < 0)
    {
        perror("listen");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %s...\n", argv[1]);

    /*
     * Accept one client
     */
    client_len = sizeof(client_addr);

    connfd = accept(listenfd,
                    (struct sockaddr *)&client_addr,
                    &client_len);

    if (connfd < 0)
    {
        perror("accept");
        close(listenfd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected from %s:%d\n",
           inet_ntoa(client_addr.sin_addr),
           ntohs(client_addr.sin_port));

    /*
     * Receive data from client
     */
    int n = read(connfd, buffer, BUFFER_SIZE - 1);

    if (n < 0)
    {
        perror("read");
    }
    else if (n == 0)
    {
        printf("Client disconnected.\n");
    }
    else
    {
        buffer[n] = '\0';

        printf("Received: %s\n", buffer);
    }

    /*
     * Close sockets
     */
    close(connfd);
    close(listenfd);

    return 0;
}