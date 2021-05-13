/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "robinhood/hashmap.h"

struct rbh_hashmap_item {
    const void *key;
    const void *value;
};

struct rbh_hashmap {
    size_t (*hash)(const void *key);
    bool (*equals)(const void *first, const void *second);
    struct rbh_hashmap_item *items;
    size_t count;
};

struct rbh_hashmap *
rbh_hashmap_new(bool (*equals)(const void *first, const void *second),
                size_t (*hash)(const void *key), size_t count)
{
    struct rbh_hashmap *hashmap;

    if (count == 0) {
        errno = EINVAL;
        return NULL;
    }

    hashmap = malloc(sizeof(*hashmap));
    if (hashmap == NULL)
        return NULL;

    hashmap->items = reallocarray(NULL, count, sizeof(*hashmap->items));
    if (hashmap->items == NULL) {
        int save_errno = errno;

        free(hashmap);
        errno = save_errno;
        return NULL;
    }

    hashmap->hash = hash;
    hashmap->equals = equals;
    for (size_t i = 0; i < count; i++)
        hashmap->items[i].key = NULL;

    hashmap->count = count;
    return hashmap;
}

/* Basic open addressing implementation */
static struct rbh_hashmap_item *
hashmap_key2slot(struct rbh_hashmap *hashmap, const void *key)
{
    size_t index = hashmap->hash(key) % hashmap->count;

    for (size_t i = index; i < hashmap->count; i++) {
        struct rbh_hashmap_item *item = &hashmap->items[i];

        if (item->key == NULL || hashmap->equals(item->key, key))
            return item;
    }

    for (size_t i = 0; i < index; i++) {
        struct rbh_hashmap_item *item = &hashmap->items[i];

        if (item->key == NULL || hashmap->equals(item->key, key))
            return item;
    }

    return NULL;
}

int
rbh_hashmap_set(struct rbh_hashmap *hashmap, const void *key, const void *value)
{
    struct rbh_hashmap_item *item;

    item = hashmap_key2slot(hashmap, key);
    if (item == NULL) {
        errno = ENOBUFS;
        return -1;
    }

    item->key = key;
    item->value = value;
    return 0;
}

const void *
rbh_hashmap_get(struct rbh_hashmap *hashmap, const void *key)
{
    struct rbh_hashmap_item *item = hashmap_key2slot(hashmap, key);

    if (item == NULL || item->key == NULL) {
        errno = ENOENT;
        return NULL;
    }

    return item->value;
}

const void *
rbh_hashmap_pop(struct rbh_hashmap *hashmap, const void *key)
{
    struct rbh_hashmap_item *item = hashmap_key2slot(hashmap, key);

    if (item == NULL || item->key == NULL) {
        errno = ENOENT;
        return NULL;
    }

    item->key = NULL;
    return item->value;
}

void
rbh_hashmap_destroy(struct rbh_hashmap *hashmap)
{
    free(hashmap->items);
    free(hashmap);
}
