#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include "ncx_core.h"
#include "ncx_lock.h"

static int __cpus = 1;

void ncx_shmtx_init()
{
    __cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (__cpus <= 0) {
        __cpus = 1;
    }
}

void ncx_shmtx_lock(ncx_shmtx_t *mutex)
{
    int i, n;

    for ( ;; ) {
        if (mutex->spin == 0 &&
                __sync_bool_compare_and_swap(&mutex->spin, 0, 1)) {
            return;
        }

        if (__cpus > 1) {
            for (n = 1; n < 129; n << 1) {
                for (i = 0; i < n; i++) {
                    __asm__("pause");
                }

                if (mutex->spin == 0 &&
                        __sync_bool_compare_and_swap(&mutex->spin, 0, 1)) {
                    return;
                }
            }
        }
        sched_yield();
    }
}

void ncx_shmtx_unlock(ncx_shmtx_t *mutex)
{
    __sync_bool_compare_and_swap(&mutex->spin, 1, 0);
}
