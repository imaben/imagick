#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "imagick.h"
#include "daemon.h"
#include "process.h"

imagick_setting_t __setting = {
    .host      = "0.0.0.0",
    .port      = 80,
    .processes = 10,
    .daemon    = 0
}, *imagick_setting = &__setting;

static struct option options[] = {
    {"host",        required_argument,   0,   'h'},
    {"port",        required_argument,   0,   'p'},
    {"processes",   required_argument,   0,   'c'},
    {"daemon",      no_argument,         0,   'd'},
    {0, 0, 0, 0}
};

static void imagick_parse_options(int argc, char **argv)
{
    int c;
    while ((c = getopt_long(argc, argv, "h:p:c:d", options, NULL)) != -1) {
        switch (c) {
            case 'h':
                imagick_setting->host = strdup(optarg);
                break;
            case 'p':
                imagick_setting->port = atoi(optarg);
                break;
            case 'c':
                imagick_setting->processes = atoi(optarg);
                if (imagick_setting->processes < 1) {
                    imagick_setting->processes = 1;
                }
                if (imagick_setting->processes > IMAGICK_MAX_PROCESSES) {
                    imagick_setting->processes = IMAGICK_MAX_PROCESSES;
                }
                break;
            case 'd':
                imagick_setting->daemon = 1;
                break;
            default:
                break;
        }
    }
}

int main (int argc, char **argv)
{
    imagick_argc = argc;
    imagick_argv = argv;
    // Ignore SIGPIPE signal
    signal(SIGPIPE, SIG_IGN);

    imagick_parse_options(argc, argv);

    if (imagick_setting->daemon) {
        imagick_daemonize();
    }
    imagick_master_process_start(imagick_setting);
    return 0;
}
