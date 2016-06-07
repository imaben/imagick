#pragma once

#include <stdio.h>

#define IMAGICK_LOG_LEVEL_DEBUG   0
#define IMAGICK_LOG_LEVEL_NOTICE  1
#define IMAGICK_LOG_LEVEL_WARN   2
#define IMAGICK_LOG_LEVEL_ERROR   3

int imagick_init_log(char *file, int mark);
void imagick_log(int level, char *fmt, ...);
void imagick_destroy_log();
FILE *imagick_log_get_fp();

#define imagick_log_debug(fmt, ...) \
    imagick_log(IMAGICK_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#define imagick_log_notice(fmt, ...) \
    imagick_log(IMAGICK_LOG_LEVEL_NOTICE, fmt, ##__VA_ARGS__)

#define imagick_log_warn(fmt, ...) \
    imagick_log(IMAGICK_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)

#define imagick_log_error(fmt, ...) \
    imagick_log(IMAGICK_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
