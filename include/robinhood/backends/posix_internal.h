/* This file is part of the RobinHood Library
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_POSIX_H
#define RBH_POSIX_H

/**
 * @file
 *
 * Internal header which defines the posix_iterator and posix_backend
 * structures, used by each backend which overload the posix one.
 *
 * For now, the only available overload is through ns_xattrs_callback
 * posix_iterator field, which may add extended attributes to the namespace.
 */

#include <fts.h>

#include "robinhood/backend.h"
#include "robinhood/sstack.h"

/*----------------------------------------------------------------------------*
 |                               posix_iterator                               |
 *----------------------------------------------------------------------------*/

struct posix_iterator {
    struct rbh_mut_iterator iterator;
    ssize_t (*ns_xattrs_callback)(int, struct rbh_value_pair *, size_t *,
                                  struct rbh_sstack *);
    int statx_sync_type;
    size_t prefix_len;
    FTS *fts_handle;
    FTSENT *ftsent;
};

struct posix_iterator *
posix_iterator_new(const char *root, const char *entry, int statx_sync_type);

/*----------------------------------------------------------------------------*
 |                               posix_backend                                |
 *----------------------------------------------------------------------------*/

struct posix_backend {
    struct rbh_backend backend;
    struct posix_iterator *(*iter_new)(const char *, const char *, int);
    char *root;
    int statx_sync_type;
};

#endif
