#include "worker.h"
#include "imagick.h"
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

static int imagick_http_parser_url(struct http_parser *hp, const char *at, size_t len);

struct http_parser_settings hp_setting = {
    .on_message_begin    = NULL,
    .on_url              = imagick_http_parser_url,
    .on_header_field     = NULL,
    .on_header_value     = NULL,
    .on_headers_complete = NULL,
    .on_body             = NULL,
    .on_message_complete = NULL
};

imagick_cache_t cache_html_page_404 = {
    .type = CACHE_TYPE_HTML,
    .data = "<html>"
"<head><title>404 Not Found</title></head>"
"<body bgcolor=\"white\">"
"<center><h1>404 Not Found</h1></center>"
"<hr><div align=\"center\">Imagick " IMAGICK_VERSION "</div>"
"</body>"
"</html>"
};

static int imagick_http_parser_url(struct http_parser *hp, const char *at, size_t len)
{
    imagick_connection_t *conn = hp->data;
    if (len <= 0) {
        return -1;
    }
    smart_str_appendl(&conn->filename, at, len);
    smart_str_0(&conn->filename);
    return 0;
}

static void imagick_sock_send_handler(imagick_event_loop_t *loop, int fd, void *arg)
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
    int retval;

    retval = http_parser_execute(&cc->hp, &hp_setting, cc->rbuf.c, cc->rbuf.len);
    if (retval == 0) {
        // todo: assign bad request
        return 0;
    }

    cc->cache = malloc(sizeof(imagick_cache_t));
    if (cc->hp.method == HTTP_GET) {
        cc->cache->size = strlen("hello world") + 1;
        cc->cache->data = (void *)"hello world";
    } else if (cc->hp.method == HTTP_POST) {
        cc->cache->size = strlen("world hello") + 1;
        cc->cache->data = (void *)"world hello";
    }
    char header[256] = { 0 };
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", cc->rbuf.len);
    smart_str_0(&cc->wbuf);

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
    loop->add_event(loop, c->sockfd, IE_WRITABLE, imagick_sock_send_handler, c);
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
