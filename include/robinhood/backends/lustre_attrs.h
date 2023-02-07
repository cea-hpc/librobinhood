/* This file is part of the RobinHood Library
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef RBH_LUSTRE_ATTRS_H
#define RBH_LUSTRE_ATTRS_H

/**
 * @file
 *
 * Header which externalizes the retrieval of lustre information.
 */

#include "robinhood/sstack.h"

/*----------------------------------------------------------------------------*
 |                                lustre tool                                 |
 *----------------------------------------------------------------------------*/

ssize_t
lustre_get_attrs(const int fd, const uint16_t mode,
                 struct rbh_value_pair *pairs, struct rbh_sstack *values);

#endif
