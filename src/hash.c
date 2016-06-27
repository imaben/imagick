#include <stdlib.h>
#include <string.h>
#include "hash.h"

static imagick_hash_malloc_fn *ht_malloc;

static unsigned int imagick_hash_buckets_size[] = {
    7,          13,         31,         61,         127,        251,
    509,        1021,       2039,       4093,       8191,       16381,
    32749,      65521,      131071,     262143,     524287,     1048575,
    2097151,    4194303,    8388607,    16777211,   33554431,   67108863,
    134217727,  268435455,  536870911,  1073741823, 2147483647, 0
};


static void imagick_hash_rehash(imagick_hash_t *o);


static long imagick_hash_default_hash(char *key, int klen)
{
    long h = 0, g;
    char *kend = key + klen;

    while (key < kend) {
        h = (h << 4) + *key++;
        if ((g = (h & 0xF0000000))) {
            h = h ^ (g >> 24);
            h = h ^ g;
        }
    }
    return h;
}


int imagick_hash_init(imagick_hash_t *o, unsigned int init_buckets,
    imagick_hash_hash_fn *hash, imagick_hash_free_fn *free, imagick_hash_malloc_fn *mmalloc)
{
    if (init_buckets < IMAGICK_HASH_BUCKETS_MIN_SIZE) {
        init_buckets = IMAGICK_HASH_BUCKETS_MIN_SIZE;

    } else if (init_buckets > IMAGICK_HASH_BUCKETS_MAX_SIZE) {
        init_buckets = IMAGICK_HASH_BUCKETS_MAX_SIZE;
    }

    if (mmalloc == NULL) {
        o->malloc = malloc;
    } else {
        o->malloc = mmalloc;
    }

    o->buckets = o->malloc(sizeof(void *) * init_buckets);
    memset(o->buckets, 0, sizeof(void *) * init_buckets);
    if (NULL == o->buckets) {
        return -1;
    }

    if (!hash) {
        hash = &imagick_hash_default_hash;
    }

    o->hash = hash;
    o->free = free;
    o->buckets_size = init_buckets;
    o->elm_nums = 0;

    return 0;
}


imagick_hash_t *imagick_hash_new(unsigned int init_buckets,
    imagick_hash_hash_fn *hash, imagick_hash_free_fn *free, imagick_hash_malloc_fn *mmalloc)
{
    imagick_hash_t *o;

    if (mmalloc == NULL) {
        o = malloc(sizeof(*o));
    } else {
        o = mmalloc(sizeof(*o));
    }

    if (NULL == o) {
        return NULL;
    }

    if (imagick_hash_init(o, init_buckets, hash, free, mmalloc) == -1) {
        free(o);
        return NULL;
    }

    return o;
}


int imagick_hash_find(imagick_hash_t *o, char *key, int klen, void **ret)
{
    imagick_hash_entry_t *e;
    long hashval = o->hash(key, klen);
    int index = hashval % o->buckets_size;

    e = o->buckets[index];
    while (e) {
        if (e->hashval == hashval && e->klen == klen &&
            !strncmp(e->key, key, klen))
        {
            if (ret) {
                *ret = e->data;
            }
            return IMAGICK_HASH_OK;
        }
        e = e->next;
    }
    return IMAGICK_HASH_ERR;
}


int imagick_hash_insert(imagick_hash_t *o, char *key, int klen, void *data, int replace)
{
    imagick_hash_entry_t *en, **ei;
    long hashval = o->hash(key, klen);
    int index = hashval % o->buckets_size;

    ei = (imagick_hash_entry_t **)&o->buckets[index];

    while (*ei) {
        if ((*ei)->hashval == hashval && (*ei)->klen == klen &&
            !strncmp((*ei)->key, key, klen)) /* found the key */
        {
            if (replace) {
                if (o->free) {
                    o->free((*ei)->data);
                }
                (*ei)->data = data;
                return IMAGICK_HASH_OK;
            }
            return IMAGICK_HASH_DUPLICATE_KEY;
        }
        ei = &((*ei)->next);
    }

    en = o->malloc(sizeof(*en) + klen);
    if (NULL == en) {
        return IMAGICK_HASH_ERR;
    }

    en->hashval = hashval;
    en->klen = klen;
    en->data = data;
    en->next = NULL;

    memcpy(en->key, key, klen);

    *ei = en; /* append to the last of hash list */

    o->elm_nums++;

    if (o->elm_nums * 1.5 > o->buckets_size) {
        imagick_hash_rehash(o);
    }

    return IMAGICK_HASH_OK;
}


int imagick_hash_remove(imagick_hash_t *o, char *key, int klen)
{
    imagick_hash_entry_t *e, *p;
    long hashval = o->hash(key, klen);
    int index = hashval % o->buckets_size;

    p = NULL;
    e = o->buckets[index];

    while (e) {
        if (e->hashval == hashval && e->klen == klen &&
            !strncmp(e->key, key, klen))
        {
            break;
        }
        p = e;
        e = e->next;
    }

    if (!e) { /* not found */
        return IMAGICK_HASH_ERR;
    }

    if (!p) {
        o->buckets[index] = e->next;
    } else {
        p->next = e->next;
    }

    if (o->free) {
        o->free(e->data);
    }

    free(e);
    o->elm_nums--;

    return IMAGICK_HASH_OK;
}


static void imagick_hash_rehash(imagick_hash_t *o)
{
    imagick_hash_t new_htb;
    unsigned int buckets_size;
    imagick_hash_entry_t *e, *next;
    int i, index;

    /* find new buckets size */
    for (i = 0; imagick_hash_buckets_size[i] != 0; i++) {
        if (imagick_hash_buckets_size[i] > o->buckets_size) {
            break;
        }
    }

    if (imagick_hash_buckets_size[i] > 0) {
        buckets_size = imagick_hash_buckets_size[i];
    } else {
        buckets_size = imagick_hash_buckets_size[i-1];
    }

    /* if new buckets size equls old buckets size,
     * or init new hashtable failed, return. */
    if (buckets_size == o->buckets_size ||
        imagick_hash_init(&new_htb, buckets_size, NULL, NULL, o->malloc) == -1) {
        return;
    }

    for (i = 0; i < o->buckets_size; i++) {

        e = o->buckets[i];
        while (e) {
            next = e->next; /* next process entry */

            index = e->hashval % new_htb.buckets_size;
            e->next = new_htb.buckets[index];
            new_htb.buckets[index] = e;

            e = next;
        }
    }

    free(o->buckets); /* free old buckets */

    o->buckets = new_htb.buckets;
    o->buckets_size = new_htb.buckets_size;

    return;
}


void imagick_hash_destroy(imagick_hash_t *o)
{
    imagick_hash_entry_t *e, *next;
    int i;

    for (i = 0; i < o->buckets_size; i++) {

        e = o->buckets[i];

        while (e) {
            next = e->next;
            if (o->free) {
                o->free(e->data);
            }
            free(e);
            e = next;
        }
    }

    free(o->buckets);

    return;
}


void imagick_hash_free(imagick_hash_t *o)
{
    imagick_hash_destroy(o);
    free(o);
}
