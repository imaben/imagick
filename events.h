#pragma once

#include "process.h"

#define IE_NONE 0
#define IE_READABLE 1
#define IE_WRITABLE 2

#define IE_DEFAULT_FD_COUNT 256

typedef struct imagick_event_s imagick_event_t;
typedef struct imagick_event_fired_s imagick_event_fired_t;
typedef struct imagick_event_loop_s imagick_event_loop_t;

typedef void (*imagick_event_handler)(imagick_event_loop_t *event_loop, int fd, void *arg);

struct imagick_event_s {
    int mask; /* once of IE_(READABLE|WRITABLE) */
    imagick_event_handler read_proc;
    imagick_event_handler write_proc;
    void *arg;
};

struct imagick_event_fired_s {
    int fd;
    int mask;
};

struct imagick_event_loop_s {
    int epollfd;
    int maxfd;
    int setsize;
    imagick_event_t *events;
    imagick_event_fired_t *fired;
    struct epoll_event *events_active;
    int stop;

    int (*add_event)(imagick_event_loop_t *loop, int fd, int mask, imagick_event_handler proc, void *arg);
    void (*del_event)(imagick_event_loop_t *loop, int fd, int delmask);
    int (*dispatch)(imagick_event_loop_t *loop);
};

imagick_event_loop_t *imagick_event_loop_create(int setsize);
void imagick_event_loop_free(imagick_event_loop_t *loop);
void imagick_event_loop_stop(imagick_event_loop_t *loop);

int imagick_add_event(imagick_event_loop_t *loop, int fd, int mask, imagick_event_handler proc, void *arg);
void imagick_delete_event(imagick_event_loop_t *loop, int fd, int delmask);
int imagick_event_dispatch(imagick_event_loop_t *loop);

