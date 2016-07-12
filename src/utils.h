#pragma once
#include "smart_str.h"

int imagick_set_nonblocking(int fd);
int imagick_file_is_exists(char *filename);

/* path */
int imagick_path_format(smart_str *dst, char *src);

/* The last argument must be NULL */
int imagick_path_join(smart_str *dst, char *src, ...);

const char *imagick_get_content_type(char *ext_name);
char *imagick_get_file_extension(char *filename);
