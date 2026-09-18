#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

ssize_t writen(int fd, const void *buf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    const char *ptr = buf;

    while (nleft > 0) {

        // Write remaining data to the socket
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {

            // If interrupted by a signal, try again
            if (nwritten < 0 && errno == EINTR) {
                nwritten = 0;
            }
            else {
                return -1;
            }
        }

        nleft -= nwritten;
        ptr += nwritten;
    }

    return n;
}