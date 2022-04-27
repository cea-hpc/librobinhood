#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/xattr.h>

#include <lustre/lustreapi.h>

#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/lustre.h"

struct iterator_data {
    struct rbh_value *mirror_count;
    struct rbh_value *stripe_count;
    struct rbh_value *stripe_size;
    struct rbh_value *mirror_id;
    struct rbh_value *pattern;
    struct rbh_value *begin;
    struct rbh_value *flags;
    struct rbh_value *pool;
    struct rbh_value *end;
    struct rbh_value *ost;
    int ost_size;
    int ost_idx;
    int comp;
};

static __thread struct rbh_sstack *_values;
static __thread bool is_dir;

static inline int
fill_pair(struct rbh_value_pair *pair, const char *key, struct rbh_value *value)
{
    pair->key = key;
    pair->value = rbh_sstack_push(_values, value, sizeof(*value));
    if (pair->value == NULL)
        return -1;

    return 0;
}

static inline int
fill_string_pair(char *str, struct rbh_value_pair *pair, const char *key)
{
    struct rbh_value *string_value = rbh_value_string_new(str);

    return fill_pair(pair, key, string_value);
}

static inline int
fill_uint32_pair(uint32_t integer, struct rbh_value_pair *pair, const char *key)
{
    struct rbh_value *uint32_value = rbh_value_uint32_new(integer);

    return fill_pair(pair, key, uint32_value);
}

static inline int
fill_sequence_pair(struct rbh_value *values, uint64_t length,
                   struct rbh_value_pair *pair, const char *key)
{
    struct rbh_value *sequence_value = rbh_value_sequence_new(values, length);

    return fill_pair(pair, key, sequence_value);
}

static int
xattrs_get_fid(int fd, struct rbh_value_pair *pairs)
{
    struct lu_fid fid;
    int subcount = 0;
    char *ptr;
    int rc;

    rc = llapi_fd2fid(fd, &fid);
    if (rc) {
        errno = -rc;
        return -1;
    }

    rc = asprintf(&ptr, DFID_NOBRACE, PFID(&fid));
    if (rc == -1)
        return -1;

    rc = fill_string_pair(ptr, &pairs[subcount++], "fid");

    return rc ? rc : subcount;
}

static int
xattrs_get_hsm(int fd, struct rbh_value_pair *pairs)
{
    struct hsm_user_state hus;
    int subcount = 0;
    int rc;

    rc = llapi_hsm_state_get_fd(fd, &hus);
    if (rc) {
        errno = -rc;
        return -1;
    }

    rc = fill_uint32_pair(hus.hus_states, &pairs[subcount++], "hsm_state");
    if (rc)
        return -1;

    rc = fill_uint32_pair(hus.hus_archive_id, &pairs[subcount++],
                          "hsm_archive_id");
    if (rc)
        return -1;

    return subcount;
}

static int
fill_iterator_data(struct llapi_layout *layout, struct iterator_data *data,
                   int index)
{
    char pool_tmp[LOV_MAXPOOLNAME + 1];
    uint64_t stripe_count = 0;
    bool is_init_or_not_comp;
    uint32_t flags = 0;
    uint64_t tmp = 0;
    int rc;

    rc = llapi_layout_stripe_count_get(layout, &stripe_count);
    if (rc)
        return -1;

    data->stripe_count[index] = *rbh_value_uint64_new(stripe_count);

    rc = llapi_layout_stripe_size_get(layout, &tmp);
    if (rc)
        return -1;

    data->stripe_size[index] = *rbh_value_uint64_new(tmp);

    rc = llapi_layout_pattern_get(layout, &tmp);
    if (rc)
        return -1;

    data->pattern[index] = *rbh_value_uint64_new(tmp);

    rc = llapi_layout_comp_flags_get(layout, &flags);
    if (rc)
        return -1;

    data->flags[index] = *rbh_value_uint32_new(flags);

    rc = llapi_layout_pool_name_get(layout, pool_tmp, sizeof(pool_tmp));
    if (rc)
        return -1;

    data->pool[index] = *rbh_value_string_new(pool_tmp);
    is_init_or_not_comp = (flags == LCME_FL_INIT ||
                           !llapi_layout_is_composite(layout));

    if (data->ost_idx + (is_init_or_not_comp ? stripe_count : 1) >
            data->ost_size) {
        data->ost = realloc(data->ost,
                data->ost_size * 2 * sizeof(*data->ost));
        if (data->ost == NULL)
            return -1;
        data->ost_size *= 2;
    }

    if (is_init_or_not_comp) {
        uint64_t i;

        for (i = 0; i < stripe_count; ++i, ++data->ost_idx) {
            data->ost[data->ost_idx].type = RBH_VT_UINT64;

            rc = llapi_layout_ost_index_get(layout, i,
                                            &data->ost[data->ost_idx].uint64);
            if (rc == -1 && errno == EINVAL)
                break;
        }
    } else {
        data->ost[data->ost_idx++] = *rbh_value_int32_new(-1);
    }

    return 0;
}

static int
xattrs_layout_iterator(struct llapi_layout *layout, void *cbdata)
{
    struct iterator_data *data = (struct iterator_data*) cbdata;
    uint64_t tmp2 = 0;
    uint64_t tmp = 0;
    int rc;

    rc = fill_iterator_data(layout, data, data->comp);
    if (rc)
        return -1;

    rc = llapi_layout_comp_extent_get(layout, &tmp, &tmp2);
    if (rc)
        return -1;

    data->begin[data->comp] = *rbh_value_uint64_new(tmp);
    data->end[data->comp] = *rbh_value_uint64_new(tmp2);

    rc = llapi_layout_mirror_id_get(layout, (uint32_t *) &tmp);
    if (rc)
        return -1;

    data->mirror_id[data->comp] = *rbh_value_uint32_new((uint32_t) tmp);

    rc = llapi_layout_mirror_count_get(layout, (uint16_t *) &tmp);
    if (rc)
        return -1;

    data->mirror_count[data->comp] = *rbh_value_uint32_new((uint16_t) tmp);

    data->comp += 1;

    return 0;
}

static int
init_iterator_data(struct iterator_data *data, uint32_t length)
{
    data->mirror_count = malloc(length * sizeof(*data->mirror_count));
    if (data->mirror_count == NULL)
        return -1;

    data->stripe_count = malloc(length * sizeof(*data->stripe_count));
    if (data->stripe_count == NULL)
        return -1;

    data->stripe_size = malloc(length * sizeof(*data->stripe_size));
    if (data->stripe_size == NULL)
        return -1;

    data->mirror_id = malloc(length * sizeof(*data->mirror_id));
    if (data->mirror_id == NULL)
        return -1;

    data->pattern = malloc(length * sizeof(*data->pattern));
    if (data->pattern == NULL)
        return -1;

    data->begin = malloc(length * sizeof(*data->begin));
    if (data->begin == NULL)
        return -1;

    data->flags = malloc(length * sizeof(*data->flags));
    if (data->flags == NULL)
        return -1;

    data->pool = malloc(length * sizeof(*data->pool));
    if (data->pool == NULL)
        return -1;

    data->end = malloc(length * sizeof(*data->end));
    if (data->end == NULL)
        return -1;

    data->ost = malloc(length * sizeof(*data->ost));
    if (data->ost == NULL)
        return -1;

    data->ost_size = length;
    data->ost_idx = 0;
    data->comp = 0;

    return 0;
}

static int
xattrs_fill_layout(struct iterator_data data, int nb_xattrs, uint32_t length,
                   struct rbh_value_pair *pairs)
{
    struct rbh_value *values[] = {data.stripe_count, data.stripe_size,
                                  data.pattern, data.flags, data.pool,
                                  data.mirror_count, data.mirror_id,
                                  data.begin, data.end};
    char *keys[] = {"stripe_count", "stripe_size", "pattern", "comp_flags",
                    "pool", "mirror_count", "mirror_id", "begin", "end"};
    int subcount = 0;
    int rc;

    for (int i = 0; i < nb_xattrs; ++i) {
        rc = fill_sequence_pair(values[i], length, &pairs[subcount++], keys[i]);
        if (rc)
            return -1;
    }

    rc = fill_sequence_pair(data.ost, data.ost_idx, &pairs[subcount++], "ost");
    if (rc)
        return -1;

    return subcount;
}

static int
xattrs_get_magic_and_gen(int fd, struct rbh_value_pair *pairs)
{
    char lov_buf[XATTR_SIZE_MAX];
    ssize_t xattr_size;
    uint32_t magic = 0;
    int subcount = 0;
    uint32_t gen = 0;
    int rc;

    xattr_size = fgetxattr(fd, XATTR_LUSTRE_LOV, lov_buf, sizeof(lov_buf));
    if (xattr_size < 0)
        return -1;

    magic = ((struct lov_user_md *) lov_buf)->lmm_magic;

    switch (magic) {
    case LOV_USER_MAGIC_V1:
        gen = ((struct lov_user_md_v1 *) lov_buf)->lmm_layout_gen;
        break;
    case LOV_USER_MAGIC_V3:
        gen = ((struct lov_user_md_v3 *) lov_buf)->lmm_layout_gen;
        break;
    case LOV_USER_MAGIC_COMP_V1:
        gen = ((struct lov_comp_md_v1 *) lov_buf)->lcm_layout_gen;
        break;
    default:
        break;
    }

    rc = fill_uint32_pair(magic, &pairs[subcount++], "magic");
    if (rc)
        return -1;

    rc = fill_uint32_pair(gen, &pairs[subcount++], "gen");
    if (rc)
        return -1;

    return subcount;
}

static int
xattrs_get_layout(int fd, struct rbh_value_pair *pairs)
{
    struct llapi_layout *layout;
    struct iterator_data data;
    uint32_t length = 1;
    int nb_xattrs = 5;
    int subcount = 0;
    uint32_t flags;
    int rc;

    layout = llapi_layout_get_by_fd(fd, 0);
    if (layout == NULL)
        return -1;

    rc = llapi_layout_flags_get(layout, &flags);
    if (rc)
        goto err;

    rc = fill_uint32_pair(flags, &pairs[subcount++], "flags");
    if (rc)
        goto err;

    if (!is_dir) {
        rc = xattrs_get_magic_and_gen(fd, &pairs[subcount]);
        if (rc < 0)
            goto err;
        else
            subcount += rc;

        rc = 0;
    }

    if (llapi_layout_is_composite(layout)) {
        rc = llapi_layout_comp_use(layout, LLAPI_LAYOUT_COMP_USE_LAST);
        if (rc)
            goto err;

        rc = llapi_layout_comp_id_get(layout, &length);
        if (rc)
            goto err;

        rc = llapi_layout_comp_use(layout, LLAPI_LAYOUT_COMP_USE_FIRST);
        if (rc)
            goto err;

        nb_xattrs += 4;
    }

    rc = init_iterator_data(&data, length);
    if (rc)
        goto err;

    if (llapi_layout_is_composite(layout)) {
        rc = llapi_layout_comp_iterate(layout, &xattrs_layout_iterator, &data);
        if (rc < 0)
            goto err;
    } else {
        rc = fill_iterator_data(layout, &data, 0);
        if (rc)
            goto err;
    }

    rc = xattrs_fill_layout(data, nb_xattrs, length, &pairs[subcount]);
    if (rc < 0)
        goto err;

    subcount += rc;
    rc = 0;

err:
    llapi_layout_free(layout);
    return rc ? rc : subcount;
}

static ssize_t
lustre_ns_xattrs_callback(int fd, struct rbh_value_pair *pairs,
                          size_t *_ns_pairs_count, struct rbh_sstack *values)
{
    struct stat stats;
    int count = 0;
    int subcount;
    int rc;

    rc = fstat(fd, &stats);
    if (rc)
        return rc;

    is_dir = S_ISDIR(stats.st_mode);
    _values = values;

    subcount = xattrs_get_fid(fd, &pairs[count]);
    if (subcount == -1)
        return -1;

    count += subcount;

    if (!is_dir) {
        subcount = xattrs_get_hsm(fd, &pairs[count]);
        if (subcount == -1)
            return -1;

        count += subcount;
    }

    subcount = xattrs_get_layout(fd, &pairs[count]);
    if (subcount == -1)
        return -1;

    count += subcount;

    return count;
}

struct posix_iterator *
lustre_iterator_new(const char *root, const char *entry, int statx_sync_type)
{
    struct posix_iterator *lustre_iter;

    lustre_iter = posix_iterator_new(root, entry, statx_sync_type);
    if (lustre_iter == NULL)
        return NULL;

    lustre_iter->ns_xattrs_callback = lustre_ns_xattrs_callback;

    return lustre_iter;
}

struct rbh_backend *
rbh_lustre_backend_new(const char *path)
{
    struct posix_backend *lustre;

    lustre = (struct posix_backend *)rbh_posix_backend_new(path);
    if (lustre == NULL)
        return NULL;

    lustre->iter_new = lustre_iterator_new;
    lustre->backend.id = RBH_BI_LUSTRE;
    lustre->backend.name = RBH_LUSTRE_BACKEND_NAME;

    return &lustre->backend;
}
