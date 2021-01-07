#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "rio.h"

/**
 *
 * BUFFERED
 *
 */

void rio_readinitb(struct rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

// used by rio_readnb and rio_readlineb on the internal rio_buf
// then store everything in usrbuf
static ssize_t rio_read(struct rio_t *rp, char *usrbuf, size_t n)
{
    // check if rio_buf is empty and try to fill it if it is
    // otherwise, do nothing
    while (rp->rio_cnt <= 0) {  
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));

        if (rp->rio_cnt < 0) {
            if (errno != EINTR) return -1;
        } else if (rp->rio_cnt == 0) {  // EOF
            return 0;
        } else {
            rp->rio_bufptr = rp->rio_buf;
        }
    }

    int cnt = rp->rio_cnt < n ? rp->rio_cnt : n;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt    -= cnt;
    return cnt;
}

ssize_t rio_readlineb(struct rio_t *rp, void *usrbuf, size_t maxlen)
{
    char c; // one byte at a time from rio_buf
    char *bufp = usrbuf;

    int n, rc;
    for (n = 1; n < maxlen; n++) { // reserve a byte for NUL
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n') {
                break;  // End of Line
            }
        } else if (rc == 0) {
            if (n == 0)
                return 0; // reading empty file
            else
                break;  // finish reading
        } else {
            return -1; // other types of error
        }
    }

    *bufp = 0;
    return n;
}

ssize_t rio_readnb(struct rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = rio_read(rp, bufp, nleft)) < 0)
            return -1;
        else if (nread == 0)
            break;
        nleft -= nread;
        bufp  += nread;
    }
    return (n - nleft);
}

/**
 *
 * UNBUFFERED
 *
 */

ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;       // how many more to read
    ssize_t nread;          // byte count
    char *bufp = usrbuf;    // current position

    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0; // read nothing this time
            else
                return -1;
        } else if (nread == 0) {
            break;         // nothing more to read 
        }

        nleft -= nread;
        bufp  += nread;
    }
    return (n - nleft);
}

ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;       // how many more to write
    ssize_t nwritten;       // byte count
    char *bufp = usrbuf;    // current position

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR)
                nwritten = 0;   // write nothing this time, try again
            else
                return -1;
        }

        nleft -= nwritten;
        bufp  += nwritten;
    }

    return n;
}
