#include "utils.h"
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

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
        if (dst->len > 0 && dst->c[dst->len - 1] != '/') {
            smart_str_appendc(dst, '/');
        }
        smart_str_appends(dst, s);
        s = va_arg(al, char*);
    }
    va_end(al);
    return 0;
}

const char *content_type[] = {
	".jpeg",
    "image/jpeg",
	".jpg",
    "image/jpeg",
	".png",
    "image/png",
	".gif",
    "image/gif",
    NULL
};
const char *default_content_type = "image/jpeg";

const char *imagick_get_content_type(char *ext_name)
{
    if (ext_name == NULL || strlen(ext_name) == 0) {
        return default_content_type;
    }

    int i = 0;
    for (;content_type[i] != NULL;) {
        /* It's over, return default content type */
        if (strlen(content_type[i]) == 0)
            return default_content_type;

        if (strlen(ext_name) >= strlen(content_type[i]))
            goto next;

        if (strcmp(ext_name, content_type[i]) == 0) {
            return content_type[i + 1];
        }
next:
        i += 2;
    }
    return default_content_type;
}

char *imagick_get_file_extension(char *filename)
{
    char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) {
        return NULL;
    }
    return dot;
}

int imagick_get_file_size(char *filename)
{
    struct stat s;
    stat(filename, &s);
    return s.st_size;
}
