#pragma once

#define IMAGICK_CACHE_MIN_SIZE (1024 * 1024 * 10)  /* 10MB */
#define IMAGICK_MAX_PROCESSES 1024

typedef unsigned int uint_t;

typedef struct {
    char *host;
    uint_t port;
    uint_t processes;
    uint_t daemon:1;
} imagick_setting_t;
