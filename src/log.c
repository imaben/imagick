#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "log.h"

static char *imagick_error_titles[IMAGICK_LOG_LEVEL_ERROR + 1] = {
    "DEBUG", "NOTICE", "WARN", "ERROR"
};

static int  imagick_log_fd         = 0;
static int  imagick_log_mark       = IMAGICK_LOG_LEVEL_DEBUG;
static int  imagick_log_initialize = 0;
static char imagick_log_buffer[4096];


int imagick_init_log(char *file, int mark)
{
    if (imagick_log_initialize) {
        return 0;
    }

    if (mark < IMAGICK_LOG_LEVEL_DEBUG
            || mark > IMAGICK_LOG_LEVEL_ERROR)
    {
        return -1;
    }

    if (file) {
        imagick_log_fd = open(file, O_WRONLY | O_CREAT | O_APPEND , 0666);
        if (!imagick_log_fd) {
            return -1;
        }

    } else {
        dup2(imagick_log_fd, STDERR_FILENO);
    }

    imagick_log_mark = mark;
    imagick_log_initialize = 1;

    return 0;
}


void imagick_log(int level, char *fmt, ...)
{
    va_list al;
    time_t current;
    struct tm *dt;
    int off1, off2;

    if (!imagick_log_initialize
            || level < imagick_log_mark
            || level > IMAGICK_LOG_LEVEL_ERROR)
    {
        return;
    }

    /* Get current date and time */
    time(&current);
    dt = localtime(&current);

    off1 = sprintf(imagick_log_buffer,
            "[%04d-%02d-%02d %02d:%02d:%02d] %s: ",
            dt->tm_year + 1900,
            dt->tm_mon + 1,
            dt->tm_mday,
            dt->tm_hour,
            dt->tm_min,
            dt->tm_sec,
            imagick_error_titles[level]);

    va_start(al, fmt);
    off2 = vsprintf(imagick_log_buffer + off1, fmt, al);
    va_end(al);

    imagick_log_buffer[off1 + off2] = '\n';

    write(imagick_log_fd, imagick_log_buffer, off1 + off2 + 1);
}

void imagick_destroy_log()
{
    if (!imagick_log_initialize) {
        return;
    }

    if (imagick_log_fd && imagick_log_fd != STDERR_FILENO) {
        close(imagick_log_fd);
    }
}

int imagick_log_get_fd()
{
    return imagick_log_fd;
}
