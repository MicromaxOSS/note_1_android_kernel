/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef BUILD_LK
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/vmalloc.h>
#include <linux/kobject.h>

/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/
#define DEVICE_NAME "it6112"
#define MIPITX_ADDR 0x56
#define MIPIRX_ADDR 0x5e
#define IT6112_DEBUG
#define REGFLAG_DELAY		(0xFE)
#define REGFLAG_END_OF_TABLE	(0xFF)

static struct i2c_client *it6112_mipirx;
static const struct i2c_device_id it6112_i2c_id[] = {
	{"it6112",	0},
};

static const struct of_device_id it6112_of_match[] = {
	{.compatible = "ite,it6112"},
	{},
};

static int it6112_i2c_driver_probe(struct i2c_client *client,
				   const struct i2c_device_id *id);

static struct i2c_driver it6112_i2c_driver = {
	  .driver = {
	      .name  = DEVICE_NAME,
	      .of_match_table = it6112_of_match,
	  },
	  .probe    = it6112_i2c_driver_probe,
	 .id_table  = it6112_i2c_id,
};

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static DEFINE_MUTEX(it6112_i2c_access);

static int it6112_read(u8 dev_addr, u8 reg, u8 *data, u16 data_len)
{
	int ret;
	struct i2c_msg msgs[] = {
		{
		 .addr = dev_addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg,
		},
		{
		 .addr = dev_addr,
		 .flags = I2C_M_RD,
		 .len = data_len,
		 .buf = data,
		}
	};

	ret = i2c_transfer(it6112_mipirx->adapter, msgs, 2);

	if (ret == 2)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

int it6112_i2c_read_byte(u8 dev_addr, u8 reg, u8 *returnData)
{
	int ret;

	it6112_read(dev_addr, reg, returnData, sizeof(u8));

	return ret;
}

static int it6112_write_bytes(u8 dev_addr, const u8 *data, u16 data_len)
{
	int ret;
	struct i2c_msg msg;

	msg.addr = dev_addr;
	msg.flags = 0;
	msg.len = data_len;
	msg.buf = (u8 *)data;

	ret = i2c_transfer(it6112_mipirx->adapter, &msg, 1);
	if (ret == 1)
		return 0;
	if (ret < 0)
		return ret;
	else
		return -EIO;
}

int it6112_i2c_write_byte(u8 dev_addr, u8 reg, u8 data)
{
	u8 buf[] = { reg, data };

	return it6112_write_bytes(dev_addr, buf, sizeof(buf));
}

/****************************************************************************
 *******************ITE IT6112 driver Start**********************************
 ****************************************************************************/

#define MIPITX_ADDR 0x56
#define MIPIRX_ADDR 0x5e

int it6112_mipi_tx_write(char offset, char data)
{
	return it6112_i2c_write_byte(MIPITX_ADDR, offset, data);
}

int it6112_mipi_tx_read(char offset)
{
	u8 data;

	it6112_i2c_read_byte(MIPITX_ADDR, offset, &data);

	return data;
}

int it6112_mipi_tx_set_bits(char offset, char mask, char data)
{
	char temp;

	temp = it6112_mipi_tx_read(offset);
	temp = (temp & ((~mask) & 0xFF)) + (mask & data);

	return it6112_mipi_tx_write(offset, temp);
}

int it6112_mipi_rx_write(char offset, char data)
{
	return it6112_i2c_write_byte(MIPIRX_ADDR, offset, data);
}

char it6112_mipi_rx_read(char offset)
{
	u8 data;

	it6112_i2c_read_byte(MIPIRX_ADDR, offset, &data);

	return data;
}

int it6112_mipi_rx_set_bits(char offset, char mask, char data)
{
	char temp;

	temp = it6112_mipi_rx_read(offset);

	temp = (temp & ((~mask) & 0xFF)) + (mask & data);
	return it6112_mipi_rx_write(offset, temp);
}

enum MIPI_VIDEO_TYPE {
	RGB_24b = 0x3E,
	RGB_30b = 0x0D,
	RGB_36b = 0x1D,
	RGB_18b = 0x1E,
	RGB_18b_L = 0x2E,
	YCbCr_16b = 0x2C,
	YCbCr_20b = 0x0C,
	YCbCr_24b = 0x1C,
};

#define EnMBPM 1
#define MPVidType RGB_24b
#define MPLaneSwap 0
#define MPPNSwap 0
#define SkipStg 4
#define HSSetNum 3
#define Reg4laneMode 0
#define InvRxMCLK 1
#define InvTxMCLK 0

struct it6112_device {
	char enable_mipi_rx_bypass_mode;
	char enable_mipi_rx_lane_swap;
	char enable_mipi_rx_pn_swap;
	char enable_mipi_4_lane_mode;
	char mipi_rx_video_type;
	char enable_mipi_rx_mclk_inverse;
	char enable_mipi_tx_mclk_inverse;
	char mipi_rx_skip_stg;
	char mipi_rx_hs_set_number;
	char revision;
	int rclk;
};

enum dcs_cmd_name {
	EXIT_SLEEP_MODE_SET_DISPLAY_ON,
	ENTER_SLEEP_MODE,
	SET_DISPLAY_OFF,
	EXIT_SLEEP_MODE,
	SET_DISPLAY_ON,
	LONG_WRITE_CMD,
	LONG_WRITE_CMD1,
};

struct dcs_setting_entry {
	enum dcs_cmd_name cmd_name;
	u8 cmd;
	u8 count;
	u8 para_list[30];
};

struct dcs_setting_entry_v2 {
	u8 cmd;
	u8 count;
	u8 para_list[30];
};

static struct dcs_setting_entry dcs_setting_table[] = {
	{EXIT_SLEEP_MODE_SET_DISPLAY_ON, 0x87, 8, {0x05, 0x11, 0x00, 0x36, 0x05,
						   0x29, 0x00, 0x1C} },
	{ENTER_SLEEP_MODE, 0x87, 4, {0x05, 0x10, 0x00, 0x2C} },
	{SET_DISPLAY_OFF, 0x87, 4, {0x05, 0x28, 0x00, 0x06} },
	{EXIT_SLEEP_MODE, 0x87, 4, {0x05, 0x11, 0x00, 0x36} },
	{SET_DISPLAY_ON, 0x87, 4, {0x05, 0x29, 0x00, 0x1C} },
	{LONG_WRITE_CMD, 0x87, 3, {0x50, 0x5A, 0x09} },
	{LONG_WRITE_CMD1, 0x87, 17, {0x80, 0x5A, 0x51, 0xB5, 0x2A, 0x6C, 0x35,
				     0x4B, 0x01, 0x40, 0xE1, 0x0D, 0x82, 0x20,
				     0x08, 0x30, 0x03} }
	/* to add other commands here. */
};

static struct dcs_setting_entry_v2 init_code[] = {
	{0x87, 4, {0x05, 0xB4, 0xF0, 0x1c} },
	{0x87, 4, {0x05, 0xB6, 0x03, 0x1c} },
	{0x87, 4, {0x05, 0xB7, 0x02, 0x1c} },
	{0x87, 4, {0x05, 0xBA, 0x43, 0x1c} },
};

void it6112_calc_rclk(struct it6112_device *it6112)
{
	int rddata, i, sum;

	sum = 0;
	for (i = 0; i < 5; i++) {
		it6112_mipi_rx_set_bits(0x94, 0x80, 0x80);
		msleep(100);
		it6112_mipi_rx_set_bits(0x94, 0x80, 0x00);

		rddata = it6112_mipi_rx_read(0x95);
		rddata += (it6112_mipi_rx_read(0x96) << 8);
		rddata += (it6112_mipi_rx_read(0x97) << 16);

		sum += rddata;
	}
	sum /= 5;

	it6112->rclk = sum / 100;

	pr_info("RCLK=%dMHz\n", it6112->rclk / 1000);
}

void it6112_calc_mclk(struct it6112_device *it6112)
{
	int i, rddata, sum, mclk;

	sum = 0;
	for (i = 0; i < 5; i++) {
		it6112_mipi_rx_set_bits(0x9B, 0x80, 0x80);
		usleep_range(1000, 2000);
		it6112_mipi_rx_set_bits(0x9B, 0x80, 0x00);

		rddata = it6112_mipi_rx_read(0x9A);
		rddata = ((it6112_mipi_rx_read(0x9B) & 0x0F) << 8) + rddata;

		sum += rddata;
	}

	sum /= 5;

	/* MCLK = 13500*2048/sum;
	 * MCLK = 27000*2048/sum;
	 */
	mclk = it6112->rclk * 2048 / sum;
	pr_info("MCLK = %dMHz\n", mclk / 1000);
}

void it6112_mipi_show_mrec(void)
{
	int MHSW, MHFP, MHBP, MHDEW, MHBlank;
	int MVSW, MVFP, MVBP, MVDEW, MVTotal, MHVR2nd, MVFP2nd;

	MHSW = it6112_mipi_rx_read(0x52);
	MHSW += (it6112_mipi_rx_read(0x53) & 0x3F) << 8;

	MHFP = it6112_mipi_rx_read(0x50);
	MHFP += (it6112_mipi_rx_read(0x51) & 0x3F) << 8;

	MHBP = it6112_mipi_rx_read(0x54);
	MHBP += (it6112_mipi_rx_read(0x55) & 0x3F) << 8;

	MHDEW = it6112_mipi_rx_read(0x56);
	MHDEW += (it6112_mipi_rx_read(0x57) & 0x3F) << 8;

	MHVR2nd = it6112_mipi_rx_read(0x58);
	MHVR2nd += (it6112_mipi_rx_read(0x59) & 0x3F) << 8;

	MHBlank = MHFP + MHSW + MHBP;

	MVSW = it6112_mipi_rx_read(0x5A);
	MVSW += (it6112_mipi_rx_read(0x5B) & 0x1F) << 8;

	MVFP = it6112_mipi_rx_read(0x58);
	MVFP += (it6112_mipi_rx_read(0x59) & 0x1F) << 8;

	MVBP = it6112_mipi_rx_read(0x5C);
	MVBP += (it6112_mipi_rx_read(0x5D) & 0x1F) << 8;

	MVDEW = it6112_mipi_rx_read(0x5E);
	MVDEW += (it6112_mipi_rx_read(0x5F) & 0x1F) << 8;

	MVFP2nd = it6112_mipi_rx_read(0x62);
	MVFP2nd += (it6112_mipi_rx_read(0x63) & 0x3F) << 8;

	MVTotal = MVFP + MVSW + MVBP + MVDEW;
}

void it6112_init_config(struct it6112_device *it6112)
{
	it6112->enable_mipi_rx_bypass_mode = EnMBPM;
	it6112->enable_mipi_rx_lane_swap = MPLaneSwap;
	it6112->enable_mipi_rx_pn_swap = MPPNSwap;
	it6112->enable_mipi_4_lane_mode = Reg4laneMode;
	it6112->mipi_rx_video_type = MPVidType;
	it6112->enable_mipi_rx_mclk_inverse = InvRxMCLK;
	it6112->enable_mipi_tx_mclk_inverse = InvTxMCLK;
	it6112->mipi_rx_skip_stg = SkipStg;
	it6112->mipi_rx_hs_set_number = HSSetNum;
	pr_info("%s done\n", __func__);
}

void it6112_mipi_rx_init(struct it6112_device *it6112)
{
	/* Enable MPRX interrupt */
	it6112_mipi_rx_write(0x09, 0xFF);
	it6112_mipi_rx_write(0x0A, 0xFF);
	it6112_mipi_rx_write(0x0B, 0x3F);
	it6112_mipi_rx_write(0x05, 0x03);
	it6112_mipi_rx_write(0x05, 0x00);

	/* Setup INT Pin: Active Low */
	it6112_mipi_rx_set_bits(0x0C, 0x0F,
				(it6112->enable_mipi_rx_lane_swap << 3) |
				(it6112->enable_mipi_rx_pn_swap << 2) | 3);
	it6112_mipi_rx_set_bits(0x11, 0x01,
				it6112->enable_mipi_rx_mclk_inverse);
	it6112_mipi_rx_set_bits(0x18, 0xff, 0x43);
	it6112_mipi_rx_set_bits(0x19, 0xf3, 0x03);
	it6112_mipi_rx_set_bits(0x20, 0xf7, 0x03);
	it6112_mipi_rx_set_bits(0x21, 0x08, 0x08);
	it6112_mipi_rx_set_bits(0x44, 0x22, 0x22);
	it6112_mipi_rx_write(0x27, it6112->mipi_rx_video_type);
	it6112_mipi_rx_write(0x72, 0x07);
	it6112_mipi_rx_set_bits(0x8A, 0x07, 0x02);
	it6112_mipi_rx_set_bits(0xA0, 0x01, 0x01);
	if (it6112->enable_mipi_4_lane_mode)
		it6112_mipi_rx_set_bits(0x80, 0x3F, 0x03);
	else
		it6112_mipi_rx_set_bits(0x80, 0x3F, 0x07);

	pr_info("\n\rit6112 MPRX initial done!");
}

void it6112_mipi_tx_init(struct it6112_device *it6112)
{
	/* software reset all, include RX!! */
	it6112_mipi_tx_set_bits(0x05, 0x01, 0x01);

	/* hold tx first */
	it6112_mipi_tx_set_bits(0x05, 0x20, 0x20);
	usleep_range(1000, 3000);

	it6112_mipi_tx_set_bits(0x10, 0x02,
				(it6112->enable_mipi_tx_mclk_inverse << 1));
	it6112_mipi_tx_set_bits(0x11, 0x08,
				(it6112->enable_mipi_4_lane_mode << 3));
	it6112_mipi_tx_set_bits(0x24, 0x0f, 0x01);
	it6112_mipi_tx_set_bits(0x3C, 0x20, 0x20);
	it6112_mipi_tx_set_bits(0x44, 0x04, 0x04);
	/* adjust tx low power state's command time interval */
	it6112_mipi_tx_set_bits(0x45, 0x0f, 0x03);
	it6112_mipi_tx_set_bits(0x47, 0xf0, 0x10);
	it6112_mipi_tx_set_bits(0xB0, 0xFF, 0x27);
	usleep_range(1000, 3000);

	pr_info("\n\rit6112 MPTX initial done!");
}

int get_dcs_ecc(int dcshead)
{
	int Q0, Q1, Q2, Q3, Q4, Q5;

	Q0 = ((dcshead >> 0) & 1) ^ ((dcshead >> 1) & 1) ^ ((dcshead >> 2) & 1)
	      ^ ((dcshead >> 4) & 1) ^ ((dcshead >> 5) & 1)
	      ^ ((dcshead >> 7) & 1) ^ ((dcshead >> 10) & 1)
	      ^ ((dcshead >> 11) & 1) ^ ((dcshead >> 13) & 1)
	      ^ ((dcshead >> 16) & 1) ^	((dcshead >> 20) & 1)
	      ^ ((dcshead >> 21) & 1) ^ ((dcshead >> 22) & 1)
	      ^ ((dcshead >> 23) & 1);

	Q1 = ((dcshead >> 0) & 1) ^ ((dcshead >> 1) & 1) ^ ((dcshead >> 3) & 1)
	      ^ ((dcshead >> 4) & 1) ^ ((dcshead >> 6) & 1)
	      ^ ((dcshead >> 8) & 1) ^ ((dcshead >> 10) & 1)
	      ^ ((dcshead >> 12) & 1) ^ ((dcshead >> 14) & 1)
	      ^ ((dcshead >> 17) & 1) ^	((dcshead >> 20) & 1)
	      ^ ((dcshead >> 21) & 1) ^ ((dcshead >> 22) & 1)
	      ^ ((dcshead >> 23) & 1);

	Q2 = ((dcshead >> 0) & 1) ^ ((dcshead >> 2) & 1) ^ ((dcshead >> 3) & 1)
	      ^ ((dcshead >> 5) & 1) ^ ((dcshead >> 6) & 1)
	      ^ ((dcshead >> 9) & 1) ^ ((dcshead >> 11) & 1)
	      ^ ((dcshead >> 12) & 1) ^ ((dcshead >> 15) & 1)
	      ^ ((dcshead >> 18) & 1) ^	((dcshead >> 20) & 1)
	      ^ ((dcshead >> 21) & 1) ^ ((dcshead >> 22) & 1);

	Q3 = ((dcshead >> 1) & 1) ^ ((dcshead >> 2) & 1) ^ ((dcshead >> 3) & 1)
	      ^ ((dcshead >> 7) & 1) ^ ((dcshead >> 8) & 1)
	      ^ ((dcshead >> 9) & 1) ^ ((dcshead >> 13) & 1)
	      ^ ((dcshead >> 14) & 1) ^ ((dcshead >> 15) & 1)
	      ^ ((dcshead >> 19) & 1) ^	((dcshead >> 20) & 1)
	      ^ ((dcshead >> 21) & 1) ^ ((dcshead >> 23) & 1);

	Q4 = ((dcshead >> 4) & 1) ^ ((dcshead >> 5) & 1) ^ ((dcshead >> 6) & 1)
	      ^ ((dcshead >> 7) & 1) ^ ((dcshead >> 8) & 1)
	      ^ ((dcshead >> 9) & 1) ^ ((dcshead >> 16) & 1)
	      ^ ((dcshead >> 17) & 1) ^ ((dcshead >> 18) & 1)
	      ^ ((dcshead >> 19) & 1) ^	((dcshead >> 20) & 1)
	      ^ ((dcshead >> 22) & 1) ^ ((dcshead >> 23) & 1);

	Q5 = ((dcshead >> 10) & 1) ^ ((dcshead >> 11) & 1)
	      ^ ((dcshead >> 12) & 1) ^ ((dcshead >> 13) & 1)
	      ^ ((dcshead >> 14) & 1) ^	((dcshead >> 15) & 1)
	      ^ ((dcshead >> 16) & 1) ^ ((dcshead >> 17) & 1)
	      ^ ((dcshead >> 18) & 1) ^ ((dcshead >> 19) & 1)
	      ^	((dcshead >> 21) & 1) ^ ((dcshead >> 22) & 1)
	      ^ ((dcshead >> 23) & 1);

	return (Q0 + (Q1 << 1) + (Q2 << 2) + (Q3 << 3) + (Q4 << 4) + (Q5 << 5));
}

void it6112_mipi_write_short_dcs_para(enum dcs_cmd_name cmd_name)
{
	int short_cmd = 0, i;

	short_cmd = dcs_setting_table[cmd_name].para_list[0] |
		    dcs_setting_table[cmd_name].para_list[1] << 8 |
		    dcs_setting_table[cmd_name].para_list[2] << 16;
	dcs_setting_table[cmd_name].para_list[3] = get_dcs_ecc(short_cmd);

	for (i = 0; i < dcs_setting_table[cmd_name].count; i++)
		it6112_mipi_tx_write(0x73,
				     dcs_setting_table[cmd_name].para_list[i]);

	it6112_mipi_tx_write(0x74, 0x40 | dcs_setting_table[cmd_name].count);
	it6112_mipi_tx_write(0x75, dcs_setting_table[cmd_name].cmd);
}

void it6112_mipi_write_dcs_cmds(enum dcs_cmd_name start, int count)
{
	u8 i, enable_force_lp_mode = !!(it6112_mipi_tx_read(0x05) & 0x20);

	if (enable_force_lp_mode)
		it6112_mipi_tx_set_bits(0x70, 0x04, 0x04);

	it6112_mipi_tx_write(0x3D, 0x00);
	it6112_mipi_tx_write(0x3E, enable_force_lp_mode ? 0x00 : 0x10);
	it6112_mipi_tx_write(0x3F, enable_force_lp_mode ? 0x30 : 0x90);

	for (i = start; i < start + count; i++)
		it6112_mipi_write_short_dcs_para(i);

	usleep_range(10000, 20000);

	if (enable_force_lp_mode)
		it6112_mipi_tx_set_bits(0x70, 0x04, 0x00);

	msleep(20);
}

void it6112_mipi_write_dcs_cmds_v2(struct dcs_setting_entry_v2 *init_cmd,
				   unsigned int cmd_index)
{
	int short_cmd = 0;
	u8 i, enable_force_lp_mode = !!(it6112_mipi_tx_read(0x05) & 0x20);

	if (enable_force_lp_mode)
		it6112_mipi_tx_set_bits(0x70, 0x04, 0x04);

	it6112_mipi_tx_write(0x3D, 0x00);
	it6112_mipi_tx_write(0x3E, enable_force_lp_mode ? 0x00 : 0x10);
	it6112_mipi_tx_write(0x3F, enable_force_lp_mode ? 0x30 : 0x90);

	short_cmd = init_cmd[cmd_index].para_list[0] |
		    init_cmd[cmd_index].para_list[1] << 8 |
		    init_cmd[cmd_index].para_list[2] << 16;
	init_cmd[cmd_index].para_list[3] = get_dcs_ecc(short_cmd);

	for (i = 0; i < init_cmd[cmd_index].count; i++)
		it6112_mipi_tx_write(0x73, init_cmd[cmd_index].para_list[i]);

	it6112_mipi_tx_write(0x74, 0x40 | init_cmd[cmd_index].count);
	it6112_mipi_tx_write(0x75, init_cmd[cmd_index].cmd);
}

void it6112_mipi_write_dcs_cmds_v3(void)
{
	u8 enable_force_lp_mode = !!(it6112_mipi_tx_read(0x05) & 0x20);

	usleep_range(10000, 20000);

	if (enable_force_lp_mode)
		it6112_mipi_tx_set_bits(0x70, 0x04, 0x00);

	msleep(20);
}

static void it6112_push_table(struct dcs_setting_entry_v2 *init_code,
			      unsigned int count)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = init_code[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(init_code[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			it6112_mipi_write_dcs_cmds_v2(init_code, i);
			break;
		}
	}
}

void it6112_mipi_set_output(struct it6112_device *it6112)
{
	it6112_calc_rclk(it6112);
	it6112_calc_mclk(it6112);
	it6112_mipi_tx_set_bits(0x11, 0x80, 0x80);
	usleep_range(1000, 2000);

	it6112_push_table(init_code,
		sizeof(init_code) / sizeof(struct dcs_setting_entry_v2));
	usleep_range(10000, 20000);
	it6112_mipi_write_dcs_cmds_v3();

	it6112_mipi_write_dcs_cmds(EXIT_SLEEP_MODE, 1);
	msleep(100);
	it6112_mipi_write_dcs_cmds(SET_DISPLAY_ON, 1);
	usleep_range(1000, 2000);
	it6112_mipi_tx_set_bits(0x11, 0x80, 0x00);
	usleep_range(1000, 2000);
	it6112_mipi_tx_write(0x05, 0xfe);
	it6112_mipi_tx_write(0x05, 0x00);
}

unsigned int it6112_revision_identify(struct it6112_device *it6112)
{
	if (it6112_mipi_tx_read(0x00) != 0x54 ||
	    it6112_mipi_tx_read(0x01) != 0x49 ||
	    it6112_mipi_tx_read(0x02) != 0x12 ||
	    it6112_mipi_tx_read(0x03) != 0x61) {
		pr_info("\n\rError: Can not find IT6112B0 Device ");
		return 0;
	}
	it6112->revision = it6112_mipi_tx_read(0x04);

	return 1;
}

void it6112_mipi_power_on(void)
{
	it6112_mipi_tx_set_bits(0x06, 0x02, 0x00);
	usleep_range(1000, 2000);
	it6112_mipi_rx_set_bits(0x05, 0x0F, 0x00);
	it6112_mipi_tx_set_bits(0x05, 0xCC, 0x00);
	usleep_range(1000, 2000);
	pr_info("\n\rit6112 mipi power on\n");
}

void it6112_mipi_power_off(void)
{
	it6112_mipi_write_dcs_cmds(SET_DISPLAY_OFF, 1);
	msleep(100);
	it6112_mipi_write_dcs_cmds(ENTER_SLEEP_MODE, 1);
	it6112_mipi_tx_set_bits(0x05, 0xFE, 0xFE);
	it6112_mipi_rx_set_bits(0x05, 0x03, 0x03);
	it6112_mipi_tx_set_bits(0x06, 0x02, 0x02);
	pr_info("\n\rit6112 mipi power off\n");
}

static struct it6112_device it6112_dev;
void it6112_init(void)
{
	struct it6112_device *it6112;

	it6112 = &it6112_dev;
	it6112->enable_mipi_rx_bypass_mode = EnMBPM;
	it6112->enable_mipi_rx_lane_swap = MPLaneSwap;
	it6112->enable_mipi_rx_pn_swap = MPPNSwap;
	it6112->enable_mipi_4_lane_mode = Reg4laneMode;
	it6112->mipi_rx_video_type = MPVidType;
	it6112->enable_mipi_rx_mclk_inverse = InvRxMCLK;
	it6112->enable_mipi_tx_mclk_inverse = InvTxMCLK;
	it6112->mipi_rx_skip_stg = SkipStg;
	it6112->mipi_rx_hs_set_number = HSSetNum;

	if (!it6112_revision_identify(it6112))
		return;

	it6112_init_config(it6112);
	it6112_mipi_power_on();
	it6112_mipi_rx_init(it6112);
	it6112_mipi_tx_init(it6112);
	it6112_mipi_set_output(it6112);
}

/*****************************************************************************
 *******************ITE IT6112 DRIVER END*************************************
 *****************************************************************************/

static int match_id(const struct i2c_client *client,
		    const struct i2c_device_id *id)
{
	if (strcmp(client->name, id->name) == 0)
		return true;
	else
		return false;
}

static int it6112_i2c_driver_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	int err = 0;

	pr_info("[%s] start!\n", __func__);

	if (match_id(client, &it6112_i2c_id[0])) {
		it6112_mipirx = kmalloc(sizeof(*it6112_mipirx), GFP_KERNEL);
		if (!it6112_mipirx) {
			err = -ENOMEM;
			goto exit;
		}

		memset(it6112_mipirx, 0, sizeof(struct i2c_client));

		it6112_mipirx = client;
	} else {
		pr_err("[%s] error!\n", __func__);

		err = -EIO;
		goto exit;
	}

	pr_info("[%s] %s i2c success!\n", __func__, client->name);

	return 0;

exit:
	return err;
}

static int __init it6112_init_dev(void)
{
	pr_info("[%s] init start\n", __func__);

	if (i2c_add_driver(&it6112_i2c_driver) != 0)
		pr_err("[%s] Failed to register i2c driver.\n", __func__);
	else
		pr_info("[%s] Success to register i2c driver.\n", __func__);

	return 0;
}

static void __exit it6112_exit(void)
{
	i2c_del_driver(&it6112_i2c_driver);
}

module_init(it6112_init_dev);
module_exit(it6112_exit);

MODULE_DESCRIPTION("I2C IT6112 Driver");
MODULE_AUTHOR("Jitao shi<jitao.shi@mediatek.com>");
#endif

