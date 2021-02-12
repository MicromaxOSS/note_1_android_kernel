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
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/string.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include "debussy.h"

void memcpy_u32(uint32_t *target, uint32_t *src, uint32_t word_len) {
    uint32_t i;

    for (i = 0; i < word_len; i++) {
        *(target + i) = *(src + i);
    }
}

void memset_u32(uint32_t *target, uint32_t data, uint32_t word_len) {
    uint32_t i;

    for (i = 0; i < word_len; i++) {
        *(target + i) = data;
    }
}

void endian_swap(unsigned int *target, unsigned int *source, unsigned int word_len) {
#if 0
    unsigned int i;
    unsigned char temp[4];
    unsigned char *p_byte;

    for (i = 0; i < word_len; i++) {
        p_byte = (unsigned char *) (source + i);
        temp[0] = *(p_byte + 3);
        temp[1] = *(p_byte + 2);
        temp[2] = *(p_byte + 1);
        temp[3] = *(p_byte + 0);
        *(target + i) = *(unsigned int *) temp;
        // temp =  (source[i] >> 24) & 0x000000FF;
        // temp += (source[i] >>  8) & 0x0000FF00;
        // temp += (source[i] <<  8) & 0x00FF0000;
        // temp += (source[i] << 24) & 0xFF000000;
        // target[i] = temp;
    }
#else
    unsigned int i;
    unsigned int temp;

    for (i = 0; i < word_len; i++) {
        temp =  (source[i] >> 24) & 0x000000FF;
        temp += (source[i] >>  8) & 0x0000FF00;
        temp += (source[i] <<  8) & 0x00FF0000;
        temp += (source[i] << 24) & 0xFF000000;
        target[i] = temp;
    }
#endif
}
