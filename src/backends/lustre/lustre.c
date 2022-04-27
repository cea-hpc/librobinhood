#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <lustre/lustreapi.h>

#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/lustre.h"

static __thread struct rbh_sstack *_values;

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

static ssize_t
lustre_ns_xattrs_callback(int fd, struct rbh_value_pair *pairs,
                          size_t *_ns_pairs_count, struct rbh_sstack *values)
{
    int count = 0;
    int subcount;

    _values = values;

    subcount = xattrs_get_fid(fd, &pairs[count]);
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
