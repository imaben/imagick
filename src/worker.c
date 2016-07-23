#include "worker.h"
#include "imagick.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

static inline int imagick_get_full_path(smart_str *dst, char *path)
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

static char html_page_500[] = "<html>"
"<head><title>500 Internal Server Error</title></head>"
"<body bgcolor=\"white\">"
"<center><h1>500 Internal Server Error</h1></center>"
"<hr><div align=\"center\">Imagick " IMAGICK_VERSION "</div>"
"</body>"
"</html>";

static char header_page_400[] = "HTTP/1.1 400 Bad Request\r\n"
"Content-Type: text/html\r\n"
"Content-Length: %d\r\n"
"Server: Imagick\r\n\r\n";

static char header_page_404[] = "HTTP/1.1 404 Not Found\r\n"
"Content-Type: text/html\r\n"
"Content-Length: %d\r\n"
"Server: Imagick\r\n\r\n";

static char header_page_500[] = "HTTP/1.1 500 Internal Server Error\r\n"
"Content-Type: text/html\r\n"
"Content-Length: %d\r\n"
"Server: Imagick\r\n\r\n";

static imagick_cache_t *imagick_get_internal_page_cache(int http_code)
{
    static imagick_cache_t *page_400 = NULL;
    static imagick_cache_t *page_404 = NULL;
    static imagick_cache_t *page_500 = NULL;
    switch (http_code) {
    case 400:
        if (page_400) {
            return page_400;
        }
        page_400 = malloc(sizeof(imagick_cache_t));
        page_400->flag = CACHE_TYPE_HTML | CACHE_TYPE_INTERNAL;
        page_400->ref_count = 0;
        page_400->http_code = 400;
        sprintf(page_400->header, header_page_400, sizeof(html_page_400));
        page_400->size =  sizeof(html_page_400);
        page_400->data = html_page_400;
        return page_400;

    case 404:
        if (page_404) {
            return page_404;
        }
        page_404 = malloc(sizeof(imagick_cache_t));
        page_404->flag = CACHE_TYPE_HTML | CACHE_TYPE_INTERNAL;
        page_404->ref_count = 0;
        page_404->http_code = 404;
        sprintf(page_404->header, header_page_404, sizeof(html_page_404));
        page_404->size =  sizeof(html_page_404);
        page_404->data = html_page_404;
        return page_404;

    case 500:
        if (page_500) {
            return page_500;
        }
        page_500 = malloc(sizeof(imagick_cache_t));
        page_500->flag = CACHE_TYPE_HTML | CACHE_TYPE_INTERNAL;
        page_500->ref_count = 0;
        page_500->http_code = 500;
        sprintf(page_500->header, header_page_500, strlen(html_page_500) + 1);
        page_500->size =  strlen(html_page_500) + 1;
        page_500->data = html_page_500;
        return page_500;

    default:
        return NULL;
    }
}

static int imagick_format_header(char *header, const char *content_type, int size)
{
    static char header_tpl[] = "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Server: Imagick\r\n\r\n";
    sprintf(header, header_tpl, content_type, size);
    return 0;
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

    n = strlen(c->header) - conn->wpos;
    while (n > 0) {
        nwrite = write(conn->sockfd, c->header + conn->wpos, n);
        if (nwrite < n) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                imagick_log_error("send http header failure (%d)", errno);
                imagick_connection_free(conn);
                return;
            }
            break;
        }
        conn->wpos += nwrite;
        n -= nwrite;
    }
    if (n <= 0) {
        // header already sent
        conn->status = IC_STATUS_SEND_BODY;
        conn->wpos = 0;
    } else {
        return;
    }
}

static void imagick_send_body(imagick_connection_t *conn)
{
    int n, nwrite;
    imagick_cache_t *c = conn->cache;

    n = c->size - conn->wpos;
    while (n > 0) {
        nwrite = write(conn->sockfd, c->data + conn->wpos, n);
        if (nwrite < n) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                imagick_log_error("send http body failure (%d)", errno);
                imagick_connection_free(conn);
                return;
            }
            break;
        }
        conn->wpos += nwrite;
        n -= nwrite;
    }
    if (n <= 0) {
        // header already sent
        conn->status = IC_STATUS_FINISH;
        conn->wpos = 0;
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

static inline int imagick_find_cache(char *key, int key_len, imagick_cache_t **c)
{
    imagick_lock_lock(&main_ctx->cache_mutex);
    int find = imagick_hash_find(main_ctx->cache_ht, key, key_len, (void **)c);
    imagick_lock_unlock(&main_ctx->cache_mutex);
    return find;
}

static inline void imagick_insert_cache_ht(imagick_connection_t *conn)
{
    imagick_lock_lock(&main_ctx->cache_mutex);
    imagick_hash_insert(main_ctx->cache_ht, conn->filename.c, conn->filename.len, conn->cache, 1);
    imagick_lock_unlock(&main_ctx->cache_mutex);
}

static int imagick_parse_http(imagick_connection_t **c)
{
    imagick_connection_t *cc = *c;
    cc->cache;

    int retval;

    retval = http_parser_execute(&cc->hp, &hp_setting, cc->rbuf.c, cc->rbuf.len);
    if (retval == 0) {
        cc->cache = imagick_get_internal_page_cache(400);
        return 0;
    }

    if (cc->filename.len == 0) {
        cc->cache = imagick_get_internal_page_cache(400);
        return 0;
    }

    // check file exists
    smart_str full_path = { 0 };
    if (-1 == imagick_get_full_path(&full_path, cc->filename.c)) {
        imagick_log_error("Failed imagick_get_full_path");
        cc->cache = imagick_get_internal_page_cache(500);
        return 0;
    }
    smart_str_0(&full_path);

    if (! imagick_file_is_exists(full_path.c)) {
        imagick_log_warn("Request %s (404)", full_path.c);
        cc->cache = imagick_get_internal_page_cache(404);
        return 0;
    }


    imagick_cache_t *r = NULL;
    int find = imagick_find_cache(cc->filename.c, cc->filename.len, &r);
    if (find == IMAGICK_HASH_OK) {
        imagick_log_debug("hit cache (%s)", cc->filename.c);
        cc->cache = r;
        CACHE_REF(cc->cache);
        return 0;
    }

    FILE *fp = fopen(full_path.c, "rb");
    if (!fp) {
        imagick_log_error("Failed to open file (%s)", full_path);
        cc->cache = imagick_get_internal_page_cache(500);
        return 0;
    }

    cc->cache = ncx_slab_alloc(main_ctx->pool, sizeof(imagick_cache_t));
    if (NULL == cc->cache) {
        cc->cache = imagick_get_internal_page_cache(500);
        imagick_log_error("Failed to alloc memory");
        return 0;
    }

    const char *content_type = imagick_get_content_type(
            imagick_get_file_extension(full_path.c));
    int fsize = imagick_get_file_size(full_path.c);

    cc->cache->flag = CACHE_TYPE_BIN;
    cc->cache->ref_count = 1;
    cc->cache->http_code = 200;
    cc->cache->size = fsize;
    imagick_format_header(cc->cache->header, content_type, fsize);
    cc->cache->data = ncx_slab_alloc(main_ctx->pool, fsize);
    fread(cc->cache->data, fsize, 1, fp);
    fclose(fp);

    imagick_log_debug("filename:%s\n", cc->filename.c);

    imagick_insert_cache_ht(cc);
    return 0;
}

static int imagick_http_recv_complete(char *content,int len)
{
    if (len > 4) {
        if (content[len - 1] == '\n' &&
            content[len - 2] == '\r' &&
            content[len - 3] == '\n' &&
            content[len - 4] == '\r') {
            return 0;
        }
    }
    return -1;
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

    for (;;) {
        nread = read(c->sockfd, buf, 128);
        if (nread < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                imagick_log_warn("Failed to read data %d", fd);
                goto fatal;
            }
            break;
        } else if (nread == 0) {
            goto fatal;
        }
        smart_str_appendl(&c->rbuf, buf, nread);
    }

    if (imagick_http_recv_complete(c->rbuf.c, c->rbuf.len) == 0) {
        // read complete
        smart_str_0(&c->rbuf);
        if (imagick_parse_http(&c) == -1) {
            goto fatal;
        }

        c->status = IC_STATUS_SEND_HEADER;
        loop->add_event(loop, c->sockfd, IE_WRITABLE, imagick_sock_send_handler, c);
    }
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
