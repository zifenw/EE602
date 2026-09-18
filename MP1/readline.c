#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

ssize_t readline(int fd, void *buf, size_t maxlen)
{
    char *ptr = buf;
    size_t nleft = maxlen;
    char c;
    ssize_t nread;

    while (nleft > 1) {

        // Read one byte from the file descriptor
        nread = read(fd, &c, 1);

        if (nread == 1) {

            // Store the character in the buffer
            *ptr++ = c;
            nleft--;

            // Stop when a newline is received
            if (c == '\n') {
                break;
            }
        }
        else if (nread == 0) {

            // EOF: no more data to read
            break;
        }
        else {

            // If interrupted by a signal, try again
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }
    }

    // Add null terminator
    *ptr = '\0';

    return maxlen - nleft;
}