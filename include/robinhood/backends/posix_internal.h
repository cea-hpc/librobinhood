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
 * structures, used by each backend which overloads the posix one.
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

    /**
     * Callback for managing and filling namespace xattrs
     *
     * @param fd        file descriptor of the entry
     * @param mode      mode of file examined
     * @param pairs     list of rbh_value_pairs to fill
     * @param values    stack that will contain every rbh_value of
     *                  \p pairs
     *
     * @return          number of filled \p pairs
     */
    ssize_t (*ns_xattrs_callback)(const int fd, const uint16_t mode,
                                  struct rbh_value_pair *pairs,
                                  struct rbh_sstack *values);

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