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
    imagick_log_debug("send fd:%d, pid:%d", c->sockfd, getpid());

    int n, nwrite;
    if (c->status == IC_STATUS_SEND_HEADER) {
        n = c->wbuf.len - c->wpos;
        while (n > 0) {
            nwrite = write(c->sockfd, c->rbuf.c + c->wpos, n);
            if (nwrite < n) {
                if (nwrite == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    imagick_log_error("send http header failure (%d)", errno);
                    imagick_connection_free(c);
                    return;
                }
                break;
            }
            c->wpos += nwrite;
            n -= nwrite;
        }
        if (n == 0) {
            // header already sent
            c->status = IC_STATUS_SEND_BODY;
            c->wpos = 0;
        } else {
            return;
        }
    }

    if (c->status == IC_STATUS_SEND_BODY) {
        n = c->cache->size - c->wpos;
        while (n > 0) {
            nwrite = write(c->sockfd, c->cache->data + c->wpos, n);
            if (nwrite < n) {
                if (nwrite == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    imagick_log_error("send http body failure (%d)", errno);
                    imagick_connection_free(c);
                    return;
                }
                break;
            }
            c->wpos += nwrite;
            n -= nwrite;
        }
        if (n == 0) {
            // header already sent
            c->status = IC_STATUS_FINISH;
            c->wpos = 0;
        } else {
            return;
        }
    }

    if (c->status == IC_STATUS_FINISH) {
        loop->del_event(loop, c->sockfd, IE_READABLE | IE_WRITABLE);
        imagick_connection_free(c);
    }
}

static int imagick_parse_http(imagick_connection_t **c)
{
    imagick_connection_t *cc = *c;
    char header[256] = { 0 };
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", cc->rbuf.len);
    smart_str_0(&cc->wbuf);

    cc->cache = malloc(sizeof(imagick_cache_t));
    cc->cache->size = strlen("hello world") + 1;
    cc->cache->data = (void *)"hello world";
    return 0;
}

static void imagick_recv_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    imagick_connection_t *c = arg;

    if (c->status == IC_STATUS_WAIT_RECV) {
        c->status = IC_STATUS_RECEIVING;
    }

    imagick_log_debug("recv fd:%d, pid:%d:", c->sockfd, getpid());

    char buf[128] = {0};
    int nread;
    while ((nread = read(c->sockfd, buf, 128)) > 0) {
        smart_str_appendl(&c->rbuf, buf, nread);
    }
    if (nread == -1 && errno != EAGAIN) {
        imagick_log_warn("Failed to read data %d", fd);
        goto fatal;
        return;
    }

    if (nread == EAGAIN || nread == EWOULDBLOCK) {
        return;
    }
    // read complete
    smart_str_0(&c->rbuf);
    if (imagick_parse_http(&c) == -1) {
        goto fatal;
    }

    c->status = IC_STATUS_SEND_HEADER;
    loop->add_event(loop, c->sockfd, IE_WRITABLE, sock_send_handler, c);
    return;

fatal:
    loop->del_event(loop, c->sockfd, IE_READABLE);
    imagick_connection_free(c);
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
        imagick_log_debug("new connection %d", fd);
        imagick_set_nonblocking(connfd);
        imagick_connection_t *c = imagick_connection_create(connfd);
        loop->add_event(loop, connfd, IE_READABLE, imagick_recv_handler, c);
    }
}
