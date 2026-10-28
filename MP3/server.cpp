
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <cerrno>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>

constexpr size_t BLOCK_SIZE = 512;
constexpr int TIMEOUT_SEC = 1;
constexpr int MAX_TIMEOUTS = 10;

void put16(unsigned char* p, uint16_t v) {
    p[0] = static_cast<unsigned char>(v >> 8);
    p[1] = static_cast<unsigned char>(v & 0xff);
}

uint16_t get16(const unsigned char* p) {
    return (static_cast<uint16_t>(p[0]) << 8) |
            static_cast<uint16_t>(p[1]);
}

void send_error(int sockfd, const sockaddr_in& client,
                socklen_t client_len, uint16_t code,
                const char* message) {
    unsigned char packet[516];
    put16(packet, 5);
    put16(packet + 2, code);

    size_t len = std::strlen(message);
    if (len > sizeof(packet) - 5)
        len = sizeof(packet) - 5;

    std::memcpy(packet + 4, message, len);
    packet[4 + len] = '\0';

    sendto(sockfd, packet, len + 5, 0,
           reinterpret_cast<const sockaddr*>(&client), client_len);
}

bool same_client(const sockaddr_in& a, const sockaddr_in& b) {
    return a.sin_family == b.sin_family &&
           a.sin_addr.s_addr == b.sin_addr.s_addr &&
           a.sin_port == b.sin_port;
}

// Read and send a file using TFTP octet mode.
void serve_rrq(int sockfd, const sockaddr_in& client,
               socklen_t client_len, const std::string& filename) {
    // Restrict access to a simple filename in the current directory.
    if (filename.empty() || filename == "." || filename == ".." ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos) {
        send_error(sockfd, client, client_len, 2,
                   "Access violation");
        return;
    }

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        send_error(sockfd, client, client_len, 1,
                   "File not found");
        return;
    }

    struct stat st{};
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        send_error(sockfd, client, client_len, 2,
                   "Access violation");
        return;
    }

    uint16_t block = 1;
    bool finished = false;

    while (!finished) {
        unsigned char packet[4 + BLOCK_SIZE];
        ssize_t n;

        // Read one block. A zero-byte block marks EOF when
        // the file size is an exact multiple of 512 bytes.
        do {
            n = read(fd, packet + 4, BLOCK_SIZE);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            send_error(sockfd, client, client_len, 0,
                       "File read error");
            break;
        }

        put16(packet, 3);       // DATA opcode
        put16(packet + 2, block);

        int consecutive_timeouts = 0;
        bool acknowledged = false;

        while (!acknowledged) {
            ssize_t sent = sendto(
                sockfd, packet, static_cast<size_t>(n) + 4, 0,
                reinterpret_cast<const sockaddr*>(&client),
                client_len);

            if (sent < 0) {
                perror("sendto");
                close(fd);
                return;
            }

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);

            timeval timeout{};
            timeout.tv_sec = TIMEOUT_SEC;

            int ready = select(sockfd + 1, &readfds,
                               nullptr, nullptr, &timeout);

            if (ready < 0) {
                if (errno == EINTR)
                    continue;
                perror("select");
                close(fd);
                return;
            }

            if (ready == 0) {
                ++consecutive_timeouts;
                if (consecutive_timeouts >= MAX_TIMEOUTS) {
                    std::cerr << "Client timed out; PID "
                              << getpid() << std::endl;
                    close(fd);
                    return;
                }
                // Retransmit the same DATA block.
                continue;
            }

            unsigned char ack[2049];
            sockaddr_in from{};
            socklen_t from_len = sizeof(from);

            ssize_t received = recvfrom(
                sockfd, ack, sizeof(ack), 0,
                reinterpret_cast<sockaddr*>(&from), &from_len);

            if (received < 0) {
                if (errno == EINTR)
                    continue;
                perror("recvfrom");
                close(fd);
                return;
            }

            // Ignore packets from other clients.
            if (!same_client(from, client))
                continue;

            // An ACK must be exactly four bytes.
            if (received != 4 || get16(ack) != 4)
                continue;

            uint16_t ack_block = get16(ack + 2);

            if (ack_block == block) {
                acknowledged = true;
            }
            // Ignore duplicate or unexpected ACKs.
            // The current DATA is retransmitted only on timeout.
        }

        if (n < static_cast<ssize_t>(BLOCK_SIZE)) {
            finished = true;
        } else {
            // uint16_t arithmetic wraps 65535 -> 0.
            block = static_cast<uint16_t>(block + 1);
        }
    }

    close(fd);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>\n";
        return 1;
    }

    char* end = nullptr;
    long port = std::strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' ||
        port < 1 || port > 65535) {
        std::cerr << "Invalid port number\n";
        return 1;
    }

    // Reap terminated child processes automatically.
    struct sigaction sa{};
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, nullptr);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(sockfd, reinterpret_cast<sockaddr*>(&server_addr),
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    std::cout << "TFTP server listening on port "
              << port << std::endl;

    while (true) {
        unsigned char request[2049];
        sockaddr_in client{};
        socklen_t client_len = sizeof(client);

        ssize_t n = recvfrom(
            sockfd, request, sizeof(request), 0,
            reinterpret_cast<sockaddr*>(&client), &client_len);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            continue;
        }

        // Require opcode, filename NUL, mode NUL.
        if (n < 4 || n >= static_cast<ssize_t>(sizeof(request))) {
            send_error(sockfd, client, client_len, 4,
                       "Illegal TFTP operation");
            continue;
        }

        if (get16(request) != 1) {
            send_error(sockfd, client, client_len, 4,
                       "Only RRQ is supported");
            continue;
        }

        const unsigned char* endp = request + n;
        const unsigned char* filename_end =
            static_cast<const unsigned char*>(
                std::memchr(request + 2, '\0',
                            static_cast<size_t>(endp - (request + 2))));

        if (filename_end == nullptr || filename_end == request + 2) {
            send_error(sockfd, client, client_len, 4,
                       "Invalid filename");
            continue;
        }

        const unsigned char* mode_start = filename_end + 1;
        if (mode_start >= endp) {
            send_error(sockfd, client, client_len, 4,
                       "Missing mode");
            continue;
        }

        const unsigned char* mode_end =
            static_cast<const unsigned char*>(
                std::memchr(mode_start, '\0',
                            static_cast<size_t>(endp - mode_start)));

        if (mode_end == nullptr || mode_end == mode_start ||
            mode_end + 1 != endp) {
            send_error(sockfd, client, client_len, 4,
                       "Invalid request fields");
            continue;
        }

        std::string filename(
            reinterpret_cast<const char*>(request + 2),
            static_cast<size_t>(filename_end - (request + 2)));

        std::string mode(
            reinterpret_cast<const char*>(mode_start),
            static_cast<size_t>(mode_end - mode_start));

        for (char& c : mode) {
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
        }

        if (mode != "octet") {
            send_error(sockfd, client, client_len, 4,
                       "Only octet mode is implemented");
            continue;
        }

        char ip[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &client.sin_addr, ip, sizeof(ip));

        std::cout << "RRQ: " << filename << " from "
                  << ip << ":" << ntohs(client.sin_port)
                  << " mode=" << mode << std::endl;

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            send_error(sockfd, client, client_len, 0,
                       "Server error");
        } else if (pid == 0) {
            close(sockfd);

            int child_sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (child_sock < 0) {
                perror("child socket");
                _exit(1);
            }

            sockaddr_in local{};
            local.sin_family = AF_INET;
            local.sin_addr.s_addr = htonl(INADDR_ANY);
            local.sin_port = htons(0);

            if (bind(child_sock,
                     reinterpret_cast<sockaddr*>(&local),
                     sizeof(local)) < 0) {
                perror("child bind");
                close(child_sock);
                _exit(1);
            }

            serve_rrq(child_sock, client, client_len, filename);
            close(child_sock);
            _exit(0);
        }
        // Parent continues listening on the well-known port.
    }

    close(sockfd);
    return 0;
}
