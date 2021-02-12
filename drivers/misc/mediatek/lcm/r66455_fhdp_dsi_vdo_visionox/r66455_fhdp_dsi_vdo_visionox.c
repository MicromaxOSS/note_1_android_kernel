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

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"


#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
/*#include <mach/mt_pm_ldo.h>*/
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
#include <cust_i2c.h>
#endif
#endif
//#include <mach/gpio_const.h>
//#include "tps65132.h"

#ifdef BUILD_LK
#define LCM_LOGI(fmt, args...)  printk(KERN_INFO  " LCM file=%s: %s: line=%d: "fmt"\n", __FILE__,__func__,  __LINE__,##args)
#define LCM_LOGD(fmt, args...)  printk(KERN_DEBUG " LCM file=%s: %s: line=%d: "fmt"\n", __FILE__,__func__,  __LINE__,##args)
#define LCM_ENTER() printk(KERN_DEBUG " LCM file=%s: %s: line=%d: Enter------->\n", __FILE__,__func__, __LINE__)
#define LCM_EXIT()  printk(KERN_DEBUG " LCM file=%s: %s: line=%d: Exit<-------\n",  __FILE__,__func__, __LINE__)

#else
#define LCM_LOGI(fmt, args...)  printk(KERN_INFO " LCM :"fmt"\n", ##args)
#define LCM_LOGD(fmt, args...)  printk(KERN_DEBUG " LCM :"fmt"\n", ##args)
#define LCM_ENTER() 
#define LCM_EXIT()  

#endif


#define I2C_I2C_LCD_BIAS_CHANNEL 0
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)			(lcm_util.set_reset_pin((v)))
#define MDELAY(n)					(lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */
#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
	lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd) \
	lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
	lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
	lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

static const unsigned char LCD_MODULE_ID = 0x01;
/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE	1 //prize-wyq 20190306 modify to cmd mode to fix brightness and hbm mode setting failed in video mode
#define FRAME_WIDTH  										1080
#define FRAME_HEIGHT 										2340
#define LCM_PHYSICAL_WIDTH                  				(68040)
#define LCM_PHYSICAL_HEIGHT                  				(147420)


#define REGFLAG_DELAY             							 0xFFFA
#define REGFLAG_UDELAY             							 0xFFFB
#define REGFLAG_PORT_SWAP									 0xFFFC
#define REGFLAG_END_OF_TABLE      							 0xFFFD   // END OF REGISTERS MARKER

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[80];
};

/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 begin */
static unsigned int g_level = 0;
/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 end */
static struct LCM_setting_table lcm_initialization_setting[] = {
#if (!LCM_DSI_CMD_MODE)
	{0xB0,1,{0x00}},
	{0xB3,1,{0x01}},
	{0xB0,1,{0x80}},
#endif		
	{0xE6,1,{0x01}},
	//{0x51,2,{0x0f,0xff}},
	{0x51,2,{0x00,0x00}},
	{0x35,1,{0x00}},

	//prize-mod wyq 20190611 color tuning-start
	{0xB0,1,{0x04}},
	{0xCD,8,{0x1D,0x00,0xEA,0xED,0xFC,0xEA,0x29,0x2B}},
	{0xB0,1,{0x80}},
	{0xE6,1,{0x00}},
	//prize-mod wyq 20190611 color tuning-end
	{0x11,1,{0x00}},
	{REGFLAG_DELAY,150,{}},
	{0x29,1,{0x00}},

	//{REGFLAG_DELAY, 60, {}},//prize-mod wyq 20181222 fix abnormal display issue
	{REGFLAG_END_OF_TABLE, 0x00, {}}    
              
};

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 1,{0x00} },
	//{REGFLAG_DELAY, 60, {} },
	{0x10, 1,{0x00} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}  
};

static void push_table(void *cmdq, struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;
    LCM_ENTER();
	for (i = 0; i < count; i++) {
		unsigned cmd;

		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count, table[i].para_list, force_update);
		}
	}
	LCM_EXIT();
}




/* --------------------------------------------------------------------------- */
/* LCM Driver Implementations */
/* --------------------------------------------------------------------------- */

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

		// enable tearing-free
	params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
	params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
    #if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
    params->dsi.switch_mode = SYNC_PULSE_VDO_MODE;
	lcm_dsi_mode = CMD_MODE;
    #else
	params->dsi.mode   = SYNC_PULSE_VDO_MODE;//SYNC_EVENT_VDO_MODE;//BURST_VDO_MODE;////
	lcm_dsi_mode = SYNC_PULSE_VDO_MODE;
    #endif

	//1 Three lane or Four lane
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	
	
	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;
	
//#if (LCM_DSI_CMD_MODE)
//	params->dsi.intermediat_buffer_num = 0;//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
//	params->dsi.word_count=FRAME_WIDTH*3;	//DSI CMD mode need set these two bellow params, different to 6577
//#else
//	params->dsi.intermediat_buffer_num = 0;	//because DSI/DPI HW design change, this parameters should be 0 when video mode in MT658X; or memory leakage
//#endif

	// Video mode setting
	params->dsi.packet_size = 256;

	params->dsi.vertical_sync_active				=  2;//prize-wyq 20190306 fix display issue when fingerprint unlock in video mode
	params->dsi.vertical_backporch					= 4;//16 25 30 35 12 8
	params->dsi.vertical_frontporch					= 25;
	params->dsi.vertical_active_line				= FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active = 10;
	params->dsi.horizontal_backporch = 60;//32
	params->dsi.horizontal_frontporch = 105;//78
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

#ifndef CONFIG_FPGA_EARLY_PORTING
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 550;	/* this value must be in MTK suggested table *///prize-wyq 20190325 modify to 60fps in cmd mode,fix bug 71529/72287/72725
#else
	params->dsi.PLL_CLOCK = 630;	/* this value must be in MTK suggested table *///prize-wyq 20190306 modify to 60fps in video mode
#endif
	params->dsi.PLL_CK_CMD = 550;
	params->dsi.PLL_CK_VDO = 480;
#else
	params->dsi.pll_div1 = 0;
	params->dsi.pll_div2 = 0;
	params->dsi.fbk_div = 0x1;
#endif

	params->dsi.ssc_disable = 0;
	params->dsi.ssc_range = 1;
	params->dsi.cont_clock = 0;
	params->dsi.noncont_clock = 1;
	params->dsi.noncont_clock_period = 2; // Unit : frames
	params->dsi.clk_lp_per_line_enable = 1;
	params->physical_width = LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;
	#if 1
	params->dsi.lcm_ext_te_monitor = TRUE;
	
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd  				= 0x0a;
	params->dsi.lcm_esd_check_table[0].count  			= 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;

	params->dsi.lcm_esd_check_table[1].cmd  				= 0xac;
	params->dsi.lcm_esd_check_table[1].count  			= 1;
	params->dsi.lcm_esd_check_table[1].para_list[0] = 0x00;
		
	params->dsi.lcm_esd_check_table[2].cmd  				= 0x0d;
	params->dsi.lcm_esd_check_table[2].count  			= 1;
	params->dsi.lcm_esd_check_table[2].para_list[0] = 0x00;

	#endif

}


static unsigned int lcm_compare_id(void)
{
	
	unsigned char buffer[2];
	unsigned int array[16];  
	
	SET_RESET_PIN(1);
	
	MDELAY(20);//100

	SET_RESET_PIN(0);
	MDELAY(20);//100
	//tps65132_avdd_en(TRUE);

	MDELAY(10);  

	SET_RESET_PIN(1);
	MDELAY(250);//250
	
   

	array[0]=0x00DE0500;
	dsi_set_cmdq(array, 1, 1);

	array[0]=0x32B41500; 
	dsi_set_cmdq(array, 1, 1);

	array[0]=0x00DF0500;
	dsi_set_cmdq(array, 1, 1);

	array[0] = 0x00013700;// read id return two byte,version and id
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x0C, buffer, 1);
	
    #ifdef BUILD_LK
	LCM_LOGI("%s, LK TDDI id = 0x%08x\n", __func__, buffer[0]);
   #else
	LCM_LOGI("%s, Kernel TDDI id = 0x%08x\n", __func__, buffer[0]);
   #endif
   return 1;
   return (0x77 == buffer[0])?1:0; 
	//return 1;//(LCM_ID_NT35532 == id)?1:0;


}
#ifndef BUILD_LK
extern atomic_t ESDCheck_byCPU;
#endif
static unsigned int lcm_ata_check(unsigned char *bufferr)
{
        unsigned char buffer1[2]={0};
        unsigned char buffer2[2]={0};
        unsigned int data_array[6];

	/*
	55h write_content_adaptive_brightness_control
	56h read_content_adaptive_brightness_control
	*/
        data_array[0]= 0x00023902;//LS packet
        data_array[1]= 0x00005055;
        dsi_set_cmdq(data_array, 2, 1);

        data_array[0] = 0x00013700;// read id return two byte,version and id
        dsi_set_cmdq(data_array, 1, 1);
        atomic_set(&ESDCheck_byCPU, 1);
        read_reg_v2(0x56, buffer1, 1);
        atomic_set(&ESDCheck_byCPU, 0);

        data_array[0]= 0x0002390a;//HS packet
        data_array[1]= 0x00003155;
        dsi_set_cmdq(data_array, 2, 1);

        data_array[0] = 0x00013700;// read id return two byte,version and id
        dsi_set_cmdq(data_array, 1, 1);
        atomic_set(&ESDCheck_byCPU, 1);
        read_reg_v2(0x56, buffer2, 1);
        atomic_set(&ESDCheck_byCPU, 0);

        LCM_LOGI("%s, Kernel TDDI id buffer1= 0x%04x buffer2= 0x%04x\n", __func__, buffer1[0],buffer2[0]);
        return ((0x50 == buffer1[0])&&(0x31 == buffer2[0]))?1:0;
}

static void lcm_init(void)
{
#if 1
	mt_dsi_pinctrl_set(LCM_RESET_PIN_NO, 1);
	MDELAY(10);
	mt_dsi_pinctrl_set(LCM_RESET_PIN_NO, 0);
	MDELAY(10);
	mt_dsi_pinctrl_set(LCM_POWER_DM_NO, 1);
	mt_dsi_pinctrl_set(LCM_RESET_PIN_NO, 1);
	MDELAY(12);
#endif	
	push_table(NULL, lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
	LCM_EXIT();
}

//prize-wyq 20190521 fix Bug73848-CMDQ occur "SW timeout" error and phone takes 2s to wakeup when press powerkey to sleep & wakeup phone quickly-start
int is_suspend = 0;
int gbm_mode = 0;
static void lcm_suspend(void)
{
 
	push_table(NULL, lcm_suspend_setting,
		   sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
	MDELAY(10);

	mt_dsi_pinctrl_set(LCM_POWER_DM_NO, 0);
	MDELAY(10);

	mt_dsi_pinctrl_set(LCM_RESET_PIN_NO, 0);
	MDELAY(10);

	is_suspend = 1;
	gbm_mode = 0;//lcd will exit hbm mode automatically when suspend it
	
}

static void lcm_resume(void)
{
	lcm_init();
	is_suspend = 0;
	
}
//prize-wyq 20190521 fix Bug73848-CMDQ occur "SW timeout" error and phone takes 2s to wakeup when press powerkey to sleep & wakeup phone quickly-end

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y,
		       unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0>>8)&0xFF);
	unsigned char x0_LSB = (x0&0xFF);
	unsigned char x1_MSB = ((x1>>8)&0xFF);
	unsigned char x1_LSB = (x1&0xFF);
	unsigned char y0_MSB = ((y0>>8)&0xFF);
	unsigned char y0_LSB = (y0&0xFF);
	unsigned char y1_MSB = ((y1>>8)&0xFF);
	unsigned char y1_LSB = (y1&0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB<<24)|(y0_LSB<<16)|(y0_MSB<<8)|0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);
	/* BEGIN PN:DTS2013013101431 modified by s00179437 , 2013-01-31 */
	/* delete high speed packet */
	/* data_array[0] =0x00290508; */
	/* dsi_set_cmdq(data_array, 1, 1); */
	/* END PN:DTS2013013101431 modified by s00179437 , 2013-01-31 */

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
#endif

static void lcm_init_power(void)
{
	//display_bias_enable();
}

static void lcm_suspend_power(void)
{
	//display_bias_disable();
}

static void lcm_resume_power(void)
{
	SET_RESET_PIN(0);
	//display_bias_enable();
}

//prize-add wyq 20181226 add  lcd-backlight interface-start
static struct LCM_setting_table bl_level[] = {
	{0x51, 2, {0x0F,0xFF} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table bl_hbm_enter[] = {
	//{0xFE, 1, {0x40} }, 
	//{0x64, 1, {0x02} },		
	{0x53, 1, {0xE0} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table bl_hbm_exit[] = {
	{0x53, 1, {0x20} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{

	LCM_LOGI("%s,nt35695 backlight: level = %d\n", __func__, level);
/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 begin */
	g_level = level;
/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 end */
	level = level*16;

	bl_level[0].para_list[0] = (level>>8)&0xff;
	bl_level[0].para_list[1] = (level)&0xff;

	push_table(handle, bl_level, sizeof(bl_level) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_set_backlight_mode(void *handle, unsigned int mode)
{
	LCM_LOGI("%s,nt35695 mode: mode = %d, gbm_mode=%d is_suspend=%d\n", __func__, mode, gbm_mode, is_suspend);

	//enter hbm
	if (mode == 1 && gbm_mode == 0) {
		gbm_mode = 1;

		if (is_suspend == 1)
			return;//prize-wyq 20190521 fix Bug73848-return if display is suspended and cmdq is stoped
		
		push_table(handle, bl_hbm_enter, sizeof(bl_hbm_enter) / sizeof(struct LCM_setting_table), 1);
		
	}
	//exit hbm
	else if (mode == 0 && gbm_mode == 1) {
		gbm_mode = 0;
		
		if (is_suspend == 1)
			return;//prize-wyq 20190521 fix Bug73848-return if display is suspended and cmdq is stoped
		
		push_table(handle, bl_hbm_exit, sizeof(bl_hbm_exit) / sizeof(struct LCM_setting_table), 1);
	}
}
//prize-add wyq 20181226 add  lcd-backlight interface-start

/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 begin */
static unsigned int lcm_get_backlight_level()
{
	LCM_LOGI("%s,nt35695 backlight: level = %d\n", __func__, g_level);
	return g_level;
}
/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 end */

/* partial update restrictions:
 * 1. roi width must be 1080 (full lcm width)
 * 2. vertical start (y) must be multiple of 16
 * 3. vertical height (h) must be multiple of 16
 */
static void lcm_validate_roi(int *x, int *y, int *width, int *height)
{
	unsigned int y1 = *y;
	unsigned int y2 = *height + y1 - 1;
	unsigned int x1, w, h;

	x1 = 0;
	w = FRAME_WIDTH;

	y1 = round_down(y1, 16);
	h = y2 - y1 + 1;

	/* in some cases, roi maybe empty. In this case we need to use minimu roi */
	if (h < 16)
		h = 16;

	h = round_up(h, 16);

	/* check height again */
	if (y1 >= FRAME_HEIGHT || y1 + h > FRAME_HEIGHT) {
		/* assign full screen roi */
		pr_warn("%s calc error,assign full roi:y=%d,h=%d\n", __func__, *y, *height);
		y1 = 0;
		h = FRAME_HEIGHT;
	}

	/*pr_err("lcm_validate_roi (%d,%d,%d,%d) to (%d,%d,%d,%d)\n",*/
	/*	*x, *y, *width, *height, x1, y1, w, h);*/

	*x = x1;
	*width = w;
	*y = y1;
	*height = h;
}

struct LCM_DRIVER r66455_fhdp_dsi_vdo_visionox_lcm_drv = {
    .name		= "r66455_fhdp_dsi_vdo_visionox_lcm_drv",
	#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "r66455",
		.vendor	= "Synaptics",
		.id		= "0x80",
		.more	= "1080*2340",
	},
	#endif
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.resume         = lcm_resume,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.set_backlight_mode = lcm_set_backlight_mode,
/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 begin */
	.backlight_level = lcm_get_backlight_level,
/* prize added by lifenfen, for backlight_level func ,  get Amoled lcd backlight if lcd esd recovery, 20190221 end */
	.compare_id     = lcm_compare_id,
	.ata_check 		= lcm_ata_check,

	.validate_roi = lcm_validate_roi,	
#if (LCM_DSI_CMD_MODE)	
	.update = lcm_update,
#endif
};
