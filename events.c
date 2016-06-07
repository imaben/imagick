#include "events.h"
#include <stdlib.h>
#include "log.h"

imagick_event_loop_t *imagick_create_event_loop(int setsize)
{
    imagick_event_loop_t *loop = NULL;
    int i;

    loop = malloc(sizeof(imagick_event_loop_t));
    if (NULL == loop) {
        imagick_log_error("failed to create event loop");
        goto fail;
    }

    loop->epollfd = epoll_create(1024);
    if (-1 == loop->epollfd) {
        imagick_log_error("failed to create event instance");
        goto fail;
    }
    loop->events = malloc(sizeof(imagick_event_t) * setsize);
    loop->fired = malloc(sizeof(imagick_event_fired_t) * setsize);
    if (NULL == loop->events || NULL == loop->fired) {
        imagick_log_error("failed to create event loop");
        goto fail;
    }
    loop->setsize = setsize;
    loop->stop = 0;

    for (i = 0; i < setsize; i++) {
        loop->events[i].mask = IE_NONE;
    }

fail:
    return loop;
}

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

