#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

void set_nonblock(int fd)
{
    int fileflags;

    if ((fileflags = fcntl(fd, F_GETFL, 0)) == -1) {
        perror("F_GETFL");
        exit(EXIT_FAILURE);
    }

    if ((fileflags = fcntl(fd, F_SETFL, fileflags | O_NONBLOCK) == -1)) {
        perror("F_SETFL");
        exit(EXIT_FAILURE);
    }
}
