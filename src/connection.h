#pragma once
#include  "http_parser.h"
#include  "smart_str.h"

enum IC_STATUS {
    IC_STATUS_WAIT_RECV = 0,
    IC_STATUS_RECEIVING,
    IC_STATUS_SEND_HEADER,
    IC_STATUS_SEND_BODY,
    IC_STATUS_FINISH
};

enum CACHE_FLAGS {
    CACHE_TYPE_HTML     = 1 << 0,
    CACHE_TYPE_BIN      = 1 << 1,
    CACHE_TYPE_INTERNAL = 1 << 2
};

#define CACHE_REF_OP(cache, op) do { \
    imagick_cache_t *t = (imagick_cache_t *)cache; \
    if (!(t->flag & CACHE_TYPE_INTERNAL)) { \
        op ? t->ref_count++ : t->ref_count--; \
    } \
} while (0)

#define CACHE_REF(cache) CACHE_REF_OP(cache, 1)
#define CACHE_UNREF(cache) CACHE_REF_OP(cache, 0)

typedef struct imagick_connection_s imagick_connection_t;
typedef struct imagick_cache_s imagick_cache_t;

struct imagick_cache_s {
    int flag;
    int ref_count;
    int http_code;
    smart_str header; /* write buffer */
    int wpos; /* pos of header or body */
    int size;
    void *data;
};

struct imagick_connection_s {
    int sockfd;
    int status;
    struct http_parser hp;
    smart_str rbuf; /* read buffer */
    smart_str filename;
    imagick_cache_t *cache;
};

int imagick_listen_socket(char *addr, int port);
int imagick_connection_init();
imagick_connection_t *imagick_connection_create(int sockfd);
void imagick_connection_free(imagick_connection_t *c);
