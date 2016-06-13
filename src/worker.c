#include "worker.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netdb.h>
#include "log.h"
#include "utils.h"

static void imagick_http_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
}

void imagick_main_sock_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    imagick_log_debug("new connection %d\n", fd);
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
        imagick_set_nonblocking(connfd);
        loop->add_event(loop, connfd, IE_READABLE, imagick_http_handler, NULL);
    }
}
