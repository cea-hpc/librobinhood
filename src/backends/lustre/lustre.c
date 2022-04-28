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

static __thread struct rbh_sstack *_values;
static __thread bool is_dir;
static __thread bool is_reg;

static inline int
fill_pair(struct rbh_value_pair *pair, const char *key,
          const struct rbh_value *value)
{
    pair->key = key;
    pair->value = rbh_sstack_push(_values, value, sizeof(*value));
    if (pair->value == NULL)
        return -1;

    return 0;
}

static inline int
fill_string_pair(const char *str, const int len, struct rbh_value_pair *pair,
                 const char *key)
{
    const struct rbh_value string_value = {
        .type = RBH_VT_STRING,
        .string = rbh_sstack_push(_values, str, len),
    };

    return fill_pair(pair, key, &string_value);
}

static inline int
fill_uint32_pair(uint32_t integer, struct rbh_value_pair *pair, const char *key)
{
    const struct rbh_value uint32_value = {
        .type = RBH_VT_UINT32,
        .uint32 = integer,
    };

    return fill_pair(pair, key, &uint32_value);
}

/**
 * Record a file's fid in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_fid(int fd, struct rbh_value_pair *pairs)
{
    char ptr[FID_NOBRACE_LEN + 1];
    struct lu_fid fid;
    int subcount = 0;
    int rc;

    rc = llapi_fd2fid(fd, &fid);
    if (rc) {
        errno = -rc;
        return -1;
    }

    rc = snprintf(ptr, FID_NOBRACE_LEN, DFID_NOBRACE, PFID(&fid));
    if (rc < 0) {
        errno = -EINVAL;
        return -1;
    }

    rc = fill_string_pair(ptr, FID_NOBRACE_LEN + 1, &pairs[subcount++], "fid");

    return rc ? rc : subcount;
}

/**
 * Record a file's hsm attributes (state and archive_id) in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_hsm(int fd, struct rbh_value_pair *pairs)
{
    struct hsm_user_state hus;
    int subcount = 0;
    int rc;

    if (is_dir)
        return 0;

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

/**
 * Record a file's magic number and layout generator in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
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

/**
 * Record a file's layout attributes (main flags, and magic number and layout
 * generation if it is a regular file) in \p pairs
 *
 * @param fd        file descriptor to check
 * @param pairs     list of pairs to fill
 *
 * @return          number of filled \p pairs
 */
static int
xattrs_get_layout(int fd, struct rbh_value_pair *pairs)
{
    struct llapi_layout *layout;
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

    if (is_reg) {
        rc = xattrs_get_magic_and_gen(fd, &pairs[subcount]);
        if (rc < 0)
            goto err;


        subcount += rc;
        rc = 0;
    }

err:
    llapi_layout_free(layout);
    return rc ? rc : subcount;
}

#define XATTRS_N_FUNC 3

static ssize_t
lustre_ns_xattrs_callback(const int fd, const uint16_t mode,
                          struct rbh_value_pair *pairs,
                          struct rbh_sstack *values)
{
    int (*xattrs_funcs[XATTRS_N_FUNC])(int, struct rbh_value_pair *) = {
        xattrs_get_fid, xattrs_get_hsm, xattrs_get_layout
    };
    int count = 0;
    int subcount;

    is_dir = S_ISDIR(mode);
    is_reg = S_ISREG(mode);
    _values = values;

    for (int i = 0; i < XATTRS_N_FUNC; ++i, count += subcount) {
        subcount = xattrs_funcs[i](fd, &pairs[count]);
        if (subcount == -1)
            return -1;
    }

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
