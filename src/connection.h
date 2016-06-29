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

typedef struct imagick_connection_s imagick_connection_t;
typedef struct imagick_cache_s imagick_cache_t;

struct imagick_cache_s {
    int size;
    void *data;
};

struct imagick_connection_s {
    int sockfd;
    int http_code;
    int status;
    struct http_parser hp;
    smart_str rbuf; /* read buffer */
    smart_str wbuf; /* write buffer */
    int wpos; /* pos of header or body */
    imagick_cache_t *cache;

    smart_str filename;
};

int imagick_listen_socket(char *addr, int port);
int imagick_connection_init();
imagick_connection_t *imagick_connection_create(int sockfd);
void imagick_connection_free(imagick_connection_t *c);
