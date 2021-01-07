#ifndef RIO_H
#define RIO_H

#include <unistd.h>

#define RIO_BUFSIZE 8192

struct rio_t {
    int rio_fd;                 // descriptor used
    int rio_cnt;                // unread bytes in rio_buf
    char *rio_bufptr;           // next unread byte in rio_buf
    char rio_buf[RIO_BUFSIZE];  // an internal buffer
};

// initializing buffer
void rio_readinitb(struct rio_t *rp, int fd);

// buffered. 
// read n bytes or read a line of up to maxlen bytes from rio_buf
ssize_t rio_readlineb(struct rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t rio_readnb(struct rio_t *rp, void *usrbuf, size_t n);

// unbuffered.
// try its best to read n bytes and write n bytes using UNIX IO
ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);

#endif
