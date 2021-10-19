/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ROBINHOOD_STATX_H
#define ROBINHOOD_STATX_H

/** @file
 * Definition of robinhood-specific extensions for statx
 */

#define CHECK_GLIBC_VERSION(_major, _minor) \
    (__GLIBC__ >= _major && __GLIBC_MINOR__ >= _minor)

#define XATTR_VALUE_MAX_LEN 4096

#define RBH_STATX_TYPE          0x00000001U
#define RBH_STATX_MODE          0x00000002U
#define RBH_STATX_NLINK         0x00000004U
#define RBH_STATX_UID           0x00000008U
#define RBH_STATX_GID           0x00000010U
#define RBH_STATX_ATIME_SEC     0x00000020U
#define RBH_STATX_MTIME_SEC     0x00000040U
#define RBH_STATX_CTIME_SEC     0x00000080U
#define RBH_STATX_INO           0x00000100U
#define RBH_STATX_SIZE          0x00000200U
#define RBH_STATX_BLOCKS        0x00000400U
#define RBH_STATX_BTIME_SEC     0x00000800U
#define RBH_STATX_MNT_ID        0x00001000U
#define RBH_STATX_BLKSIZE       0x40000000U
#define RBH_STATX_ATTRIBUTES    0x20000000U
#define RBH_STATX_ATIME_NSEC    0x10000000U
#define RBH_STATX_BTIME_NSEC    0x08000000U
#define RBH_STATX_CTIME_NSEC    0x04000000U
#define RBH_STATX_MTIME_NSEC    0x02000000U
#define RBH_STATX_RDEV_MAJOR    0x01000000U
#define RBH_STATX_RDEV_MINOR    0x00800000U
#define RBH_STATX_DEV_MAJOR     0x00400000U
#define RBH_STATX_DEV_MINOR     0x00200000U

#define RBH_STATX_ATIME         0x10000020U
#define RBH_STATX_BTIME         0x08000800U
#define RBH_STATX_CTIME         0x04000080U
#define RBH_STATX_MTIME         0x02000040U
#define RBH_STATX_RDEV          0x01800000U
#define RBH_STATX_DEV           0x00600000U

#define RBH_STATX_BASIC_STATS   0x57e007ffU
#define RBH_STATX_ALL           0x7fe01fffU

#endif
