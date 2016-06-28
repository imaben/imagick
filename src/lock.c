#include "lock.h"
#include "log.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

int imagick_lock_init(imagick_lock_t *l)
{
    pthread_mutexattr_t mattr;
    int fd = open("/dev/zero", O_RDWR, 0);
    if (fd == -1) {
        imagick_log_error("open (/dev/zero) failed");
        return -1;
    }

    l->mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0);
    if (l->mutex == MAP_FAILED) {
        imagick_log_error("mmap (/dev/zero, MAP_SHARED) failed");
        return -1;
    }
    close(fd);

    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(l->mutex, &mattr);
    return 0;
}

void imagick_lock_lock(imagick_lock_t *l)
{
    pthread_mutex_lock(l->mutex);
}

void imagick_lock_unlock(imagick_lock_t *l)
{
    pthread_mutex_unlock(l->mutex);
}

void imagick_lock_free(imagick_lock_t *l)
{
    pthread_mutex_destroy(l->mutex);
    munmap(l->mutex, sizeof(pthread_mutex_t));
}
