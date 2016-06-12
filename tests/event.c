#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "events.h"

#define fatal(fmt, ...) do { \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
    exit(1); \
}while (0)

static int set_nonblocking(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0 ||
            fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

int listen_socket(char *addr, int port)
{
    int sock;
    int flags;

    struct linger ln = {0, 0};
    struct sockaddr_in sin;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return -1;
    }

    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));
    setsockopt(sock, SOL_SOCKET, SO_LINGER, &ln, sizeof(ln));

    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(inet_addr(addr));

    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        goto fail;
    }

    if (listen(sock, 2048) == -1) {
        goto fail;
    }

    return sock;
fail:
    close(sock);
    return -1;
}

void sock_recv_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    char buf[1024] = {0};
    read(fd, buf, 1024);
    fprintf(stdout, buf);
    loop->del_event(loop, fd, IE_READABLE);
}

void main_sock_recv_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    fprintf(stdout, "new connection %d\n", fd);

    struct sockaddr_in clientaddr;
    socklen_t clientlen;

    int connfd = 0;
    for (;;) {
        connfd = accept(fd, (struct sockaddr *)&clientaddr, &clientlen);
        if (connfd == -1) {
            if (errno = EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
        }
        set_nonblocking(connfd);
        loop->add_event(loop, connfd, IE_READABLE, main_sock_recv_handler, NULL);
    }
}

int main (int argc, char **argv)
{
    int sockfd = listen_socket("0.0.0.0", 8888);
    if (sockfd == -1) {
        fatal("Cannot to listen socket");
    }

    set_nonblocking(sockfd);

    imagick_event_loop_t *main_loop = imagick_event_loop_create(1024);
    if (main_loop == NULL) {
        fatal("Can't create event loop %d", errno);
    }

    int r = main_loop->add_event(main_loop, sockfd,
            IE_READABLE, main_sock_recv_handler, NULL);
    if (r == -1) {
        fatal("Can't add event %d", errno);
    }

    main_loop->dispatch(main_loop);
    imagick_event_loop_free(main_loop);
}
