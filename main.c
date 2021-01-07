#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>

#include "utils.h"
#include "tpool.h"
#include "http.h"

#define PORT "3333"
#define SHORTMAX 512
#define LONGMAX  8192
#define MAXEVENT 1024

/*
 * Currently this proxy server supports HTTP only, or
 * more precisely the GET method for HTTP/1.x.
 *
 * Also, the thread-safety of stdio functions is 
 * guaranteed only for POSIX systems and therefore
 * this proxy server may not work as expected on other
 * systems.
 */

static void request_handler(void *arg);
static int  setup_listenfd();

int listenfd;
int epfd;

int main()
{
    /*
     * according to
     * http://www.cs.cmu.edu/afs/cs/academic/class/15213-f01/L7/L7.pdf
     *
     * to suppress SIGPIPE error
     */
    signal(SIGPIPE, SIG_IGN);

    listenfd = setup_listenfd();
    set_nonblock(listenfd);

    tpool_t *tpool = tpool_create(4);
    perror("pool create");

    epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event ev;
    ev.data.fd = listenfd;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
        perror("EPOLL_CTL_ADD");
    }

    struct epoll_event * events;
    events = (struct epoll_event *) malloc(sizeof(struct epoll_event) * MAXEVENT);

    fprintf(stderr, "proxy server started\n");

    while (1) {
        int ready_fds = epoll_wait(epfd, events, MAXEVENT, -1);

        int i;
        for (i = 0; i < ready_fds; ++i) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                fprintf(stderr, "epoll_wait: unexpected event detected, closing affected fd\n");
                close(events[i].data.fd);
                continue;
            }

            tpool_add_job(tpool, request_handler, &events[i].data.fd);
        }
    }

    tpool_wait(tpool);
    tpool_destroy(tpool);

    return 0;
}

static void request_handler(void *arg)
{
    char s[INET6_ADDRSTRLEN] = {0};
    int cli_fd;
    struct sockaddr_storage cli_addr;
    socklen_t sin_size = sizeof(cli_addr);

    struct epoll_event ev;

    int fd = * (int *) arg;

    if (fd == listenfd) {
        for (;;) {
            cli_fd = accept(fd, (struct sockaddr *) &cli_addr, &sin_size);
            if (cli_fd == -1) {
                if ((errno == EAGAIN) ||
                    (errno == EWOULDBLOCK)) { // has handled all requests
                    break;
                } else {
                    fprintf(stderr, "accept");
                    break; // see man accept for more errors
                }
            }

            inet_ntop(cli_addr.ss_family, get_in_addr((struct sockaddr *) &cli_addr), s, sizeof(s));
            fprintf(stderr, "client %s\n", s);

            set_nonblock(cli_fd);

            ev.data.fd = cli_fd;
            ev.events = EPOLLIN | EPOLLET;

            if (epoll_ctl(epfd, EPOLL_CTL_ADD, cli_fd, &ev) == -1) {
                perror("EPOLL_CTL_ADD");
            }
        }
    } else {
        proxy_connect(fd);
        /*
         * TODO
         * when errors occur, proxy_connect simply returns and 
         * and the job will simply be destroyed, what should we
         * do to the socket?
         */
    }
}

static int setup_listenfd()
{
    struct addrinfo hints, *servinfo, *servinfo_list;
    int sockfd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err;
    if ((err = getaddrinfo(NULL, PORT, &hints, &servinfo_list))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(EXIT_FAILURE);
    }

    for (servinfo = servinfo_list; servinfo != NULL; servinfo = servinfo->ai_next) {
        if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }

        if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
            close(sockfd);
            perror("bind");
            continue;
        }

        break;
    }

    if (servinfo == NULL) {
        fprintf(stderr, "failed to bind socket\n");
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, SOMAXCONN) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(servinfo_list);

    return sockfd;
}
