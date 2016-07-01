#include "utils.h"
#include <stdio.h>

int main (int argc, char **argv)
{
    /* path format */
    smart_str dst_f = {0};
    char *path = "/opt/imagick";
    imagick_path_format(&dst_f, path);
    printf("formated path:%s\n", dst_f.c);

    /* path join */
    smart_str dst_j = {0};
    char *path1 = "p1/";
    char *path2 = "p2";
    imagick_path_join(&dst_j, path, path1, path2);
    printf("formated path(join):%s\n", dst_j.c);
    return 0;
}
