#include "worker.h"
#include "imagick.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netdb.h>
#include "log.h"
#include "utils.h"
#include "http_parser.h"
#include "connection.h"
#include "lock.h"
#include "hash.h"

static int imagick_http_parser_url(struct http_parser *hp, const char *at, size_t len);
static struct http_parser_settings hp_setting = {
    .on_message_begin    = NULL,
    .on_url              = imagick_http_parser_url,
    .on_header_field     = NULL,
    .on_header_value     = NULL,
    .on_headers_complete = NULL,
    .on_body             = NULL,
    .on_message_complete = NULL
};

static int imagick_get_full_path(smart_str *dst, char *path)
{
    if (NULL == dst || NULL == path || strlen(path) == 0) {
        return -1;
    }
    return imagick_path_join(dst, main_ctx->setting->imgroot, path, NULL);
}

static char html_page_400[] = "<html>"
"<head><title>400 Bad Request</title></head>"
"<body bgcolor=\"white\">"
"<center><h1>400 Bad Request</h1></center>"
"<hr><div align=\"center\">Imagick " IMAGICK_VERSION "</div>"
"</body>"
"</html>";

static char html_page_404[] = "<html>"
"<head><title>404 Not Found</title></head>"
"<body bgcolor=\"white\">"
"<center><h1>404 Not Found</h1></center>"
"<hr><div align=\"center\">Imagick " IMAGICK_VERSION "</div>"
"</body>"
"</html>";

static char header_page_400[] = "HTTP/1.1 400 Bad Request\r\n"
"Content-Type: text/html\r\n"
"Content-Length: 166\r\n"
"Server: Imagick\r\n\r\n";

static char header_page_404[] = "HTTP/1.1 404 Not Found\r\n"
"Content-Type: text/html\r\n"
"Content-Length: 166\r\n"
"Server: Imagick\r\n\r\n";

static imagick_cache_t cache_page_400 = {
    .flag = CACHE_TYPE_HTML | CACHE_TYPE_INTERNAL,
    .ref_count = 0,
    .http_code = 400,
    /* !!! read only !!! */
    .header = {
        .c = header_page_400,
        .len = sizeof(header_page_400)
    },
    .wpos = 0,
    .size = sizeof(html_page_400),
    .data = html_page_400
};

static imagick_cache_t cache_page_404 = {
    .flag = CACHE_TYPE_HTML | CACHE_TYPE_INTERNAL,
    .ref_count = 0,
    .http_code = 404,
    /* !!! read only !!! */
    .header = {
        .c = header_page_404,
        .len = sizeof(header_page_404)
    },
    .wpos = 0,
    .size = sizeof(html_page_404),
    .data = html_page_404
};


static int imagick_set_header(imagick_cache_t *cache)
{
    if (!cache) {
        return -1;
    }
    switch (cache->http_code) {
        case 200:
            // todo
            break;

        case 400:
            smart_str_appends(&cache->header, "HTTP/1.1 400 Bad Request\r\n");
            smart_str_appends(&cache->header, "Content-Type: text/html\r\n");
            smart_str_appends(&cache->header, "Content-Length: ");
            smart_str_append_long(&cache->header, cache->size);
            smart_str_appends(&cache->header, "\r\nServer: Imagick\r\n\r\n");
            break;
    }
}

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

static void imagick_send_header(imagick_connection_t *conn)
{
    int n, nwrite;
    imagick_cache_t *c = conn->cache;

    n = c->header.len - c->wpos;
    while (n > 0) {
        nwrite = write(conn->sockfd, c->header.c + c->wpos, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                imagick_log_error("send http header failure (%d)", errno);
                imagick_connection_free(conn);
                return;
            }
            break;
        }
        c->wpos += nwrite;
        n -= nwrite;
    }
    if (n == 0) {
        // header already sent
        conn->status = IC_STATUS_SEND_BODY;
        c->wpos = 0;
    } else {
        return;
    }
}

static void imagick_send_body(imagick_connection_t *conn)
{
    int n, nwrite;
    imagick_cache_t *c = conn->cache;

    n = c->size - c->wpos;
    while (n > 0) {
        nwrite = write(conn->sockfd, c->data + c->wpos, n);
        if (nwrite < n) {
            if (nwrite == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                imagick_log_error("send http body failure (%d)", errno);
                imagick_connection_free(conn);
                return;
            }
            break;
        }
        c->wpos += nwrite;
        n -= nwrite;
    }
    if (n == 0) {
        // header already sent
        conn->status = IC_STATUS_FINISH;
        c->wpos = 0;
    } else {
        return;
    }

}

static void imagick_sock_send_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    imagick_connection_t *conn = arg;
    imagick_cache_t *c = conn->cache;
    imagick_log_debug("send fd:%d, pid:%d", conn->sockfd, getpid());

    int n, nwrite;

    if (conn->status == IC_STATUS_SEND_HEADER) {
        imagick_send_header(conn);
    }

    if (conn->status == IC_STATUS_SEND_BODY) {
        imagick_send_body(conn);
    }

    if (conn->status == IC_STATUS_FINISH) {
        loop->del_event(loop, conn->sockfd, IE_READABLE | IE_WRITABLE);
        imagick_connection_free(conn);
    }
}

static int imagick_parse_http(imagick_connection_t **c)
{
    imagick_connection_t *cc = *c;
    cc->cache;

    int retval;

    retval = http_parser_execute(&cc->hp, &hp_setting, cc->rbuf.c, cc->rbuf.len);
    if (retval == 0) {
        cc->cache = &cache_page_400;
        return 0;
    }

    if (cc->filename.len == 0) {
        cc->cache = &cache_page_400;
        return 0;
    }

    // check file exists
    smart_str full_path = { 0 };
    if (-1 == imagick_get_full_path(&full_path, cc->filename.c)) {
        imagick_log_error("Failed imagick_get_full_path");
        // todo 500 page
        return 0;
    }

    if (! imagick_file_is_exists(full_path.c)) {
        cc->cache = &cache_page_404;
        return 0;
    }

    imagick_lock_lock(&main_ctx->cache_mutex);

    imagick_cache_t *r = NULL;

    int find = imagick_hash_find(main_ctx->cache_ht, cc->filename.c,
            cc->filename.len, (void **)&r);
    if (find == IMAGICK_HASH_OK) {
        cc->cache = r;
        CACHE_REF(cc->cache);
    } else {
        cc->cache = ncx_slab_alloc(main_ctx->pool, sizeof(imagick_cache_t));
    }

    imagick_lock_unlock(&main_ctx->cache_mutex);

    imagick_log_debug("filename:%s\n", cc->filename.c);

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
