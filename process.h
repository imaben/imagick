#pragma once

#include <sys/types.h>
#include "imagick.h"
#include "events.h"

extern int imagick_argc;
extern char **imagick_argv;

#define IMAGICK_CMD_OPEN_CHANNEL   1
#define IMAGICK_CMD_CLOSE_CHANNEL  2
#define IMAGICK_CMD_QUIT           3
#define IMAGICK_CMD_TERMINATE      4

typedef void (*imagick_spawn_proc_pt) (void *data);

typedef struct imagick_process_s imagick_process_t;
typedef struct imagick_worker_ctx_s imagick_worker_ctx_t;

struct imagick_process_s {
    pid_t pid;
    int status;
    int channel[2];
    imagick_spawn_proc_pt proc;
    void *data;
    char *name;

    unsigned exiting:1;
    unsigned exited:1;
};

struct imagick_worker_ctx_s {
    pid_t pid;
    int rwfd;
    struct imagick_event_loop_s *loop;
};

void imagick_master_process_start(imagick_setting_t *setting);
