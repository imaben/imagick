#include "worker.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include "log.h"
#include "utils.h"
#include "http_parser.h"
#include "connection.h"

struct http_parser_settings hp_setting;

static void sock_send_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    imagick_connection_t *c = arg;
    fprintf(stdout, "send fd:%d, pid:%d\n", c->sockfd, getpid());
    char buf[1024] = {0};
    int n;
    sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\nHello World", 11);
    int nwrite, data_size = strlen(buf);
    n = data_size;
    while (n > 0) {
        nwrite = write(c->sockfd, buf + data_size - n, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN) {
                fprintf(stderr, "write error");
            }
            break;
        }
        n -= nwrite;
    }
    loop->del_event(loop, c->sockfd, IE_READABLE | IE_WRITABLE);
    imagick_connection_free(c);
}

static void imagick_http_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    imagick_connection_t *c = arg;
    fprintf(stdout, "recv fd:%d, pid:%d:\n", c->sockfd, getpid());
    char buf[1024] = {0};
    int n = 0, nread;
    while ((nread = read(c->sockfd, buf + n, 1024 - n)) > 0) {
        n += nread;
    }
    if (nread == -1 && errno != EAGAIN) {
        fprintf(stderr, "read error");
    }
    loop->add_event(loop, c->sockfd, IE_WRITABLE, sock_send_handler, c);
}

void imagick_main_sock_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    struct sockaddr_in clientaddr;
    socklen_t clientlen;

    int connfd = 0;
    for (;;) {
        connfd = accept(fd, (struct sockaddr *)&clientaddr, &clientlen);
        if (connfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
        }
        imagick_log_debug("new connection %d\n", fd);
        imagick_set_nonblocking(connfd);
        imagick_connection_t *c = imagick_connection_create(connfd);
        loop->add_event(loop, connfd, IE_READABLE, imagick_http_handler, c);
    }
}
