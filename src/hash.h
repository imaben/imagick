#pragma once

#define  IMAGICK_HASH_OK              (0)
#define  IMAGICK_HASH_ERR             (-1)
#define  IMAGICK_HASH_DUPLICATE_KEY   (-2)

#define  IMAGICK_HASH_BUCKETS_MIN_SIZE   7
#define  IMAGICK_HASH_BUCKETS_MAX_SIZE   2147483647

typedef long imagick_hash_hash_fn(char *, int);
typedef void imagick_hash_free_fn(void *);
typedef void *imagick_hash_malloc_fn(size_t);

typedef struct imagick_hash_s {
    imagick_hash_hash_fn *hash;
    imagick_hash_free_fn *free;
    imagick_hash_malloc_fn *malloc;
    void **buckets;
    unsigned int buckets_size;
    unsigned int elm_nums;
} imagick_hash_t;

typedef struct imagick_hash_entry_s imagick_hash_entry_t;

struct imagick_hash_entry_s {
    int hashval;
    int klen;
    void *data;
    imagick_hash_entry_t *next;
    char key[0];
};


int imagick_hash_init(imagick_hash_t *o, unsigned int init_buckets, imagick_hash_hash_fn *hash,
    imagick_hash_free_fn *free, imagick_hash_malloc_fn *mmalloc);
imagick_hash_t *imagick_hash_new(unsigned int init_buckets, imagick_hash_hash_fn *hash,
    imagick_hash_free_fn *free, imagick_hash_malloc_fn *mmalloc);
int imagick_hash_find(imagick_hash_t *o, char *key, int klen, void **ret);
int imagick_hash_insert(imagick_hash_t *o, char *key, int klen, void *data, int replace);
int imagick_hash_remove(imagick_hash_t *o, char *key, int klen);
void imagick_hash_destroy(imagick_hash_t *o);
void imagick_hash_free(imagick_hash_t *o);
