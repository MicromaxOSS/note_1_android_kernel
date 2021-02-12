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

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include "debussy_intf.h"
#include "debussy.h"
#ifdef ENABLE_SPI_INTF
#include "debussy_spidev.h"
#endif

int igo_spi_read_buffer(unsigned int addr, unsigned int *buff, unsigned int word_len)
{
#ifdef ENABLE_SPI_INTF
    int ret = 0;

    if (!buff || !word_len)
        return -EFAULT;

    memset((void *) buff, 0, word_len << 2);
    ret = debussy_spidrv_buffer_read(addr, buff, word_len);

    if (ret)
        return 0;

    return ret;
#else
    return 1;
#endif
}

int igo_spi_read(unsigned int addr, unsigned int *value)
{
#ifdef ENABLE_SPI_INTF
    int ret;

    if (!value)
        return -EFAULT;

    *value = 0;
    ret = debussy_spidrv_reg_read(addr, value);

    if (ret)
        return 0;

    return ret;
#else
    return 1;
#endif
}

int igo_spi_write(unsigned int addr, unsigned int value) {
    return igo_spi_write_buffer(addr, &value, 1);
}

int igo_spi_write_buffer(unsigned int addr, unsigned int *buff, unsigned int word_len)
{
#ifdef ENABLE_SPI_INTF
    ssize_t ret;

    if (!buff)
        return -EFAULT;

    ret = debussy_spidrv_buffer_write(addr, buff, word_len);

    if (ret)
        return 0;

    return ret;
#else
    return 1;
#endif
}

int igo_spi_intf_enable(uint32_t enable) {
    #ifdef ENABLE_SPI_INTF
    return debussy_spidrv_intf_enable(enable);
    #else
    return 1;
    #endif
}

int igo_spi_intf_check(void) {
    #ifdef ENABLE_SPI_INTF
    return debussy_spidrv_intf_check();
    #else
    return 1;
    #endif
}

void igo_spi_intf_init(void) {
    #ifdef ENABLE_SPI_INTF
    debussy_spidev_init();
    #endif
}

void igo_spi_intf_exit(void) {
    #ifdef ENABLE_SPI_INTF
    debussy_spidev_exit();
    #endif
}
