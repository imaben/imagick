#pragma once

#include "imagick.h"
#include <unistd.h>

typedef struct {
    uint_t   command;
    pid_t    pid;
    int      slot;
    int      fd;
} imagick_channel_t;

int imagick_write_channel(int sockfd, imagick_channel_t *ch, size_t size);
void imagick_close_channel(int *sockfd);
