#pragma once

#include "ncx_slab.h"
#include "hash.h"
#include "lock.h"
#include "list.h"

#define IMAGICK_CACHE_MIN_SIZE (1024 * 1024 * 10)  /* 10MB */
#define IMAGICK_MAX_PROCESSES 1024

#define IMAGICK_VERSION "0.0.1"

typedef unsigned int uint_t;

typedef struct imagick_setting_s imagick_setting_t;

struct imagick_setting_s {
    char *host;
    uint_t port;
    uint_t processes;
    char *logfile;
    int logmark;
    char *imgroot;
    int max_cache;
    uint_t daemon:1;
};

typedef struct imagick_ctx_s imagick_ctx_t;

struct imagick_ctx_s {
    int               sockfd;
    imagick_setting_t *setting;
    ncx_slab_pool_t   *pool;
    imagick_hash_t    *cache_ht;  /* Hash table of images */
    imagick_lock_t    cache_mutex;

    struct list_head  *lru;
    imagick_lock_t    lru_mutex;
};


extern imagick_ctx_t *main_ctx;
