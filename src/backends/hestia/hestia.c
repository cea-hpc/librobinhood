#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "robinhood/backends/hestia.h"
#include "robinhood/sstack.h"

#include "hestia_glue.h"

/*----------------------------------------------------------------------------*
 |                              hestia_iterator                               |
 *----------------------------------------------------------------------------*/

struct hestia_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_sstack *values;
    struct hestia_id *ids;
    size_t current;
    size_t length;
};

static void *
hestia_iter_next(void *iterator)
{
    struct hestia_iterator *hestia_iter = iterator;
    char *obj_attrs = NULL;
    struct hestia_id obj;
    size_t attrs_len;
    int rc;

    if (hestia_iter->current >= hestia_iter->length)
        return NULL;

    obj = hestia_iter->ids[hestia_iter->current];

    rc = list_object_attrs(&obj, &obj_attrs, &attrs_len);
    if (rc)
        return NULL;

    /* The following lines will be removed in the next patch */
    fprintf(stderr, "len = %ld, cur = %ld\n", hestia_iter->length, hestia_iter->current);
    fprintf(stderr, "object found = (%ld, %ld)\n", obj.higher, obj.lower);
    fprintf(stderr, "object attrs = '%s'\n", obj_attrs);

    hestia_iter->current++;

    free(obj_attrs);

    /* Will be changed next patch */
    return NULL;
}

static void
hestia_iter_destroy(void *iterator)
{
    struct hestia_iterator *hestia_iter = iterator;

    rbh_sstack_destroy(hestia_iter->values);
    free(hestia_iter->ids);
    free(hestia_iter);
}

static const struct rbh_mut_iterator_operations HESTIA_ITER_OPS = {
    .next = hestia_iter_next,
    .destroy = hestia_iter_destroy,
};

static const struct rbh_mut_iterator HESTIA_ITER = {
    .ops = &HESTIA_ITER_OPS,
};

struct hestia_iterator *
hestia_iterator_new()
{
    struct hestia_iterator *hestia_iter = NULL;
    struct hestia_id *ids = NULL;
    uint8_t *tiers = NULL;
    size_t tiers_len;
    size_t ids_len;
    int save_errno;
    int rc;

    hestia_iter = malloc(sizeof(*hestia_iter));
    if (hestia_iter == NULL)
        return NULL;

    rc = list_tiers(&tiers, &tiers_len);
    if (rc)
        goto err;

    rc = list_objects(tiers, tiers_len, &ids, &ids_len);
    if (rc)
        goto err;

    free(tiers);

    hestia_iter->iterator = HESTIA_ITER;
    hestia_iter->ids = ids;
    hestia_iter->length = ids_len;
    hestia_iter->current = 0;

    hestia_iter->values = rbh_sstack_new(1 << 10);
    if (hestia_iter->values == NULL)
        goto err;

    return hestia_iter;

err:
    save_errno = errno;
    free(tiers);
    free(ids);
    free(hestia_iter);
    errno = save_errno;
    return NULL;
}

/*----------------------------------------------------------------------------*
 |                               hestia_backend                               |
 *----------------------------------------------------------------------------*/

struct hestia_backend {
    struct rbh_backend backend;
    struct hestia_iterator *(*iter_new)();
};

    /*--------------------------------------------------------------------*
     |                              filter()                              |
     *--------------------------------------------------------------------*/

static struct rbh_mut_iterator *
hestia_backend_filter(void *backend, const struct rbh_filter *filter,
                     const struct rbh_filter_options *options)
{
    struct hestia_backend *hestia = backend;
    struct hestia_iterator *hestia_iter;

    if (filter != NULL) {
        errno = ENOTSUP;
        return NULL;
    }

    if (options->skip > 0 || options->limit > 0 || options->sort.count > 0) {
        errno = ENOTSUP;
        return NULL;
    }

    hestia_iter = hestia->iter_new();
    if (hestia_iter == NULL)
        return NULL;

    return &hestia_iter->iterator;
}

    /*--------------------------------------------------------------------*
     |                             destroy()                              |
     *--------------------------------------------------------------------*/

static void
hestia_backend_destroy(void *backend)
{
    struct hestia_backend *hestia = backend;

    free(hestia);
}

static const struct rbh_backend_operations HESTIA_BACKEND_OPS = {
    .filter = hestia_backend_filter,
    .destroy = hestia_backend_destroy,
};

static const struct rbh_backend HESTIA_BACKEND = {
    .id = RBH_BI_HESTIA,
    .name = RBH_HESTIA_BACKEND_NAME,
    .ops = &HESTIA_BACKEND_OPS,
};

struct rbh_backend *
rbh_hestia_backend_new(__attribute__((unused)) const char *path)
{
    struct hestia_backend *hestia;

    hestia = malloc(sizeof(*hestia));
    if (hestia == NULL)
        return NULL;

    hestia->iter_new = hestia_iterator_new;
    hestia->backend = HESTIA_BACKEND;

    return &hestia->backend;
}
