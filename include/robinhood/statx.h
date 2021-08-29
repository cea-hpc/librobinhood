/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ROBINHOOD_STATX_H
#define ROBINHOOD_STATX_H

/** @file
 * Definition of robinhood-specific extensions for statx
 */

#define RBH_STATX_BLKSIZE       0x80000000U
#define RBH_STATX_ATTRIBUTES    0x40000000U
#define RBH_STATX_ATIME_NSEC    0x20000000U
#define RBH_STATX_BTIME_NSEC    0x10000000U
#define RBH_STATX_CTIME_NSEC    0x08000000U
#define RBH_STATX_MTIME_NSEC    0x04000000U
#define RBH_STATX_RDEV_MAJOR    0x02000000U
#define RBH_STATX_RDEV_MINOR    0x01000000U
#define RBH_STATX_DEV_MAJOR     0x00800000U
#define RBH_STATX_DEV_MINOR     0x00400000U
#define RBH_STATX_ALL           0xffc00000U

#endif
