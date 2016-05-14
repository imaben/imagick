#pragma once
#include  "http_parser.h"

typedef struct imagick_connection_s imagick_connection_t;

struct imagick_connection_s {
    int sockfd;
    int http_code;
    struct http_parser hp;
};

int imagick_listen_socket(char *addr, int port);
int imagick_connection_init();
imagick_connection_t *imagick_connection_create(int sockfd);
