#pragma once
#include <pthread.h>

typedef struct imagick_lock_s imagick_lock_t;

struct imagick_lock_s {
    pthread_mutex_t *mutex;
};

int imagick_lock_init(imagick_lock_t *l);
void imagick_lock_lock(imagick_lock_t *l);
void imagick_lock_unlock(imagick_lock_t *l);
void imagick_lock_free(imagick_lock_t *l);
