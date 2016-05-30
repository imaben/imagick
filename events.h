#pragma once

#include "process.h"

#define IE_NONE 0
#define IE_READABLE 1
#define IE_WRITABLE 2

typedef (void *imagick_event_handler)(imagick_event_loop_t event_loop, int fd, void *arg);

typedef struct imagick_event_s imagick_event_t;
typedef struct imagick_event_fired_s imagick_event_fired_t;
typedef struct imagick_event_loop_s imagick_event_loop_t;

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
    int maxfd;
    int setsize;
    imagick_event_t *events;
    imagick_event_fired_t *fired;
};


void imagick_add_event(imagick_worker_ctx_t *ctx, int fd, int flags);
void imagick_delete_event(imagick_worker_ctx_t *ctx, int fd, int flags);
void imagick_modify_event(imagick_worker_ctx_t *ctx, int fd, int flags);


