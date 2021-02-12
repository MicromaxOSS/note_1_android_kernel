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
#include "debussy_intf.h"


#ifdef DEBUSSY_TYPE_PSRAM
#define DEBUSSY_WRITE_PAGE_SIZE         (256)

//#define DEBUSSY_PSRAM_WRITE_READBACK_CHECK

static uint32_t max_write_page_size_i2c = 256;
static uint32_t max_write_page_size_spi = 256;

static int _debussy_psram_control(struct device* dev, unsigned int cmd, unsigned int faddr,
                                  unsigned int baddr, unsigned int len,
                                  unsigned int f2b)
{
    uint32_t buf[5];
    struct i2c_client *client = i2c_verify_client(dev);

    buf[0] = cmd;
    buf[1] = faddr;
    buf[2] = baddr;
    buf[3] = len;
    buf[4] = f2b;

    if (NULL != client) {
        return igo_i2c_write_buffer(client, 0x2A013024, (unsigned int *) &buf, 5);
    }
    else {
        return igo_spi_write_buffer(0x2A013024, (unsigned int *) &buf, 5);
    }

    return 1;
}

static unsigned int _debussy_psram_writepage(struct device* dev,
                                             unsigned int mem_addr,
                                             unsigned int* data)
{
    unsigned int i;
    struct i2c_client *client = i2c_verify_client(dev);

    if (NULL != client) {
        for (i = 0; i < (DEBUSSY_WRITE_PAGE_SIZE / max_write_page_size_i2c); i++) {
            if (0 != igo_i2c_write_buffer(client, mem_addr + max_write_page_size_i2c * i,
                                          &data[i * (max_write_page_size_i2c >> 2)],
                                          max_write_page_size_i2c >> 2)) {
                return 1;
            }
        }

#ifdef DEBUSSY_PSRAM_WRITE_READBACK_CHECK
        {
            uint32_t *readback, fail_count = 0;

            readback = devm_kzalloc(dev, DEBUSSY_WRITE_PAGE_SIZE, GFP_KERNEL );

            if (readback) {
                memset_u32(readback, 0, DEBUSSY_WRITE_PAGE_SIZE >> 2);
                for (i = 0; i < (DEBUSSY_WRITE_PAGE_SIZE / max_write_page_size_spi); i++) {
                    igo_i2c_read_buffer(client, mem_addr + max_write_page_size_spi * i,
                                        &readback[i * (max_write_page_size_spi >> 2)],
                                        max_write_page_size_spi >> 2);
                }

                for (i = 0; i < (DEBUSSY_WRITE_PAGE_SIZE >> 2); i++) {
                    if (*(readback + i) != *(data + i)) {
                        dev_err(dev, "%s: Org: 0%08X, Readback: 0%08X\n", __func__, *(data + i), *(readback + i));
                        fail_count++;
                    }
                }

                devm_kfree(dev, readback);
            }
        }
#endif

        return 0;
    }
    else {
        for (i = 0; i < (DEBUSSY_WRITE_PAGE_SIZE / max_write_page_size_spi); i++) {
            if (0 != igo_spi_write_buffer(mem_addr + max_write_page_size_spi * i,
                                          &data[i * (max_write_page_size_spi >> 2)],
                                          max_write_page_size_spi >> 2)) {
                return 1;
            }
        }

#ifdef DEBUSSY_PSRAM_WRITE_READBACK_CHECK
        {
            uint32_t *readback, fail_count = 0;

            readback = devm_kzalloc(dev, DEBUSSY_WRITE_PAGE_SIZE, GFP_KERNEL);

            if (readback) {
                memset_u32(readback, 0, DEBUSSY_WRITE_PAGE_SIZE >> 2);
                for (i = 0; i < (DEBUSSY_WRITE_PAGE_SIZE / max_write_page_size_spi); i++) {
                    igo_spi_read_buffer(mem_addr + max_write_page_size_spi * i,
                                        &readback[i * (max_write_page_size_spi >> 2)],
                                        max_write_page_size_spi >> 2);
                }

                for (i = 0; i < (DEBUSSY_WRITE_PAGE_SIZE >> 2); i++) {
                    if (*(readback + i) != *(data + i)) {
                        dev_err(dev, "%s: Org: 0%08X, Readback: 0%08X\n", __func__, *(data + i), *(readback + i));
                        fail_count++;
                    }
                }

                devm_kfree(dev, readback);
            }
        }
#endif

        return 0;
    }

    return 1;
}

static int _debussy_psram_pollingDone(struct device* dev)
{
    uint32_t status, count, k = 5;
    struct i2c_client *client = i2c_verify_client(dev);

    while (k--) {
        if (NULL != client) {
            igo_i2c_read(client, 0x2A013044, &status);
            igo_i2c_read(client, 0x2A01304C, &count);
        }
        else {
            igo_spi_read(0x2A013044, &status);
            igo_spi_read(0x2A01304C, &count);
        }

    // dev_err(dev, "s%08X c%08X", status, count);

    #if 1
        if (((status & 0x01) == 0) && (0 == count)) {
            return 0;
        }
    #else
        if (0 == count) {
            return 0;
        }
    #endif
        usleep_range(499, 500);
    }

    dev_err(dev, "%s: Timeout (CC)\n", __func__);

    return 1;
}

static int _debussy_psram_flush(struct device* dev, unsigned int faddr, unsigned int mem_addr)
{
    _debussy_psram_pollingDone(dev);
    return _debussy_psram_control(dev, 0x510C0002, faddr, mem_addr, DEBUSSY_WRITE_PAGE_SIZE, 0);
}

static int _psram_write_bin(struct device* dev, unsigned int faddr, const u8* data, size_t size)
{
    int total_page_num, page_index, left_cnt;
    int idx, word_index, k;
    int write_cnt;
    unsigned int wdata[DEBUSSY_WRITE_PAGE_SIZE >> 2];
    int percent = -1;

    write_cnt = 0;
    total_page_num = (size / sizeof(char)) / DEBUSSY_WRITE_PAGE_SIZE;
    left_cnt = (size / sizeof(char)) % DEBUSSY_WRITE_PAGE_SIZE;

    for (page_index = 0; page_index < (total_page_num >> 2) << 2; page_index += 4) {
        for (k = 0; k < 4; k++) {
            for (word_index = 0; word_index < (DEBUSSY_WRITE_PAGE_SIZE >> 2); ++word_index) {
                idx = write_cnt + (k * DEBUSSY_WRITE_PAGE_SIZE) + (word_index << 2);
                wdata[word_index] = (data[idx + 3] << 24) |
                                    (data[idx + 2] << 16) |
                                    (data[idx + 1] << 8) |
                                    (data[idx]);
            }

            //printk("debussy to psram writepage\n");
            if (0 != _debussy_psram_writepage(dev, 0x2A0C0004 + (k * DEBUSSY_WRITE_PAGE_SIZE), wdata)) {
                dev_err(dev, "%s: _debussy_psram_writepage fail\n", __func__);
                return 1;
            }

            _debussy_psram_flush(dev, faddr + write_cnt + (k * DEBUSSY_WRITE_PAGE_SIZE),
                                    0x2A0C0004 + (k * DEBUSSY_WRITE_PAGE_SIZE));
        }

        write_cnt += DEBUSSY_WRITE_PAGE_SIZE << 2;

        if ((write_cnt * 10 / size) != percent) {
            percent = write_cnt * 10 / size;
            dev_info(dev, "%s: %d%%\n", __func__, percent * 10);
        }
    }

    if (total_page_num % 4) {
        for (page_index = 0; page_index < (total_page_num % 4); page_index++) {
            for (word_index = 0; word_index < (DEBUSSY_WRITE_PAGE_SIZE >> 2); ++word_index) {
                idx = write_cnt + (page_index * DEBUSSY_WRITE_PAGE_SIZE) + (word_index << 2);
                wdata[word_index] = (data[idx + 3] << 24) |
                                    (data[idx + 2] << 16) |
                                    (data[idx + 1] << 8) |
                                    data[idx];
            }

            if (0 != _debussy_psram_writepage(dev, 0x2A0C0004 + (page_index * DEBUSSY_WRITE_PAGE_SIZE), wdata)) {
                dev_err(dev, "%s: _debussy_psram_writepage fail\n", __func__);
                return 1;
            }

            _debussy_psram_flush(dev, faddr + write_cnt + (page_index * DEBUSSY_WRITE_PAGE_SIZE),
                                 0x2A0C0004 + (page_index * DEBUSSY_WRITE_PAGE_SIZE));
        }

        write_cnt += page_index * DEBUSSY_WRITE_PAGE_SIZE;
    }

    if ((write_cnt * 10 / size) != percent) {
        percent = write_cnt * 10 / size;
        dev_info(dev, "%s: %d%%\n", __func__, percent * 10);
    }

    if (left_cnt != 0) {
        memset_u32(wdata, 0xFFFFFFFF, sizeof(wdata) / sizeof(uint32_t));
        for (word_index = 0; word_index < (left_cnt >> 2); ++word_index) {
            idx = write_cnt + (word_index << 2);
            wdata[word_index] = (data[idx + 3] << 24) | (data[idx + 2] << 16) | (data[idx + 1] << 8) | (data[idx]);
        }

        if (0 != _debussy_psram_writepage(dev, 0x2A0C0004, wdata)) {
            dev_err(dev, "%s: _debussy_psram_writepage fail\n", __func__);
            return 1;
        }

        _debussy_psram_flush(dev, faddr + write_cnt, 0x2A0C0004);
        write_cnt += left_cnt;
    }

    dev_info(dev, "%s: 100%%\n", __func__);

    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void debussy_psram_read_id(struct device* dev) {
    struct i2c_client* client;
    unsigned int tmp_data[4];

    client = i2c_verify_client(dev);

    _debussy_psram_control(dev, 0x510A009F, 0, 0x0A0C0000, 4, 1);
    igo_i2c_read_buffer(client, 0x2A0C0000, (unsigned int *) &tmp_data, 4);
    dev_info(dev, "pSRAM ID : 0x%08x 0x%08X 0x%08X 0x%08X\n", tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
}

int debussy_psram_update_firmware(struct device* dev, unsigned int faddr, const u8* data, size_t size)
{
    int ret = 1;
    unsigned int addr = 0x00000;
    struct i2c_client *client = i2c_verify_client(dev);
    struct debussy_priv *debussy = i2c_get_clientdata(client);

    dev_info(dev, "%s: Download to address  0x%08x\n", __func__, addr);

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

    //debussy_psram_read_id(dev);

    /* Update firmware init sequence */
    igo_i2c_write(client, 0x2A00003C, 0);           // LV3
    igo_i2c_write(client, 0x2A000018, 1);           // HOLD
    igo_i2c_write(client, 0x2A000030, 1);           // LV1
    msleep(500);
    igo_i2c_write(client, 0x2A00003C, 0x830001);    // LV3
    igo_i2c_write(client, 0x2A013020, 0x04);        // AGN_BRUST
    igo_i2c_write(client, 0x2A013048, 0x00);        // AGN_PWD

    if (debussy->spi_dev && (0 == igo_spi_intf_enable(1))) {
        // igo_i2c_write(client, 0x2A01C028, 0x03);     // enable HFOSC
        // igo_i2c_write(client, 0x2A00005C, 0x03);     // set sysck div to (3+1)
        // igo_i2c_write(client, 0x2A000058, 0x01);     // set sysck to HFOSC(240/4 = 80mhz)
        ret = _psram_write_bin(debussy->spi_dev, addr, data, size);
    }
    else {
        ret = _psram_write_bin(dev, addr, data, size);
    }

    if (ret == 0) {
        // igo_i2c_write(client, 0x2A000018, 0x01);
        // Boot Message write message
        // boot via rom code flash_boot path, SINGLE
        // igo_i2c_write(client, 0x2A000010, 0x21000000);
        msleep(100);
        debussy->reset_chip(debussy->dev, 1);

        dev_info(dev, "Debussy FW update done\n");
    }
    else {
        dev_info(dev, "Debussy FW update fail\n");
    }

    return ret;
}

#define PSRAM_PAGE_SIZE	512  // 4

void debussy_psram_readpage(struct device* dev, unsigned int start_addr, unsigned int read_len)
{
	//unsigned int tmp_buf[PSRAM_PAGE_SIZE / 4] = {0};
	unsigned int *tmp_buf;
	unsigned int word_cnt = 0, dump_len = 0;
    struct i2c_client* client = i2c_verify_client(dev);
	struct debussy_priv *debussy = i2c_get_clientdata(client);
	int i = 0;
    
	dev_info(dev, "[PSRAM] addr=0x%08X, read_len=0x%x", start_addr, read_len);

    if ((tmp_buf = devm_kzalloc(debussy->dev, read_len + 1, GFP_KERNEL)) == NULL) {
        dev_err(debussy->dev, "%s: alloc fail\n", __func__);
        return;
    }
	
	//memset(tmp_buf, 0, PSRAM_PAGE_SIZE / 4);
	memset(tmp_buf, 0, read_len + 1);

	while (read_len != 0)
	{
		if (read_len > PSRAM_PAGE_SIZE) {
			dump_len = PSRAM_PAGE_SIZE;
		} else {
			dump_len = read_len;
		}
		
		word_cnt = (dump_len >> 2);

		if (debussy->spi_dev) {
	    	igo_spi_write(0x2A013048, 0x00);
		} else {
			igo_i2c_write(client, 0x2A013048, 0x00);
		}
		
	    //_debussy_psram_control(dev, 0x510A0003, start_addr, 0x0A0C0004, dump_len, 1);  // single read
	    _debussy_psram_control(dev, 0x7F8A00EB, start_addr, 0x0A0C0004, dump_len, 1);  // quad read, unit: bytes

		if (debussy->spi_dev) {
			igo_spi_read_buffer(0x2A0C0004, (unsigned int *)tmp_buf, word_cnt);
		} else {
	    	igo_i2c_read_buffer(client, 0x2A0C0004, (unsigned int *)tmp_buf, word_cnt);
		}

		dev_info(dev, "[PSRAM] addr=0x%08X, read_len=0x%x, dump_len=0x%x, word_cnt=0x%x", start_addr, read_len, dump_len, word_cnt);
		for (i = 0; i < (word_cnt); i++)
		{
			dev_info(dev, "[PSRAM 0x%08x] 0x%08X", start_addr, tmp_buf[i]);
			start_addr += 4;
		}		
		read_len -= dump_len;
	}

	devm_kfree(debussy->dev, tmp_buf);
}
#endif  /* end of DEBUSSY_TYPE_PSRAM */

