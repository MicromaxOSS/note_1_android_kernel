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

#ifndef DEBUSSY_SPIDEV_H
#define DEBUSSY_SPIDEV_H

#include <linux/device.h>

extern ssize_t debussy_spidrv_buffer_read(uint32_t address, uint32_t *data, size_t word_len);
extern ssize_t debussy_spidrv_buffer_write(uint32_t address, uint32_t *data, size_t word_len);

#define debussy_spidrv_reg_read(address, retData)      debussy_spidrv_buffer_read(address, retData, 1)
#define debussy_spidrv_reg_write(address, data)        debussy_spidrv_buffer_write(address, data, 1)

extern int debussy_spidrv_intf_enable(uint32_t enable);
extern int debussy_spidrv_intf_check(void);
extern int debussy_spidev_init(void);
extern void debussy_spidev_exit(void);

#endif
