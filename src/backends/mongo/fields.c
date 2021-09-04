/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/statx.h"

#include "mongo.h"

const char *field2str(const struct rbh_filter_field *field, char **buffer,
                      size_t bufsize)
{
    switch (field->fsentry) {
    case RBH_FP_ID:
        return MFF_ID;
    case RBH_FP_PARENT_ID:
        return MFF_NAMESPACE "." MFF_PARENT_ID;
    case RBH_FP_NAME:
        return MFF_NAMESPACE "." MFF_NAME;
    case RBH_FP_SYMLINK:
        return MFF_SYMLINK;
    case RBH_FP_STATX:
        switch (field->statx) {
        case RBH_STATX_TYPE:
            return MFF_STATX "." MFF_STATX_TYPE;
        case RBH_STATX_MODE:
            return MFF_STATX "." MFF_STATX_MODE;
        case RBH_STATX_NLINK:
            return MFF_STATX "." MFF_STATX_NLINK;
        case RBH_STATX_UID:
            return MFF_STATX "." MFF_STATX_UID;
        case RBH_STATX_GID:
            return MFF_STATX "." MFF_STATX_GID;
        case RBH_STATX_ATIME_SEC:
            return MFF_STATX "." MFF_STATX_ATIME "." MFF_STATX_TIMESTAMP_SEC;
        case RBH_STATX_MTIME_SEC:
            return MFF_STATX "." MFF_STATX_MTIME "." MFF_STATX_TIMESTAMP_SEC;
        case RBH_STATX_CTIME_SEC:
            return MFF_STATX "." MFF_STATX_CTIME "." MFF_STATX_TIMESTAMP_SEC;
        case RBH_STATX_INO:
            return MFF_STATX "." MFF_STATX_INO;
        case RBH_STATX_SIZE:
            return MFF_STATX "." MFF_STATX_SIZE;
        case RBH_STATX_BLOCKS:
            return MFF_STATX "." MFF_STATX_BLOCKS;
        case RBH_STATX_BTIME_SEC:
            return MFF_STATX "." MFF_STATX_BTIME "." MFF_STATX_TIMESTAMP_SEC;
        case RBH_STATX_MNT_ID:
            return MFF_STATX "." MFF_STATX_MNT_ID;
        case RBH_STATX_BLKSIZE:
            return MFF_STATX "." MFF_STATX_BLKSIZE;
        case RBH_STATX_ATTRIBUTES:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES;
        case RBH_STATX_ATIME_NSEC:
            return MFF_STATX "." MFF_STATX_ATIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_BTIME_NSEC:
            return MFF_STATX "." MFF_STATX_BTIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_CTIME_NSEC:
            return MFF_STATX "." MFF_STATX_CTIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_MTIME_NSEC:
            return MFF_STATX "." MFF_STATX_MTIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_RDEV_MAJOR:
            return MFF_STATX "." MFF_STATX_RDEV "." MFF_STATX_DEVICE_MAJOR;
        case RBH_STATX_RDEV_MINOR:
            return MFF_STATX "." MFF_STATX_RDEV "." MFF_STATX_DEVICE_MINOR;
        case RBH_STATX_DEV_MAJOR:
            return MFF_STATX "." MFF_STATX_DEV "." MFF_STATX_DEVICE_MAJOR;
        case RBH_STATX_DEV_MINOR:
            return MFF_STATX "." MFF_STATX_DEV "." MFF_STATX_DEVICE_MINOR;
        }
        break;
    case RBH_FP_NAMESPACE_XATTRS:
        if (field->xattr == NULL)
            return MFF_NAMESPACE "." MFF_XATTRS;

        if (snprintf(*buffer, bufsize, "%s.%s", MFF_NAMESPACE "." MFF_XATTRS,
                     field->xattr) < bufsize)
            return *buffer;

        /* `*buffer' is too small */
        if (asprintf(buffer, "%s.%s", MFF_NAMESPACE "." MFF_XATTRS,
                     field->xattr) < 0)
            return NULL;

        return *buffer;
    case RBH_FP_INODE_XATTRS:
        if (field->xattr == NULL)
            return MFF_XATTRS;

        if (snprintf(*buffer, bufsize, "%s.%s", MFF_XATTRS, field->xattr)
                < bufsize)
            return *buffer;

        /* `*buffer' is too small */
        if (asprintf(buffer, "%s.%s", MFF_XATTRS, field->xattr) < 0)
            return NULL;
        return *buffer;
    }

    errno = ENOTSUP;
    return NULL;
}
