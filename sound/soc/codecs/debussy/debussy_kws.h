/*
 * Copyright (C) 2017-2019 Intelligo Technology Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef DEBUSSY_KWS_H
#define DEBUSSY_KWS_H

#include "debussy.h"
#include "debussy_customer.h"
int debussy_kws_init(struct debussy_priv* debussy);
extern void debussy_kws_hit(struct debussy_priv* debussy);
//void debussy_kws_enable(void);
//void debussy_kws_disable(void);

#endif
