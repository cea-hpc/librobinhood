/* This file is part of the RobinHood Library
 * Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_RING_H
#define RBH_RING_H

/**
 * @file
 *
 * Internal header to let the ring reader use ring fields.
 */

#include <stddef.h>

struct rbh_ring {
    size_t size;
    char *head;
    size_t used;
    char *data;
};

#endif
