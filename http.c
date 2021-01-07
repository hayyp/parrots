#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>

#include "http.h"
#include "utils.h"
#include "rio.h"

#define LONGMAX 1024*8 /* a often-used limit for the size of a HTTP request */
#define LLMAX 65535    /* the size of a Full Request */
#define SHORTMAX 512

/*
 * TODO keep client and remote server connected
 */

static int  parse_url(char *url, char *hostname, char *rest);
static void proxy_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


/*
 * BACKGROUND
 *
 * According to
 * http://www.csc.villanova.edu/~mdamian/Past/csc8410sp07/hw/proxyc.pdf
 *
 * a HTTP request to a proxy server should look like
 *
 *      GET http://www.google.com/index.html HTTP/1.0
 * 
 * and as depicted in
 * https://courses.engr.illinois.edu/cs241/sp2012/lectures/35-http.pdf
 *
 * each line in a HTTP request header should be CRLF-terminated and the
 * entire request header should be terminated by a trailing CRLF, which
 * is then immediately followed by the request body.
 *
 */

void proxy_connect(int fd)
{

    /* client (localhost) -> [listen, accept, >read<, parse] proxy [connect, write] -> server (google)
     *     ^                                                                            | [read, parse]
     *     |----------------------------------------------------------------- [write] proxy
     *
     * Currently, threads use synchronous blocking I/O when dealing with remote servers
     */

    struct rio_t rio;
    char method[SHORTMAX], url[SHORTMAX], version[SHORTMAX];
    char buf[LONGMAX];

    char hostname[SHORTMAX] = {0}, rest[SHORTMAX] = {0};
    char headers[LONGMAX]; // anything following the startline

    rio_readinitb(&rio, fd);

    fprintf(stderr, "parsing HTTP request from client\n");

    int rc = rio_readlineb(&rio, buf, LONGMAX);
    if (rc == -1) {
        if (errno != EAGAIN) {// neither EAGAIN nor EINTR
            /*
             * not sure what might happen yet, print error and
             * stop reading for later investigation should any
             * error occurs
             */
            perror("rio_readlineb");
            close(fd); // TODO
        } 
       // else {
       //     break;
       // }
    }

    sscanf(buf, "%s %s %s", method, url, version);

    if (strcasecmp(method, "GET")) {
        fprintf(stderr, "parser: unsupported http method\n");
        proxy_error(fd, method, "501", "Unsupported Method", "HTTP Method Not Supported");
        close(fd);
        return;
    }

    int error = parse_url(url, hostname, rest);
    // if (error) {
    //     // what to do??
    //     close(fd); // TODO
    //     return;
    // }
    
    fprintf(stderr, "remote server address confirmed, %s\n", hostname);

    while (rc = rio_readlineb(&rio, buf, LONGMAX) && rc > 0  && strcmp(buf, "\r\n")) {
        if (!strncmp(buf, "Content-length", 14)) {
            continue;
        }
        if (!strncmp(buf, "GET", 3)) {
            break;
        }
        strcat(headers, buf);
    }
    if (rc == -1) {
        if (errno != EAGAIN) {// neither EAGAIN nor EINTR
            /*
             * not sure what might happen yet, print error and
             * stop reading for later investigation should any
             * error occurs
             */
            perror("rio_readlineb");
            close(fd);
            //break;
            return;
        } 
    }

    // send request and wait for a response

    fprintf(stderr, "forwarding request to a remote server\n");

    int sockfd, numbytes;
    char reverse_buf[LONGMAX];
    struct addrinfo hints, *servlist, *serv;
    int err;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((err = getaddrinfo(hostname, "80", &hints, &servlist))) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
            return;
    }

    for (serv = servlist; serv != NULL; serv = serv->ai_next) {
        if ((sockfd = socket(serv->ai_family, serv->ai_socktype, serv->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, serv->ai_addr, serv->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (serv == NULL) {
        fprintf(stderr, "client, failed to connect\n");
        return; // what to do?
    }

    inet_ntop(serv->ai_family, get_in_addr((struct sockaddr *) serv->ai_addr), s, sizeof(s));
    printf("remote server found %s\n", s);

    freeaddrinfo(servlist);

    char forward_header[LONGMAX];

    // Undefined behavious may occur if the request contains a body

    fprintf(stderr, "target: %s\n", rest);
    fprintf(stderr, "preserved header lines:\n%s", headers);
    sprintf(forward_header, "GET %s HTTP/1.1\r\n", rest);
    strcat(forward_header, headers);
    sprintf(forward_header, "%sContent-length: %d\r\n\r\n", forward_header, (int) strlen(forward_header));

    fprintf(stderr, "header generated:\n%s", forward_header);
    
    rio_writen(sockfd, forward_header, strlen(forward_header));
    fprintf(stderr, "new header written and sent\n");
    

    // forward the response back to our client

    char response_header[LONGMAX] = {0};
    char response_body[LLMAX] = {0};
    char response_body_lenth_c[SHORTMAX] = {0};
    int response_body_length;

    // read the entire response
    struct rio_t rio_response;
    rio_readinitb(&rio_response, sockfd);
    rc = rio_readlineb(&rio_response, buf, LONGMAX);
    fprintf(stderr, "response header: %s\n", buf);
    strcat(response_header, buf);
    while (rc = rio_readlineb(&rio_response, buf, LONGMAX) && rc > 0  && strcmp(buf, "\r\n")) {
        if (!strncasecmp(buf, "Content-length", 14)) {
            strcpy(response_body_lenth_c, buf + 15);
            response_body_length = atoi(response_body_lenth_c);
        }
        strcat(response_header, buf);
    }
    if (rc == -1) {
        perror("rio_readlineb");
        close(fd);
        return;
    }
    strcat(response_header, "\r\n");
    fprintf(stderr, "finished reading response header\n%s\n", response_header);

    int response_size_limited = fmin(LLMAX, response_body_length);
    rio_writen(fd, response_header, strlen(response_header));

    rc = rio_readnb(&rio_response, response_body, response_size_limited);
    // fprintf(stderr, "response body length (actual): %d\n", rc);

    rio_writen(fd, response_body, response_body_length);
    // fprintf(stderr, "response body is as follows: \n%s", response_body);
    close(sockfd);
    close(fd);
}

void proxy_error(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char header[LONGMAX], body[LONGMAX];

    sprintf(body, 
        "<html><title>ERROR</title>\r\n"
        "%s: %s\r\n"
        "<p>%s: %s\r\n</p>"
        "<hr><em>PROXY</em></body></html>\r\n",
        errnum, shortmsg, cause, longmsg);

    sprintf(header, 
        "HTTP/1.0 %s %s\r\n"
        "Connection: close\r\n"
        "Content-length: %d\r\n\r\n",
        errnum, shortmsg, (int) strlen(body));
    
    rio_writen(fd, header, strlen(header));
    rio_writen(fd, body, strlen(body));
}

static int parse_url(char *url, char *hostname, char *rest)
// for example, http://www.google.com/index.html 
{
    fprintf(stderr, "parser: received new url %s\n", url);
    int protocol_len;
    int hostname_len;
    char protocol[5] = {0};
    strncpy(protocol, url, 5);
    fprintf(stderr, "parser: protocol confirmed %s\n", protocol);
    if (!strncmp(protocol, "https", 5)) {
        protocol_len = 8;
        return -1;
    } else {
        protocol_len = 7;
    }
        fprintf(stderr,"parser: protocol length is %d\n", protocol_len);


    int i;
    for (i = protocol_len; i < SHORTMAX; ++i) {
        if (url[i] == '/') break;
    }

    if (i == SHORTMAX - 1) { // currently 512 but a valid domain name should be shorter than 253
        fprintf(stderr, "error occurs when parsing url: %s\n", url);
        return -1;
    } else {
        hostname_len = i - protocol_len;
        fprintf(stderr, "parser, hostname length confirmed  %d\n", hostname_len);
        strncat(hostname, url + protocol_len, hostname_len);
        strcat(rest, url + protocol_len + hostname_len);
        fprintf(stderr, "parser: hostname confirmed %s\n", hostname);
    }

    return 0;
}

