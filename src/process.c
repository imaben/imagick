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
#include <sys/wait.h>
#include <sys/mman.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "process.h"
#include "channel.h"
#include "log.h"
#include "connection.h"
#include "worker.h"
#include "utils.h"
#include "ncx_slab.h"
#include "ncx_lock.h"
#include "hash.h"

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

imagick_worker_ctx_t *ctx;

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

static int imagick_worker_ctx_init()
{
    static imagick_worker_ctx_t __ctx;
    ctx = &__ctx;
    ctx->pid = getpid();
    ctx->rwfd = imagick_channel;

    ctx->loop = imagick_event_loop_create(IE_DEFAULT_FD_COUNT);

    return 0;
}

static void imagick_listening_init()
{

}

static void imagick_worker_process_exit()
{
    imagick_close_channel(imagick_processes[imagick_process_slot].channel);

    imagick_log_debug("worker process %d exit;", getpid());
    exit(0);
}

static void imagick_worker_cmd_handler(int sockfd, imagick_channel_cmd_t *cmd)
{
    switch (cmd->cmd) {
        case IMAGICK_PROCESS_CMD_INIT:
            imagick_listening_init();
            break;
        case IMAGICK_PROCESS_CMD_EXIT:
            imagick_worker_process_exit();
            break;
        default:
            break;
    }
}

static void imagick_ipc_handler(imagick_event_loop_t *loop, int fd, void *arg)
{
    int r;
    imagick_channel_cmd_t cmd;
    r = read(ctx->rwfd, &cmd, sizeof(imagick_channel_cmd_t));
    if (r == -1) {
        imagick_log_warn("read failure, code:%d", errno);
        return;
    }
    imagick_worker_cmd_handler(imagick_channel, &cmd);
}

static void imagick_worker_process_cycle(void *data)
{
    imagick_log_debug("worker process %d start", getpid());
    int i, r, connfd;

    r = imagick_worker_ctx_init();
    if (r == -1) {
        goto fail;
    }

    struct sockaddr_in clientaddr;
    socklen_t clilen;

    /* IPC events */
    ctx->loop->add_event(ctx->loop, ctx->rwfd, IE_READABLE, imagick_ipc_handler, NULL);

    /* http connection events */
    ctx->loop->add_event(ctx->loop, main_ctx->sockfd, IE_READABLE, imagick_main_sock_handler, NULL);

    ctx->loop->dispatch(ctx->loop);

fail:
    imagick_worker_process_exit();
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
}

void imagick_master_exit(int sig_no)
{
    int i;
    imagick_channel_t ch;
    imagick_channel_cmd_t cmd;
    cmd.cmd = IMAGICK_PROCESS_CMD_EXIT;
    for (i = 0; i < imagick_last_process; i++) {
        ch.pid = imagick_processes[i].pid;
        ch.slot = i;
        ch.fd = imagick_processes[i].channel[0];
        imagick_write_channel(&ch, &cmd);
    }
}

void imagick_worker_exit(int signo)
{
    pid_t pid;
    int stat;
    pid = wait(&stat);
    return;
}

static void *imagick_ht_malloc(size_t size)
{
    return ncx_slab_alloc(main_ctx->pool, size);
}

static u_char *imagick_shm_alloc(size_t size)
{
    int fd;
    u_char *addr;
    fd = open("/dev/zero", O_RDWR);
    if (fd == -1) {
        imagick_log_error("open (/dev/zero) failed");
        return NULL;
    }

    addr = (u_char *)mmap(NULL, size, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        imagick_log_error("mmap (/dev/zero, MAP_SHARED, %uz) failed", size);
        return NULL;
    }

    close(fd);
    return addr;
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

    /* init socket listen */
    main_ctx->sockfd = imagick_listen_socket(setting->host, setting->port);
    if (main_ctx->sockfd == -1) {
        imagick_log_error("cannot listen %s:%d", setting->host, setting->port);
        return;
    }

    if (imagick_set_nonblocking(main_ctx->sockfd) == -1) {
        imagick_log_error("Faild to set socket nonblocking");
        return;
    }

    /* init shared memory */
    ncx_shmtx_init();
    u_char *space = imagick_shm_alloc(setting->max_cache);
    if (space == NULL) {
        imagick_log_error("Cannot alloc shared memory");
        return;
    }
    main_ctx->pool = (ncx_slab_pool_t *)space;
    main_ctx->pool->addr = space;
    main_ctx->pool->min_shift = 3;
    main_ctx->pool->end = space + setting->max_cache;
    ncx_slab_init(main_ctx->pool);

    /* init hash table */
    main_ctx->cache_ht = imagick_hash_new(0, NULL, NULL, imagick_ht_malloc);
    if (main_ctx->cache_ht == NULL) {
        imagick_log_error("Failed to alloc shared memory for HashTable");
        return;
    }

    imagick_worker_process_start(setting->processes);

    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1) {
        imagick_log_error("sigprocmask SIG_SETMASK failed");
    }

    sigsuspend(&set);
}
