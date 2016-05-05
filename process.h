#pragma once

#include "imagick.h"

extern int imagick_argc;
extern char **imagick_argv;

#define IMAGICK_CMD_OPEN_CHANNEL   1
#define IMAGICK_CMD_CLOSE_CHANNEL  2
#define IMAGICK_CMD_QUIT           3
#define IMAGICK_CMD_TERMINATE      4

typedef void (*imagick_spawn_proc_pt) (void *data);

typedef struct {
    pid_t pid;
    int status;
    int channel[2];
    imagick_spawn_proc_pt proc;
    void *data;
    char *name;

    unsigned exiting:1;
    unsigned exited:1;
} imagick_process_t;

void imagick_master_process_start(imagick_setting_t *setting);
