#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "robinhood/backends/hestia.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"

#include "hestia_glue.h"

/*----------------------------------------------------------------------------*
 |                              hestia_iterator                               |
 *----------------------------------------------------------------------------*/

struct hestia_iterator {
    struct rbh_mut_iterator iterator;
    struct rbh_sstack *values;
    struct hestia_id *ids;
    size_t current_id; /* index of the object in 'ids' that will be managed in
                        * the next call to "hestia_iter_next". */
    size_t length;
};

/* FIXME: this function and its uses is temporary until either Hestia provides
 * a better API for attribute management, or we properly parse the string using
 * a JSON library.
 */
static int
attr2uint(char *json_attrs, char *attr, uint64_t *value)
{
    char *found;
    int rc = 0;
    char *end;

    /* In Hestia, attributes are given as a string of the form:
     * {"attr1":value1,"attr2":value2,...}
     * So to parse a specific attribute, we have to find it in the string,
     * increase the pointer by the length of the attr + 1 (the last quote mark)
     * + 1 (the colon), and then we can convert the value to uint64_t.
     */
    found = strstr(json_attrs, attr);
    if (found == NULL) {
        errno = ENODATA;
        return -1;
    }

    fprintf(stderr, "found = '%s'\n", found);

    found += strlen(attr) + 2;
    if (!isdigit(*found)) {
        errno = EINVAL;
        return -1;
    }

    fprintf(stderr, "found after = '%s'\n", found);
    errno = 0;
    *value = strtoull(found, &end, 0);
    if (errno) {
        rc = -1;
    } else if (*end != ',' && *end != '}') {
        errno = EINVAL;
        rc = -1;
    }

    fprintf(stderr, "value = '%ld'\n", *value);

    return rc;
}

static int
get_statx(char *attrs, struct rbh_statx *statx)
{
    uint64_t value;
    int rc;

    rc = attr2uint(attrs, "creation_time", &value);
    if (rc)
        return rc;

    statx->stx_btime.tv_sec = value;
    statx->stx_btime.tv_nsec = 0;
    statx->stx_mask = RBH_STATX_BTIME;

    rc = attr2uint(attrs, "last_modified", &value);
    if (rc)
        return rc;

    statx->stx_mtime.tv_sec = value;
    statx->stx_mtime.tv_nsec = 0;
    statx->stx_mask = RBH_STATX_MTIME;

    return 0;
}

static int
get_xattrs(char *attrs, struct rbh_value_pair **_pair,
           struct rbh_sstack *values)
{
    struct rbh_value *tier_value;
    struct rbh_value_pair *pair;
    int rc;

    tier_value = rbh_sstack_push(values, NULL, sizeof(*tier_value));
    if (tier_value == NULL)
        return -1;

    tier_value->type = RBH_VT_UINT64;
    rc = attr2uint(attrs, "tier", &tier_value->uint64);
    if (rc)
        return -1;

    pair = rbh_sstack_push(values, NULL, sizeof(*pair));
    if (pair == NULL)
        return -1;

    pair->key = "tier";
    pair->value = tier_value;

    *_pair = pair;

    return 0;
}

static int
fill_path(char *path, struct rbh_value_pair **_pairs, struct rbh_sstack *values)
{
    struct rbh_value *path_value;
    struct rbh_value_pair *pair;

    path_value = rbh_sstack_push(values, NULL, sizeof(*path_value));
    if (path_value == NULL)
        return -1;

    path_value->type = RBH_VT_STRING;
    path_value->string = path;

    pair = rbh_sstack_push(values, NULL, sizeof(*pair));
    if (pair == NULL)
        return -1;

    pair->key = "path";
    pair->value = path_value;

    *_pairs = pair;

    return 0;
}

static void *
hestia_iter_next(void *iterator)
{
    struct hestia_iterator *hestia_iter = iterator;
    struct rbh_fsentry *fsentry = NULL;
    struct rbh_value_pair *inode_pairs;
    struct rbh_value_map inode_xattrs;
    struct rbh_value_pair *ns_pairs;
    struct rbh_value_map ns_xattrs;
    struct rbh_id parent_id;
    char *obj_attrs = NULL;
    struct rbh_statx statx;
    struct hestia_id *obj;
    char *name = NULL;
    struct rbh_id id;
    size_t attrs_len;
    int rc;

    if (hestia_iter->current_id >= hestia_iter->length) {
        errno = ENODATA;
        return NULL;
    }

    obj = &hestia_iter->ids[hestia_iter->current_id];

    rc = list_object_attrs(obj, &obj_attrs, &attrs_len);
    if (rc)
        return NULL;

    /* Use the hestia_id of each file as rbh_id */
    id.data = rbh_sstack_push(hestia_iter->values, obj, sizeof(*obj));
    if (id.data == NULL)
        goto err;

    id.size = sizeof(*obj);

    /* All objects have no parent */
    parent_id.size = 0;

    rc = asprintf(&name, "%ld-%ld", obj->higher, obj->lower);
    if (rc <= 0)
        goto err;

    rc = get_statx(obj_attrs, &statx);
    if (rc)
        goto err;

    rc = get_xattrs(obj_attrs, &inode_pairs, hestia_iter->values);
    if (rc)
        goto err;

    inode_xattrs.pairs = inode_pairs;
    inode_xattrs.count = 1;

    rc = fill_path(name, &ns_pairs, hestia_iter->values);
    if (rc)
        goto err;

    ns_xattrs.pairs = ns_pairs;
    ns_xattrs.count = 1;

    fsentry = rbh_fsentry_new(&id, &parent_id, name, &statx, &ns_xattrs,
                              &inode_xattrs, NULL);
    if (fsentry == NULL)
        goto err;

    hestia_iter->current_id++;

err:
    free(name);
    free(obj_attrs);

    return fsentry;
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
    free(tiers);
    if (rc)
        goto err;

    hestia_iter->iterator = HESTIA_ITER;
    hestia_iter->ids = ids;
    hestia_iter->length = ids_len;
    hestia_iter->current_id = 0;

    hestia_iter->values = rbh_sstack_new(1 << 10);
    if (hestia_iter->values == NULL)
        goto err;

    return hestia_iter;

err:
    save_errno = errno;
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
