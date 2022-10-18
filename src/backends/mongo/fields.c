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

const char *subdoc2str(const uint32_t subdoc)
{
    switch (subdoc) {
        case RBH_STATX_ATIME:
            return MFF_STATX "." MFF_STATX_ATIME;
        case RBH_STATX_BTIME:
            return MFF_STATX "." MFF_STATX_BTIME;
        case RBH_STATX_CTIME:
            return MFF_STATX "." MFF_STATX_CTIME;
        case RBH_STATX_MTIME:
            return MFF_STATX "." MFF_STATX_MTIME;
        case RBH_STATX_RDEV:
            return MFF_STATX "." MFF_STATX_RDEV;
        case RBH_STATX_DEV:
            return MFF_STATX "." MFF_STATX_DEV;
    }

    errno = ENOTSUP;
    return NULL;
}

const char *attr2str(const uint32_t attr)
{
    switch (attr) {
        case RBH_STATX_ATTR_COMPRESSED:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_COMPRESSED;
        case RBH_STATX_ATTR_IMMUTABLE:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_IMMUTABLE;
        case RBH_STATX_ATTR_APPEND:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_APPEND;
        case RBH_STATX_ATTR_NODUMP:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_NODUMP;
        case RBH_STATX_ATTR_ENCRYPTED:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_ENCRYPTED;
        case RBH_STATX_ATTR_AUTOMOUNT:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_AUTOMOUNT;
        case RBH_STATX_ATTR_MOUNT_ROOT:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_MOUNT_ROOT;
        case RBH_STATX_ATTR_VERITY:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_VERITY;
        case RBH_STATX_ATTR_DAX:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES "." MFF_STATX_DAX;
    }

    errno = ENOTSUP;
    return NULL;
}

const char *statx2str(const uint32_t statx)
{
    switch (statx) {
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
        case RBH_STATX_ATIME_NSEC:
            return MFF_STATX "." MFF_STATX_ATIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_BTIME_SEC:
            return MFF_STATX "." MFF_STATX_BTIME "." MFF_STATX_TIMESTAMP_SEC;
        case RBH_STATX_BTIME_NSEC:
            return MFF_STATX "." MFF_STATX_BTIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_CTIME_SEC:
            return MFF_STATX "." MFF_STATX_CTIME "." MFF_STATX_TIMESTAMP_SEC;
        case RBH_STATX_CTIME_NSEC:
            return MFF_STATX "." MFF_STATX_CTIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_MTIME_SEC:
            return MFF_STATX "." MFF_STATX_MTIME "." MFF_STATX_TIMESTAMP_SEC;
        case RBH_STATX_MTIME_NSEC:
            return MFF_STATX "." MFF_STATX_MTIME "." MFF_STATX_TIMESTAMP_NSEC;
        case RBH_STATX_INO:
            return MFF_STATX "." MFF_STATX_INO;
        case RBH_STATX_SIZE:
            return MFF_STATX "." MFF_STATX_SIZE;
         case RBH_STATX_BLOCKS:
            return MFF_STATX "." MFF_STATX_BLOCKS;
        case RBH_STATX_MNT_ID:
            return MFF_STATX "." MFF_STATX_MNT_ID;
        case RBH_STATX_BLKSIZE:
            return MFF_STATX "." MFF_STATX_BLKSIZE;
        case RBH_STATX_ATTRIBUTES:
            return MFF_STATX "." MFF_STATX_ATTRIBUTES;
        case RBH_STATX_RDEV_MAJOR:
            return MFF_STATX "." MFF_STATX_RDEV "." MFF_STATX_DEVICE_MAJOR;
        case RBH_STATX_RDEV_MINOR:
            return MFF_STATX "." MFF_STATX_RDEV "." MFF_STATX_DEVICE_MINOR;
        case RBH_STATX_DEV_MAJOR:
            return MFF_STATX "." MFF_STATX_DEV "." MFF_STATX_DEVICE_MAJOR;
        case RBH_STATX_DEV_MINOR:
            return MFF_STATX "." MFF_STATX_DEV "." MFF_STATX_DEVICE_MINOR;
    }

    errno = ENOTSUP;
    return NULL;
}

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
        return statx2str(field->statx);
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
    case RBH_FP_ADD:
        return NULL;
    }

    errno = ENOTSUP;
    return NULL;
}

#define FIELD_BUFFER_SIZE 64

bool
bson_append_rbh_field(bson_t *bson, const char *key, size_t key_length,
                      const struct rbh_filter_field *field)
{
    char *fieldA_buffer = malloc(FIELD_BUFFER_SIZE);
    char *fieldB_buffer = malloc(FIELD_BUFFER_SIZE);
    char fieldA_str[32];
    char fieldB_str[32];
    char second_idx[4];
    char first_idx[4];
    bson_t document;
    bson_t array;

    snprintf(fieldA_str, sizeof(fieldA_str), "$%s",
             field2str(field->compute.fieldA, (char **)&fieldA_buffer,
                       FIELD_BUFFER_SIZE));
    snprintf(fieldB_str, sizeof(fieldB_str), "$%s",
             field2str(field->compute.fieldB, (char **)&fieldB_buffer,
                       FIELD_BUFFER_SIZE));

    if (!(bson_append_document_begin(bson, key, key_length, &document) &&
          bson_append_array_begin(&document, "$add", strlen("$add"), &array)))
        return false;

    key_length = bson_uint32_to_string(0, &key, first_idx, sizeof(first_idx));
    if (!bson_append_utf8(&array, key, key_length, fieldA_str,
                          strlen(fieldA_str)))
        return false;

    key_length = bson_uint32_to_string(1, &key, second_idx, sizeof(second_idx));
    if (!bson_append_utf8(&array, key, key_length, fieldB_str,
                          strlen(fieldB_str)))
        return false;

    free(fieldA_buffer);
    free(fieldB_buffer);

    return bson_append_array_end(&document, &array) &&
        bson_append_document_end(bson, &document);
}
