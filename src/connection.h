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

enum CACHE_TYPE {
    CACHE_TYPE_HTML,
    CACHE_TYPE_BIN
};

typedef struct imagick_connection_s imagick_connection_t;
typedef struct imagick_cache_s imagick_cache_t;

struct imagick_cache_s {
    enum CACHE_TYPE type;
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
