#include "imagick.h"
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include "process.h"
#include "channel.h"
#include "log.h"

int imagick_argc;
char **imagick_argv;

extern char **environ;

static char **main_argv = NULL; /* pointer to argument vector */
static char *main_last_argv = NULL; /* end of argv */

static char title_master[] = "imagick: master process";
static char title_worker[] = "imagick: worker process";

int               imagick_process_slot;
int               imagick_channel;
int               imagick_last_process;
imagick_process_t imagick_processes[IMAGICK_MAX_PROCESSES];

static int imagick_set_nonblocking(int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0 ||
            fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return -1;
    }
    return 0;
}

static void imagick_set_process_title_init(int argc, char **argv, char **envp)
{
    int i;

    for (i = 0; envp[i]; i++);

    environ = (char **)malloc(sizeof(char *) * (i + 1));

    for (i = 0; envp[i]; i++) {
        environ[i] = malloc(sizeof(char) * strlen(envp[i]) + 1);
        strcpy(environ[i], envp[i]);
    }
    environ[i] = NULL;

    main_argv = argv;
    if (i > 0) {
        main_last_argv = envp[i - 1] + strlen(envp[i - 1]);
    } else {
        main_last_argv = argv[argc - 1] + strlen(argv[argc -1]);
    }
}

static void imagick_set_process_title(const char *fmt, ...)
{
    static int init = 0;
    if (!init) {
        imagick_set_process_title_init(imagick_argc, imagick_argv, environ);
    }

    int i;
    char buf[256], *p = buf;
    memset(p, 0, 256);

    extern char **main_argv;
    extern char *main_last_argv;

    va_list ap;
    vsprintf(p, fmt, ap);
    va_end(ap);

    i = strlen(buf);

    if (i > main_last_argv - main_argv[0] - 2) {
        i = main_last_argv - main_argv[0] - 2;
        buf[i] = '\0';
    }

    strcpy(main_argv[0], buf);

    p = &main_argv[0][i];
    while (p < main_last_argv) *p++ = '\0';

    main_argv[1] = NULL;

    prctl(PR_SET_NAME, buf);
}

static void imagick_worker_process_init()
{

}

static void imagick_worker_process_exit()
{
    imagick_close_channel(imagick_processes[imagick_process_slot].channel);
    exit(0);
}

static void imagick_worker_cmd_handler(int sockfd, imagick_channel_cmd_t *cmd)
{
    switch (cmd->cmd) {
        case IMAGICK_PROCESS_CMD_INIT:
            imagick_worker_process_init();
            break;
        case IMAGICK_PROCESS_CMD_EXIT:
            imagick_worker_process_exit();
            break;
        default:
            break;
    }
}

static void imagick_worker_process_cycle(void *data)
{
    imagick_log_debug("worker process %d start", getpid());
    imagick_channel_cmd_t cmd;
    int epfd, i, r;
    epfd = epoll_create(128);
    if (-1 == epfd) {
        imagick_log_error("epoll_create failed");
        return;
    }

    struct epoll_event evs[20];

    struct epoll_event ev;
    ev.data.fd = imagick_channel;
    ev.events = EPOLLIN | EPOLLET;

    epoll_ctl(epfd, EPOLL_CTL_ADD, imagick_channel, &ev);

    for (;;) {

        int num = epoll_wait(epfd, evs, 20, -1);
        for (i = 0; i < num; i++) {
            if (evs[i].data.fd == imagick_channel) {
                r = read(imagick_channel, &cmd, sizeof(imagick_channel_cmd_t));
                if (r == -1) {
                    imagick_log_warn("read failure, code:%d", errno);
                    continue;
                }
                imagick_worker_cmd_handler(imagick_channel, &cmd);
            }
        }

    }

    imagick_log_debug("worker process %d exit;", getpid());
    exit(0);
}

static pid_t imagick_spawn_process(char *name, void *data, imagick_spawn_proc_pt proc)
{
    int s;
    u_long on;
    pid_t pid;

    for (s = 0; s < imagick_last_process; s++) {
        if (imagick_processes[s].pid == -1) {
            break;
        }
    }

    if (s == IMAGICK_MAX_PROCESSES) {
        imagick_log_error("no more than %d processes can be spawned", IMAGICK_MAX_PROCESSES);
        return -1;
    }

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, imagick_processes[s].channel) == -1) {
        imagick_log_error("socketpair() failed while spawning");
        return -1;
    }

    if (imagick_set_nonblocking(imagick_processes[s].channel[0]) == -1) {
        imagick_log_error("failed to set nonblocking");
        imagick_close_channel(imagick_processes[s].channel);
        return -1;
    }

    if (imagick_set_nonblocking(imagick_processes[s].channel[1]) == -1) {
        imagick_log_error("failed to set nonblocking");
        imagick_close_channel(imagick_processes[s].channel);
        return -1;
    }

    on = 1;
    if (ioctl(imagick_processes[s].channel[0], FIOASYNC, &on) == -1) {
        imagick_log_error("failed to ioctl()");
        imagick_close_channel(imagick_processes[s].channel);
        return -1;
    }

    if (fcntl(imagick_processes[s].channel[0], F_SETOWN, getpid()) == -1) {
        imagick_log_error("failed to set F_SETOWN");
        imagick_close_channel(imagick_processes[s].channel);
        return -1;
    }

    if (fcntl(imagick_processes[s].channel[0], F_SETFD, FD_CLOEXEC) == -1) {
        imagick_log_error("failed to set F_SETFD");
        imagick_close_channel(imagick_processes[s].channel);
        return -1;
    }

    if (fcntl(imagick_processes[s].channel[1], F_SETFD, FD_CLOEXEC) == -1) {
        imagick_log_error("failed to set F_SETFD");
        imagick_close_channel(imagick_processes[s].channel);
        return -1;
    }

    imagick_channel = imagick_processes[s].channel[1];
    imagick_process_slot = s;

    pid = fork();

    switch (pid) {
        case -1:
            imagick_log_error("failed to fork child process");
            imagick_close_channel(imagick_processes[s].channel);
            return -1;
        case 0:
            strcpy(main_argv[0], name);
            proc(data);
            break;
        default:
            break;
    }

    imagick_processes[s].pid = pid;
    imagick_processes[s].exited = 0;

    if (s == imagick_last_process)
        imagick_last_process++;

    return pid;
}

static void imagick_worker_process_start(int processes)
{
    int i;
    imagick_channel_t ch;
    imagick_channel_cmd_t cmd;

    cmd.cmd = IMAGICK_PROCESS_CMD_INIT;

    memset(&ch, 0, sizeof(imagick_channel_t));

    for (i = 0; i < processes; i++) {
        imagick_spawn_process(title_worker, NULL, imagick_worker_process_cycle);
        ch.fd = imagick_processes[imagick_process_slot].channel[0];
        ch.pid = imagick_processes[imagick_process_slot].pid;
        ch.slot = imagick_process_slot;
        imagick_write_channel(&ch, &cmd);
    }

    cmd.cmd = IMAGICK_PROCESS_CMD_EXIT;
    for (i = 0; i < imagick_last_process; i++) {
        ch.pid = imagick_processes[i].pid;
        ch.slot = i;
        ch.fd = imagick_processes[i].channel[0];
        imagick_write_channel(&ch, &cmd);
        sleep(1);
    }
}

void imagick_master_exit(int sig_no)
{
    int i;
    for (i = 0; i < imagick_last_process; i++) {
        write(imagick_processes[i].channel[0], "\0", 1);
    }
}

void imagick_worker_exit(int signo)
{
    pid_t pid;
    int stat;
    pid = wait(&stat);
    return;
}

void imagick_master_process_start(imagick_setting_t *setting)
{
    signal(SIGINT, &imagick_master_exit);
    signal(SIGCHLD, &imagick_worker_exit);

    sigset_t set, old;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGINT);

    if (sigprocmask(SIG_BLOCK, &set, &old) == -1) {
        imagick_log_error("sigprocmask SIG_BLOCK failed");
    }

    sigemptyset(&set);

    imagick_set_process_title(title_master);
    imagick_worker_process_start(setting->processes);

    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) {
        imagick_log_error("sigprocmask SIG_SETMASK failed");
    }

    sigsuspend(&set);
}
