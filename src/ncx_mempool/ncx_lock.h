#ifndef _NCX_LOCK_H_
#define _NCX_LOCK_H_

typedef struct {

    ncx_uint_t spin;

} ncx_shmtx_t;

void ncx_shmtx_init();
void ncx_shmtx_lock(ncx_shmtx_t *mutex);
void ncx_shmtx_unlock(ncx_shmtx_t *mutex);
#endif
