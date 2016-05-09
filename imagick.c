#include <stdio.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "imagick.h"
#include "daemon.h"
#include "process.h"
#include "log.h"

imagick_setting_t __setting = {
    .host      = "0.0.0.0",
    .port      = 80,
    .processes = 10,
    .logfile   = "/tmp/.imagick.log",
    .logmark   = IMAGICK_LOG_LEVEL_DEBUG,
    .daemon    = 0
}, *imagick_setting = &__setting;

static struct option options[] = {
    {"host",        required_argument,   0,   'h'},
    {"port",        required_argument,   0,   'p'},
    {"processes",   required_argument,   0,   'c'},
    {"logfile",     required_argument,   0,   'l'},
    {"logmark",     required_argument,   0,   'm'},
    {"daemon",      no_argument,         0,   'd'},
    {0, 0, 0, 0}
};

#define fail(fmt, ...) do { \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
    exit(1); \
} while (0)

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
            case 'l':
                imagick_setting->logfile = strdup(optarg);
                break;
            case 'd':
                imagick_setting->daemon = 1;
                break;
            case 'm':
                if (strcasecmp(optarg, "debug") == 0) {
                    imagick_setting->logmark = IMAGICK_LOG_LEVEL_DEBUG;
                } else if (strcasecmp(optarg, "notice") == 0) {
                    imagick_setting->logmark = IMAGICK_LOG_LEVEL_NOTICE;
                } else if (strcasecmp(optarg, "warn") == 0) {
                    imagick_setting->logmark = IMAGICK_LOG_LEVEL_WARN;
                } else if (strcasecmp(optarg, "error") == 0) {
                    imagick_setting->logmark = IMAGICK_LOG_LEVEL_ERROR;
                } else {
                    fail("Invalid logmark argument\n");
                }
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

    if (imagick_init_log(imagick_setting->logfile, imagick_setting->logmark) == -1) {
        fail("Failed to initialize log file:%s\n", imagick_setting->logfile);
    }

    if (imagick_setting->daemon) {
        imagick_daemonize();
    }
    imagick_master_process_start(imagick_setting);
    return 0;
}
