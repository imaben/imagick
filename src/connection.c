#include "connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "log.h"

int imagick_listen_socket(char *addr, int port)
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
#if !defined(TCP_NOPUSH) && defined(TCP_NODELAY)
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));
#endif

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

int imagick_connection_init()
{
    return 0;
}

imagick_connection_t *imagick_connection_create(int sockfd)
{
    imagick_connection_t *conn;
    conn = (imagick_connection_t *)malloc(sizeof(*conn));
    if (conn == NULL) {
        imagick_log_error("Cannot alloc memory to create connection");
        return NULL;
    }

    conn->sockfd = sockfd;
    conn->status = IC_STATUS_WAIT_RECV;

    http_parser_init(&conn->hp, HTTP_REQUEST);
    conn->hp.data = conn;
    memset(&conn->rbuf, 0, sizeof(conn->rbuf));
    memset(&conn->filename, 0, sizeof(conn->filename));
    return conn;
}

void imagick_connection_free(imagick_connection_t *c)
{
    smart_str_free(&c->rbuf);
    smart_str_free(&c->filename);
    if (c->sockfd) close(c->sockfd);
    free(c);
}
