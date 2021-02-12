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
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/regmap.h>

#include "debussy.h"
#include "debussy_intf.h"

int igo_i2c_read_buffer(struct i2c_client *client,
                        unsigned int addr, unsigned int *buff,
                        unsigned int word_len)
{
    struct debussy_priv* debussy = i2c_get_clientdata(client);
    unsigned int *input_data = NULL;
    unsigned char buf[4];
    struct i2c_msg xfer[2];
    int ret;

    if (!buff || !word_len)
        return -EFAULT;

    memset_u32(buff, 0, word_len);

#ifdef ENABLE_DEBUSSY_I2C_REGMAP
    if (debussy->i2c_regmap) {
        return regmap_raw_read(debussy->i2c_regmap, addr, buff, word_len << 2);
    }
#endif

    buf[0] = (unsigned char)(addr >> 24);
    buf[1] = (unsigned char)(addr >> 16);
    buf[2] = (unsigned char)(addr >> 8);
    buf[3] = (unsigned char)(addr & 0xff);

    memset(xfer, 0, sizeof(xfer));

    xfer[0].addr = client->addr;
    xfer[0].flags = 0;
    xfer[0].len = 4;
    xfer[0].buf = buf;

    xfer[1].addr = client->addr;
    xfer[1].flags = I2C_M_RD;
    xfer[1].len = word_len << 2;

    if (debussy->isLittleEndian) {
        xfer[1].buf = (u8 *) buff;
    }
    else {
        input_data = (unsigned int *) devm_kzalloc(debussy->dev, word_len << 2, GFP_KERNEL);

        if (NULL == input_data) {
            dev_err(debussy->dev, "%s: alloc fail\n", __func__);
            return -EFAULT;
        }

        xfer[1].buf = (u8 *) input_data;
    }

    ret = i2c_transfer(client->adapter, xfer, 2);

    if (ret < 0) {
        pr_err("debussy: %s - i2c_transfer error => %d, Reg = 0x%X\n", __func__, ret, addr);

        if (input_data) {
            devm_kfree(&client->dev, input_data);
        }

        return ret;
    }
    else if (ret != 2) {
        pr_err("debussy: %s - i2c_transfer error => -EIO, Reg = 0x%X\n", __func__, addr);

        if (input_data) {
            devm_kfree(&client->dev, input_data);
        }

        return -EIO;
    }

    if (input_data) {
        if (0 == debussy->isLittleEndian) {
            endian_swap(buff, input_data, word_len);
        }

        devm_kfree(&client->dev, input_data);
    }

    return 0;
}

#if 0
int igo_i2c_read(struct i2c_client *client,
                 unsigned int addr,
                 unsigned int *value)
{
    struct debussy_priv* debussy = i2c_get_clientdata(client);
    unsigned char buf[4];
    struct i2c_msg xfer[2];
    unsigned int regVal;
    int ret;

    if (!value)
        return -EFAULT;

    *value = 0;

#ifdef ENABLE_DEBUSSY_I2C_REGMAP
    if (debussy->i2c_regmap) {
        return regmap_read(debussy->i2c_regmap, addr, value);
    }
#endif

    memset(xfer, 0, sizeof(xfer));

    buf[0] = (unsigned char)(addr >> 24);
    buf[1] = (unsigned char)(addr >> 16);
    buf[2] = (unsigned char)(addr >> 8);
    buf[3] = (unsigned char)(addr & 0xff);

    xfer[0].addr = client->addr;
    xfer[0].flags = 0;
    xfer[0].len = 4;
    xfer[0].buf = buf;

    xfer[1].addr = client->addr;
    xfer[1].flags = I2C_M_RD;
    xfer[1].len = 4;
    xfer[1].buf = (u8 *) &regVal;

    ret = i2c_transfer(client->adapter, xfer, 2);
    if (ret < 0) {
        pr_err("debussy: %s - i2c_transfer error => %d, Reg = 0x%X\n", __func__, ret, addr);
        return ret;
    }
    else if (ret != 2) {
        pr_err("debussy: %s - i2c_transfer error => -EIO, Reg = 0x%X\n", __func__, addr);
        return -EIO;
    }

    if (debussy->isLittleEndian) {
        *value = regVal;
    }
    else {
        endian_swap(value, &regVal, 1);
    }

    return 0;
}
#else
int igo_i2c_read(struct i2c_client *client,
                 unsigned int addr,
                 unsigned int *value)
{
    // system crash, why ?
    return igo_i2c_read_buffer(client, addr, value, 1);
}
#endif

int igo_i2c_write(struct i2c_client *client,
                  unsigned int addr, unsigned int value)
{
    return igo_i2c_write_buffer(client, addr, &value, 1);
}

int igo_i2c_write_buffer(struct i2c_client *client,
                        unsigned int addr, unsigned int *buff,
                        unsigned int word_len)
{
    struct debussy_priv *debussy = i2c_get_clientdata(client);
    unsigned char *buf;
    struct i2c_msg xfer[1];
    int ret;

    if (!buff || !word_len)
        return -EFAULT;

#ifdef ENABLE_DEBUSSY_I2C_REGMAP
    if (debussy->i2c_regmap) {
        return regmap_raw_write(debussy->i2c_regmap, addr, buff, word_len << 2);
    }
#endif

    buf = (unsigned char *) devm_kzalloc(&client->dev, (word_len << 2) + 4, GFP_KERNEL);

    if (NULL == buf) {
        pr_err("debussy: %s: alloc fail\n", __func__);
        return -EFAULT;
    }

    buf[0] = (unsigned char) (addr >> 24) | 0x20;
    buf[1] = (unsigned char) (addr >> 16);
    buf[2] = (unsigned char) (addr >> 8);
    buf[3] = (unsigned char) (addr & 0xFF);

    if (debussy->isLittleEndian) {
        memcpy_u32((uint32_t *) &buf[4], buff, word_len);
    }
    else {
        endian_swap((unsigned int *) &buf[4], buff, word_len);
    }

    memset(xfer, 0, sizeof(xfer));

    xfer[0].addr = client->addr;
    xfer[0].flags = 0;
    xfer[0].len = 4 + word_len * 4;
    xfer[0].buf = (u8 *) buf;

    ret = i2c_transfer(client->adapter, xfer, 1);
    devm_kfree(&client->dev, buf);

    if (ret < 0) {
        pr_err("debussy: %s - i2c_transfer error => %d, Reg = 0x%X\n", __func__, ret, addr);
        return ret;
    }
    else if (ret != 1) {
        pr_err("debussy: %s - i2c_transfer error => -EIO, Reg = 0x%X\n", __func__, addr);
        return -EIO;
    }

    return 0;
}
