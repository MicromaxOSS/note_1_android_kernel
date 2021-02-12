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

#ifndef DEBUSSY_TYPE_PSRAM
#include "debussy_intf.h"

#define DEBUSSY_FLASH_PAGE_SZ       (4096)
#define DEBUSSY_FLASH_SECTOR_SZ     (65536)
#define DEBUSSY_FLASH_POLLING_CNT   (300 / 2)     // about 300ms
#define DEBUSSY_WRITE_PAGE_SIZE     (256)

//#define DEBUSSY_FLASH_WRITE_READBACK_CHECK

static uint32_t max_write_page_size_i2c = 256;
static uint32_t max_write_page_size_spi = 256;

static void _debussy_flash_control(struct device* dev, unsigned int cmd, unsigned int faddr,
    unsigned int baddr, unsigned int len, unsigned int f2b)
{
    struct i2c_client *client = i2c_verify_client(dev);
    uint32_t buf[5];

    buf[0] = cmd;
    buf[1] = faddr;
    buf[2] = baddr;
    buf[3] = len;
    buf[4] = f2b;

    if (NULL != client) {
        igo_i2c_write_buffer(client, 0x2A013024, buf, 5);
    }
    else {
        igo_spi_write_buffer(0x2A013024, buf, 5);
    }
}

static unsigned int _debussy_flash_readStatus(struct device* dev, unsigned int cmd)
{
    unsigned int ret = 1;
    int resp;
    struct i2c_client *client = i2c_verify_client(dev);

    _debussy_flash_control(dev, 0x410A0000 + cmd, 0, 0x2A0C0000, 4, 1);

    if (NULL != client) {
        resp = igo_i2c_read(client, 0x2A0C0000, &ret);
    }
    else {
        resp = igo_spi_read(0x2A0C0000, &ret);
    }

    if (0 != resp) {
        ret = 1;
    }

    return ret;
}

static unsigned int _debussy_flash_pollingDone(struct device* dev)
{
    int count;
    unsigned int ret;

    count = 0;
    // About 300ms
    while (count < DEBUSSY_FLASH_POLLING_CNT) {
        ret = _debussy_flash_readStatus(dev, 0x05);
        if ((ret & 0x1) == 0x0)
            break;
        usleep_range(1999, 2000);       // 2ms
        count++;
    }

    if (count == DEBUSSY_FLASH_POLLING_CNT) {
        dev_err(dev, "debussy: wait flash complete timeout\n");
        return 1;
    }

    return 0;
}

static void _debussy_flash_wren(struct device* dev)
{
    _debussy_flash_control(dev, 0x400E0006, 0, 0, 0, 0);
}

static unsigned int debussy_flash_page_erase(struct device* dev, unsigned int faddr)
{
    _debussy_flash_wren(dev);
    _debussy_flash_control(dev, 0x500E0020, faddr, 0, 0, 0);
    return _debussy_flash_pollingDone(dev);
}

static unsigned int debussy_flash_sector_erase(struct device* dev, unsigned int faddr)
{
    _debussy_flash_wren(dev);
    _debussy_flash_control(dev, 0x500E00D8, faddr, 0, 0, 0);
    return _debussy_flash_pollingDone(dev);
}

static unsigned int debussy_flash_32k_erase(struct device* dev, unsigned int faddr)
{
    _debussy_flash_wren(dev);
    _debussy_flash_control(dev, 0x500E0052, faddr, 0, 0, 0);
    return _debussy_flash_pollingDone(dev);
}

static unsigned int _debussy_flash_writepage(struct device* dev, unsigned int faddr, unsigned int* data)
{
    unsigned int cmd;
    int is_ignore, i;
    struct i2c_client *client = i2c_verify_client(dev);

    cmd = 0x530C0032;
    is_ignore = 1;

    for (i = 0; i < (DEBUSSY_WRITE_PAGE_SIZE >> 2); ++i) {
        if (data[i] != 0xffffffff)
            is_ignore = 0;
    }

    if (!is_ignore) {
        unsigned int j;

        _debussy_flash_wren(dev);

        if (NULL != client) {
            for (j = 0; j < (DEBUSSY_WRITE_PAGE_SIZE / max_write_page_size_i2c); j++) {
                if (0 != igo_i2c_write_buffer(client, 0x2A0C0004 + j * max_write_page_size_i2c,
                                              &data[j * (max_write_page_size_i2c >> 2)],
                                              max_write_page_size_i2c >> 2)) {
                    dev_err(dev, "%s: igo_i2c_write_buffer fail\n", __func__);
                    return 1;
                }

#ifdef DEBUSSY_FLASH_WRITE_READBACK
                {
                    uint32_t *readback_data = devm_kzalloc(dev, DEBUSSY_WRITE_PAGE_SIZE >> 2, GFP_KERNEL );
                    uint32_t count;

                    if (0 == igo_i2c_read_buffer(client, 0x2A0C0004 + j * max_write_page_size_i2c,
                                                 readback_data, max_write_page_size_i2c >> 2)) {
                        for (count = 0; count < max_write_page_size_i2c >> 2; count++) {
                            if (readback_data[count] != data[j * (max_write_page_size_i2c >> 2) + count]) {
                                dev_err(dev, "faddr = 0x%08X, [%08X %08X]\n",
                                        faddr,
                                        readback_data[count],
                                        data[j * (max_write_page_size_i2c >> 2) + count]);
                            }
                        }
                    }
                    else {
                        dev_err(dev, "[%s %d]: igo_i2c_read_buffer fail\n", __func__,__LINE__);
                    }
                    devm_kfree(readback_data);
                }
#endif
            }
        }
        else {
            for (j = 0; j < (DEBUSSY_WRITE_PAGE_SIZE / max_write_page_size_spi); j++) {
                if (0 != igo_spi_write_buffer(0x2A0C0004 + j * max_write_page_size_spi,
                                              &data[j * (max_write_page_size_spi >> 2)],
                                              max_write_page_size_spi >> 2)) {
                    dev_err(dev, "%s: igo_spi_write_buffer fail\n", __func__);
                    return 1;
                }
            }

#ifdef DEBUSSY_FLASH_WRITE_READBACK_CHECK
            {
                uint32_t *readback_data = devm_kzalloc(dev, DEBUSSY_WRITE_PAGE_SIZE >> 2, GFP_KERNEL);    
                uint32_t count;

                for (j = 0; j < (DEBUSSY_WRITE_PAGE_SIZE / max_write_page_size_spi); j++) {
                    if (0 == igo_spi_read_buffer(0x2A0C0004 + j * max_write_page_size_spi,
                                                 readback_data, max_write_page_size_spi >> 2)) {
                        for (count = 0; count < max_write_page_size_spi >> 2; count++) {
                            if (readback_data[count] != data[j * (max_write_page_size_spi >> 2) + count]) {
                                dev_err(dev, "faddr = 0x%08X, [%08X %08X]\n",
                                        faddr,
                                        readback_data[count],
                                        data[j * (max_write_page_size_spi >> 2) + count]);
                            }
                        }
                    }
                    else {
                        dev_err(dev, "[%s %d]: igo_spi_read_buffer fail\n", __func__,__LINE__);
                    }
                }
                devm_kfree(readback_data); 
            }
#endif
        }

        _debussy_flash_control(dev, cmd, faddr, 0x2A0C0004, DEBUSSY_WRITE_PAGE_SIZE, 0);
        return _debussy_flash_pollingDone(dev);
    }

    return 0;
}

static unsigned int _debussy_flash_erase(struct device* dev, unsigned int faddr, size_t write_size)
{
    int i;
    unsigned int erase_64k_num;
    unsigned int erase_32k_num;
    unsigned int erase_4k_num;

    erase_64k_num = write_size / DEBUSSY_FLASH_SECTOR_SZ;
    erase_32k_num = (write_size % DEBUSSY_FLASH_SECTOR_SZ) / 32768;
    erase_4k_num = (write_size % 32768) / DEBUSSY_FLASH_PAGE_SZ;

    if ((write_size % 32768) % DEBUSSY_FLASH_PAGE_SZ > 0) {
        ++erase_4k_num;
    }

    for (i = 0; i < erase_64k_num; ++i) {
        if (0 != debussy_flash_sector_erase(dev, faddr)) {
            return 1;
        }
        faddr += DEBUSSY_FLASH_SECTOR_SZ;
    }

    for (i = 0; i < erase_32k_num; ++i) {
        if (0 != debussy_flash_32k_erase(dev, faddr)) {
            return 1;
        }
        faddr += 32768;
    }

    for (i = 0; i < erase_4k_num; ++i) {
        if (0 != debussy_flash_page_erase(dev, faddr)) {
            return 1;
        }
        faddr += DEBUSSY_FLASH_PAGE_SZ;
    }

    return 0;
}

static unsigned int _flash_write_bin(struct device* dev, unsigned int faddr, const u8* data, size_t size)
{
    int page_num, left_cnt;
    int idx, i, j;
    int write_cnt;
    unsigned int wdata[DEBUSSY_WRITE_PAGE_SIZE >> 2];
    int percent = -1;

    write_cnt = 0;
    page_num = (size / sizeof(char)) / DEBUSSY_WRITE_PAGE_SIZE;
    left_cnt = (size / sizeof(char)) % DEBUSSY_WRITE_PAGE_SIZE;

    if (0 != _debussy_flash_erase(dev, faddr, size)) {
        return 1;
    }

    for (i = 0; i < page_num; ++i) {
        for (j = 0; j < (DEBUSSY_WRITE_PAGE_SIZE >> 2); ++j) {
            idx = i * DEBUSSY_WRITE_PAGE_SIZE + j * 4;
            wdata[j] = (data[idx + 3] << 24) | (data[idx + 2] << 16) | (data[idx + 1] << 8) | (data[idx]);
        }

        //printk("debussy to flash writepage\n");
        if (0 != _debussy_flash_writepage(dev, faddr + write_cnt, wdata)) {
            dev_err(dev, "%s: _debussy_flash_writepage fail\n", __func__);
            return 1;
        }

        write_cnt += DEBUSSY_WRITE_PAGE_SIZE;

        if ((write_cnt * 10 / size) != percent) {
            percent = write_cnt * 10 / size;
            dev_info(dev, "%s: %d%%\n", __func__, percent * 10);
        }
    }

    if (left_cnt != 0) {
        memset_u32(wdata, 0xFFFFFFFF, sizeof(wdata) / sizeof(uint32_t));

        for (j = 0; j < (left_cnt / 4); ++j) {
            idx = i * DEBUSSY_WRITE_PAGE_SIZE + j * 4;
            wdata[j] = (data[idx + 3] << 24) | (data[idx + 2] << 16) | (data[idx + 1] << 8) | (data[idx]);
        }

        //printk("debussy flash write page\n");
        if (0 != _debussy_flash_writepage(dev, faddr + write_cnt, wdata)) {
            dev_err(dev, "%s: _debussy_flash_writepage fail\n", __func__);
            return 1;
        }

        if ((write_cnt * 10 / size) != percent) {
            percent = write_cnt * 10 / size;
            dev_info(dev, "%s: %d%%\n", __func__, percent * 10);
        }
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
int debussy_flash_update_firmware(struct device* dev, unsigned int faddr, const u8* data, size_t size)
{
    int ret = 1;
    struct i2c_client *client = i2c_verify_client(dev);
    struct debussy_priv* debussy = i2c_get_clientdata(client);

    dev_info(dev, "%s: Download to address  0x%08x\n", __func__, faddr);

    if (0 == debussy->max_data_length_i2c)
        debussy->max_data_length_i2c = DEBUSSY_WRITE_PAGE_SIZE;
    else if (DEBUSSY_WRITE_PAGE_SIZE < debussy->max_data_length_i2c)
        debussy->max_data_length_i2c = DEBUSSY_WRITE_PAGE_SIZE;

    max_write_page_size_i2c = (debussy->max_data_length_i2c >> 2) << 2;

    if (0 == debussy->max_data_length_spi)
        debussy->max_data_length_spi = DEBUSSY_WRITE_PAGE_SIZE;
    else if (DEBUSSY_WRITE_PAGE_SIZE < debussy->max_data_length_spi)
        debussy->max_data_length_spi = DEBUSSY_WRITE_PAGE_SIZE;

    max_write_page_size_spi = (debussy->max_data_length_spi >> 2) << 2;

    /* Update firmware init sequence */
    igo_i2c_write(client, 0x2A00003C, 0);           // LV3
    igo_i2c_write(client, 0x2A000018, 1);           // HOLD
    igo_i2c_write(client, 0x2A000030, 1);           // LV1
    msleep(500);
    igo_i2c_write(client, 0x2A00003C, 0x830001);    // LV3
    igo_i2c_write(client, 0x2A013020, 0x4);         // AGN_BRUST
    igo_i2c_write(client, 0x2A0130C8, 0x5500);      // PHY_CTL
    igo_i2c_write(client, 0x2A013048, 0x0);         // AGN_PWD

    if (debussy->spi_dev && (0 == igo_spi_intf_enable(1))) {
        // tmp_data = 0x03;        // enable HFOSC
        // igo_i2c_write(client, 0x2A01C028, &tmp_data, 1);
        // tmp_data = 0x03;        // set sysck div to (3+1)
        // igo_i2c_write(client, 0x2A00005C, &tmp_data, 1);
        // tmp_data = 0x01;        // set sysck to HFOSC(240/4 = 80mhz)
        // igo_i2c_write(client, 0x2A000058, &tmp_data, 1);

        dev_info(dev, "%s: Enable SPI Interface \n", __func__ );
        ret = _flash_write_bin(debussy->spi_dev, faddr, data, size);
    }
    else {
        ret = _flash_write_bin(dev, faddr, data, size);
    }

    if (ret == 0) {
        dev_info(dev, "Debussy FW update done\n");
        debussy->mcu_hold(dev, 0);
        debussy->reset_chip(dev, 1);
    }
    else {
        dev_info(dev, "Debussy FW update fail\n");
    }

    return ret;
}
#endif  /* end of DEBUSSY_TYPE_PSRAM not defined */


