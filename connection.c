#include "connection.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

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
