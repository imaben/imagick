#include "events.h"

void imagick_add_event(imagick_worker_ctx_t *ctx, int fd, int flags)
{
    struct epoll_event ev;
    ev.events = flags;
    ev.data.fd = fd;
    epoll_ctl(ctx->epollfd, EPOLL_CTL_ADD, fd, &ev);
}

void imagick_delete_event(imagick_worker_ctx_t *ctx, int fd, int flags)
{
    struct epoll_event ev;
    ev.events = flags;
    ev.data.fd = fd;
    epoll_ctl(ctx->epollfd, EPOLL_CTL_DEL, fd, &ev);
}

void imagick_modify_event(imagick_worker_ctx_t *ctx, int fd, int flags)
{
    struct epoll_event ev;
    ev.events = flags;
    ev.data.fd = fd;
    epoll_ctl(ctx->epollfd, EPOLL_CTL_MOD, fd, &ev);
}

