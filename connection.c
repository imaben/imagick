#include "connection.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
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
    conn->http_code = 200;
    http_parser_init(&conn->hp, HTTP_REQUEST);

    return conn;
}
