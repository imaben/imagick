#pragma once

#define IMAGICK_CACHE_MIN_SIZE (1024 * 1024 * 10)  /* 10MB */
#define IMAGICK_MAX_PROCESSES 1024

typedef unsigned int uint_t;

typedef struct imagick_setting_s imagick_setting_t;

struct imagick_setting_s {
    char *host;
    uint_t port;
    uint_t processes;
    char *logfile;
    int logmark;
    char *imgroot;
    uint_t daemon:1;
};

typedef struct imagick_main_ctx_s imagick_main_ctx_t;

struct imagick_main_ctx_s {
    int sockfd;
};


extern imagick_main_ctx_t *main_ctx;
