#pragma once

#include "imagick.h"
#include <unistd.h>

#define IMAGICK_PROCESS_CMD_INIT 0
#define IMAGICK_PROCESS_CMD_EXIT 1

typedef struct {
    pid_t    pid;
    int      slot;
    int      fd;
} imagick_channel_t;

typedef struct {
    int  cmd;
    /* reserved */
    int  len;
    char data[0];
} imagick_channel_cmd_t;

int imagick_write_channel(imagick_channel_t *ch, imagick_channel_cmd_t *cmd);
void imagick_close_channel(int *sockfd);
