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

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#else
#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif
#endif

#include <mt-plat/upmu_common.h>

#include "lcm_drv.h"
#include "it6112.h"
/*****************************************************************************
 *******************PANEL DRIVER START****************************************
 *****************************************************************************/

#ifndef BUILD_LK

/* ------------------------------------------------------------------------- */
/* Local Variables */
/* ------------------------------------------------------------------------- */

static struct LCM_UTIL_FUNCS lcm_util = { 0 };

#define SET_RESET_PIN(v)	(lcm_util.set_reset_pin((v)))
#define UDELAY(n) (lcm_util.udelay(n))
#define MDELAY(n) (lcm_util.mdelay(n))

static void lcm_set_gpio_output(unsigned int GPIO, unsigned int output)
{
	gpio_direction_output(GPIO, output);
	gpio_set_value(GPIO, output);
}

static unsigned int GPIO_LCD_RST_EN; /* GPIO45 */
static unsigned int GPIO_LCD_PWR_EN; /* GPIO158 */

void lcm_request_gpio_control(struct device *dev)
{
	GPIO_LCD_RST_EN = of_get_named_gpio(dev->of_node, "lcd_rst_pin", 0);
	gpio_request(GPIO_LCD_RST_EN, "GPIO_LCD_RST_EN");

	GPIO_LCD_PWR_EN = of_get_named_gpio(dev->of_node, "gpio_lcd_pwr_en",
					    0);
	gpio_request(GPIO_LCD_PWR_EN, "GPIO_LCD_PWR_EN");
}

static int lcm_driver_probe(struct device *dev, void const *data)
{
	pr_info("it6112_sample_dsi_vdo %s done\n", __func__);

	lcm_request_gpio_control(dev);

	return 0;
}

static const struct of_device_id lcm_platform_of_match[] = {
	{ .compatible = "mediatek,it6112-sample-lcm", },
	{},
};

static int lcm_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *id;

	id = of_match_node(lcm_platform_of_match, pdev->dev.of_node);
	if (!id) {
		pr_err("it6112 sample %s fail\n", __func__);
		return -ENODEV;
	}
	return lcm_driver_probe(&pdev->dev, id->data);
}

static struct platform_driver lcm_driver = {
	.probe = lcm_platform_probe,
	.driver = {
		   .name = "it6112 sample_dsi",
		   .owner = THIS_MODULE,
		   .of_match_table = lcm_platform_of_match,
		   },
};

static int __init lcm_drv_init(void)
{
	if (platform_driver_register(&lcm_driver)) {
		pr_err("LCM: failed to register this driver!\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit lcm_exit(void)
{
	platform_driver_unregister(&lcm_driver);
}

late_initcall(lcm_drv_init);
module_exit(lcm_exit);
MODULE_AUTHOR("mediatek");
MODULE_DESCRIPTION("LCM display subsystem driver");
MODULE_LICENSE("GPL");
#endif

#define FRAME_WIDTH  (1600)
#define FRAME_HEIGHT (2176)

#define   LCM_DSI_CMD_MODE	0

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type = LCM_TYPE_DSI;

	params->width = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->physical_width = 135;
	params->physical_height = 217;

	params->dsi.mode = SYNC_PULSE_VDO_MODE;

	/* DSI */
	/* Command mode setting */
	/* Three lane or Four lane */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;

	/* The following defined the format for data coming from LCD engine. */
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

	/* Highly depends on LCD driver capability. */
	params->dsi.packet_size = 256;

	params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.word_count = FRAME_WIDTH * 3;

	params->dsi.vertical_sync_active = 8;
	params->dsi.vertical_backporch = 73;
	params->dsi.vertical_frontporch = 250;
	params->dsi.vertical_active_line = FRAME_HEIGHT;

	params->dsi.horizontal_sync_active = 14;
	params->dsi.horizontal_backporch = 25;
	params->dsi.horizontal_frontporch = 25;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	params->dsi.PLL_CLOCK = 780;
	params->dsi.cont_clock = 1;
	params->dsi.ssc_disable = 1;
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("%s, lk\n", __func__);
#else
	pr_info("%s, kernel\n", __func__);
#endif

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
	MDELAY(30);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 1);
	MDELAY(5);

	lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	MDELAY(10);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, 1);
	MDELAY(120);

	it6112_init();
}

static void lcm_suspend(void)
{
#ifndef BUILD_LK
	it6112_mipi_power_off();

	MDELAY(20);
	lcm_set_gpio_output(GPIO_LCD_RST_EN, 0);
	MDELAY(10);

	lcm_set_gpio_output(GPIO_LCD_PWR_EN, 0);
	MDELAY(20);
#endif
}

static void lcm_resume(void)
{
#ifndef BUILD_LK
	lcm_init();
#endif
}

struct LCM_DRIVER it6112_sample_dsi_vdo_lcm_drv = {
	.name = "it6112_sample_dsi_vdo_lcm_drv",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
};

/*****************************************************************************
 *******************PANEL DRIVER END******************************************
 *****************************************************************************/

