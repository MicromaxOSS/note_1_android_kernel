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

#ifndef DEBUSSY_INTF_H
#define DEBUSSY_INTF_H

#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

typedef enum {
    IGO_CH_ACTION_NOP = 0,
    IGO_CH_ACTION_READ = 1,
    IGO_CH_ACTION_WRITE = 2,
    IGO_CH_ACTION_BATCH = 3,
} IGO_CH_ACTION_E;

typedef enum {
    IGO_CH_STATUS_NOP = 0,
    IGO_CH_STATUS_CMD_RDY = 1,
    IGO_CH_STATUS_DONE = 2,
    IGO_CH_STATUS_BUSY = 3,
    IGO_CH_STATUS_ACTION_ERROR = 4,
    IGO_CH_STATUS_ADDR_ERROR = 5,
    IGO_CH_STATUS_CFG_ERROR = 6,
    IGO_CH_STATUS_CFG_NOT_SUPPORT = 7,
    IGO_CH_STATUS_TIMEOUT = 8,

    // keeping in the last position
    IGO_CH_STATUS_MAX
} IGO_CH_STATUS_E;

extern int igo_i2c_read(struct i2c_client *client, unsigned int addr, unsigned int *value);
extern int igo_i2c_write(struct i2c_client *client, unsigned int addr, unsigned int value);
extern int igo_i2c_read_buffer(struct i2c_client *client, unsigned int addr, unsigned int *buff, unsigned int word_len);
extern int igo_i2c_write_buffer(struct i2c_client *client, unsigned int addr, unsigned int *buff, unsigned int word_len);

extern int igo_spi_read_buffer(unsigned int addr, unsigned int *buff, unsigned int word_len);
extern int igo_spi_read(unsigned int addr, unsigned int *value);
extern int igo_spi_write(unsigned int addr, unsigned int value);
extern int igo_spi_write_buffer(unsigned int addr, unsigned int *buff, unsigned int word_len);
extern int igo_spi_intf_enable(uint32_t enable);
extern int igo_spi_intf_check(void);
extern void igo_spi_intf_init(void);
extern void igo_spi_intf_exit(void);

extern int igo_ch_write(struct device* dev, unsigned int reg, unsigned int data);
extern int igo_ch_read(struct device* dev, unsigned int reg, unsigned int *data);
extern int igo_ch_batch_write(struct device* dev, unsigned int cmd_index,
                              unsigned int *data, unsigned int data_length);
extern int igo_ch_batch_finish_write(struct device* dev, unsigned int num_of_cmd);
extern int igo_ch_write_wait(struct device* dev, unsigned int reg, unsigned int data, unsigned int wait_time);
extern int igo_ch_buf_write(struct device* dev, unsigned int addr, unsigned int *data, unsigned int word_len);
extern int igo_ch_buf_read(struct device* dev, unsigned int addr, unsigned int *data, unsigned int word_len);
extern int igo_ch_buf_read_spi(struct device* dev, unsigned int addr, unsigned int *data, unsigned int word_len);

#define igo_ch_slow_write(dev, cmd, value)              igo_ch_write(dev, cmd, value)

#define IG_BUF_RW_LEN                                   (256)       // bytes

#endif
