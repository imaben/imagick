#include "events.h"
#include <sys/epoll.h>
#include <stdlib.h>
#include <unistd.h>
#include "log.h"

imagick_event_loop_t *imagick_event_loop_create(int setsize)
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
    if (NULL == loop->events) {
        imagick_log_error("failed to create events");
        close(loop->epollfd);
        goto fail;
    }

    loop->fired = malloc(sizeof(imagick_event_fired_t) * setsize);
    if (NULL == loop->fired) {
        imagick_log_error("failed to create fired");
        free(loop->events);
        close(loop->epollfd);
        goto fail;
    }

    loop->events_active = malloc(sizeof(struct epoll_event) * setsize);
    if (NULL == loop->events_active) {
        imagick_log_error("failed to create events_active");
        free(loop->events);
        free(loop->fired);
        close(loop->epollfd);
        goto fail;
    }

    loop->setsize = setsize;
    loop->stop = 0;

    loop->add_event = imagick_add_event;
    loop->del_event = imagick_delete_event;
    loop->dispatch = imagick_event_dispatch;

    for (i = 0; i < setsize; i++) {
        loop->events[i].mask = IE_NONE;
    }
    return loop;

fail:
    return NULL;
}

void imagick_event_loop_free(imagick_event_loop_t *loop)
{
    free(loop->events);
    free(loop->fired);
    free(loop->events_active);
    free(loop);
}

void imagick_event_loop_stop(imagick_event_loop_t *loop)
{
    loop->stop = 1;
}

int imagick_add_event(imagick_event_loop_t *loop, int fd, int mask,
        imagick_event_handler proc, void *arg)
{
    if (fd >= loop->setsize) {
        return -1;
    }

    imagick_event_t *ev = &loop->events[fd];
    ev->mask |= mask;
    if (mask & IE_READABLE) {
        ev->read_proc = proc;
    }
    if (mask & IE_WRITABLE) {
        ev->write_proc = proc;
    }
    ev->arg = arg;

    struct epoll_event ee = {0};
    int op = loop->events[fd].mask == IE_NONE ?
            EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    ee.data.fd = fd;

    if (epoll_ctl(loop->epollfd, op, fd, &ee) == -1) {
        return -1;
    }

    if (fd > loop->maxfd) {
        loop->maxfd = fd;
    }
    return 0;
}

void imagick_delete_event(imagick_event_loop_t *loop, int fd, int delmask)
{
    if (fd > loop->setsize) {
        return;
    }

    imagick_event_t *ev = &loop->events[fd];
    if (ev->mask == IE_NONE) {
        return;
    }

    int mask = loop->events[fd].mask & (~delmask);

    struct epoll_event ee;
    ee.events = 0;
    if (mask & IE_READABLE) {
        ee.events |= EPOLLIN;
    }
    if (mask & IE_WRITABLE) {
        ee.events |= EPOLLOUT;
    }
    ee.data.fd = fd;
    if (mask != IE_NONE) {
        epoll_ctl(loop->epollfd, EPOLL_CTL_DEL, fd, &ee);
    } else {
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         * EPOLL_CTL_DEL. */
        epoll_ctl(loop->epollfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

int imagick_event_poll(imagick_event_loop_t *loop)
{
    int retval, events_num;
    retval = epoll_wait(loop->epollfd, loop->events_active, loop->setsize, -1);
    if (retval > 0) {
        int j;
        events_num = retval;
        for (j = 0; j < events_num; j++) {
            int mask = 0;
            struct epoll_event *e = loop->events_active + j;

            if (e->events & EPOLLIN) mask |= IE_READABLE;
            if (e->events & EPOLLOUT) mask |= IE_WRITABLE;
            if (e->events & EPOLLERR) mask |= IE_WRITABLE;
            if (e->events & EPOLLHUP) mask |= IE_WRITABLE;

            loop->fired[j].fd = e->data.fd;
            loop->fired[j].mask = mask;
        }
    }
    return events_num;
}

int imagick_event_dispatch(imagick_event_loop_t *loop)
{
    int events_num = 0, i;
    events_num = imagick_event_poll(loop);
    while (!loop->stop) {
        for (i = 0; i < events_num; i++) {
            imagick_event_t *ev = &loop->events[loop->fired[i].fd];
            int mask = loop->fired[i].mask;
            int fd = loop->fired[i].fd;

            if (ev->mask & mask & IE_READABLE) {
                ev->read_proc(loop, fd, ev->arg);
            }

            if (ev->mask & mask & IE_WRITABLE) {
                ev->write_proc(loop, fd, ev->arg);
            }
        }
    }
    return 0;
}
