#include "utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>

int imagick_set_nonblocking(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0 ||
            fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

int imagick_file_is_exists(char *filename)
{
   return access((const char *)filename, F_OK) == 0;
}

int imagick_path_format(smart_str *dst, char *src)
{
    assert(dst);
    assert(src);
    if (strlen(src) == 0) {
        return 0;
    }
    smart_str_appends(dst, src);
    if (src[strlen(src) - 1] != '/') {
        smart_str_appendc(dst, '/');
    }
    smart_str_0(dst);
    return 0;
}

int imagick_path_join(smart_str *dst, char *src, ...)
{
    assert(dst);
    assert(src);
    va_list al;
    char *s = src;

    va_start(al, src);
    while (s) {
        smart_str_appends(dst, s);
        if (s[strlen(s) - 1] != '/') {
            smart_str_appendc(dst, '/');
        }
        s = va_arg(al, char*);
    }
    va_end(al);
    return 0;
}
