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
    .imgroot   = NULL,
    .daemon    = 0,
    .max_cache = IMAGICK_CACHE_MIN_SIZE
}, *imagick_setting = &__setting;

imagick_ctx_t __main_ctx, *main_ctx = &__main_ctx;;

static struct option options[] = {
    {"host",        required_argument,   0,   'h'},
    {"port",        required_argument,   0,   'p'},
    {"processes",   required_argument,   0,   'c'},
    {"logfile",     required_argument,   0,   'l'},
    {"logmark",     required_argument,   0,   'm'},
    {"imgroot",     required_argument,   0,   'r'},
    {"max-cache",   required_argument,   0,   'C'},
    {"daemon",      no_argument,         0,   'd'},
    {"help",        no_argument,         0,   '?'},
    {0, 0, 0, 0}
};

#define fatal(fmt, ...) do { \
    fprintf(stderr, fmt, ##__VA_ARGS__); \
    exit(1); \
} while (0)

static void imagick_usage()
{
    char *usage =
        "Usage: imagick [options] [-h] <host> [-p] <port> [--] [args...]\n\n"
        " -h --host <host>\t\t\t\tThe host to bind\n"
        " -p --port <port>\t\t\t\tThe port to listen\n"
        " -c --processes <processes>\t\tThe worker processes number\n"
        " -r --imgroot <dir>\t\tThe images root path\n"
        " -l --logfile <file>\t\t\tThe log file to output\n"
        " -m --logmark <debug|notice|warn|error>\tWhich level log would be mark\n"
        " -C --max-cache  The max cache size\n"
        " -d --daemon\t\t\t\t Using daemonize mode\n"
        " --help\t\t\t\t\tDiskplay the usage\n";
    fprintf(stdout, usage);
    exit(0);
}

static void imagick_parse_cache_size(char *optarg)
{
    int i, unit, with_unit, ts;
    int units[] = {
        66, 98,     // B
        75, 107,    // K
        77, 109,    // M
        71, 103,    // G
        84, 116,    // T
        0
    };
    with_unit = 0;
    unit = (int)optarg[strlen(optarg) - 1];
    for (i = 0; units[i] != 0; i++) {
        if (units[i] == unit) {
            with_unit = 1;
            break;
        }
    }
    if (with_unit) {
        char size[32] = { 0 };
        strncpy(size, optarg, strlen(optarg) - 1);
        ts = atoi(size);
        switch ((char)unit) {
            case 'B':
            case 'b':
                imagick_setting->max_cache = ts;
                break;
            case 'K':
            case 'k':
                imagick_setting->max_cache = ts * 1024;
                break;
            case 'M':
            case 'm':
                imagick_setting->max_cache = ts * 1024 * 1024;
                break;
            case 'G':
            case 'g':
                imagick_setting->max_cache = ts * 1024 * 1024 * 1024;
                break;
            default:
                fatal("Unknow max-cache option");
                break;
        }
    } else {
        imagick_setting->max_cache = atoi(optarg);
    }
    if (imagick_setting->max_cache < IMAGICK_CACHE_MIN_SIZE) {
        imagick_setting->max_cache = IMAGICK_CACHE_MIN_SIZE;
    }
}

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
                    fatal("Invalid logmark argument\n");
                }
                break;
            case 'r':
                imagick_setting->imgroot = strdup(optarg);
                break;
            case 'C':
                imagick_parse_cache_size(optarg);
                break;
            case '?':
                imagick_usage();
                break;
            default:
                imagick_usage();
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

    if (imagick_setting->imgroot == NULL) {
        fatal("Image root cannot be empty!\n");
    }

    if (imagick_init_log(imagick_setting->logfile, imagick_setting->logmark) == -1) {
        fatal("Failed to initialize log file:%s\n", imagick_setting->logfile);
    }

    if (imagick_setting->daemon) {
        imagick_daemonize();
    }
    main_ctx->setting = imagick_setting;
    imagick_master_process_start(imagick_setting);
    return 0;
}
