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

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)            (lcm_util.set_reset_pin((v)))
#define MDELAY(n)                   (lcm_util.mdelay(n))

/* --------------------------------------------------------------------------- */
/* Local Functions */
/* --------------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------------- */
/* Local Constants */
/* --------------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE	0
#define FRAME_WIDTH  										(720)
#define FRAME_HEIGHT 										(1520)

#define REGFLAG_DELAY             							 0xFFFA
#define REGFLAG_UDELAY             							 0xFFFB
#define REGFLAG_PORT_SWAP									 0xFFFC
#define REGFLAG_END_OF_TABLE      							 0xFFFD   // END OF REGISTERS MARKER


#ifndef TRUE
    #define   TRUE     1
#endif
 
#ifndef FALSE
    #define   FALSE    0
#endif

//extern void chipone_fts_set_tp_rst(int enable);
/* --------------------------------------------------------------------------- */
/* Local Variables */
/* --------------------------------------------------------------------------- */

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_initialization_setting[] = {
{0xF0, 2,{0x5A, 0x59}},
{0xF1, 2,{0xA5, 0xA6}},
{0xB0, 32,{0x8C, 0x83, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x10, 0x00, 0x00, 0x00, 0x01, 0x01,0x81,0x01,0x01,0x0F,0x85,0x04,0x03,0x02,0x01,0x02,0x03,0x04,0x00,0x00,0x00,0x00,0x00}},
{0xB1, 32,{0xB0,0x02,0x89,0x81,0x02,0x00,0x00,0x7B,0x00,0x00,0x04,0x09,0x54,0x00,0x00,0x00,0x44,0x40,0x02,0x01,0x40,0x02,0x01,0x40,0x02,0x01,0x40,0x02,0x01,0x00,0x00,0x00}},
{0xB2, 17,{0x54,0xD4,0x82,0x05,0x40,0x02,0x01,0x40,0x02,0x01,0x05,0x05,0x54,0x0C,0x0C,0x0D,0x0B}},
{0xB3, 31,{0x02,0x0B,0x08,0x0B,0x08,0x26,0x26,0x91,0xA2,0x33,0x44,0x00,0x26,0x00,0x18,0x01,0x02,0x08,0x20,0x30,0x08,0x09,0x44,0x20,0x40,0x20,0x40,0x08,0x09,0x22,0x33}},
{0xB4, 28,{0x22,0x22,0x00,0x00,0x06,0x06,0xE3,0xE3,0xC1,0xC1,0x00,0x00,0x04,0x04,0x0F,0x0F,0x0D,0x0D,0xE3,0xE3,0x22,0x22,0xFF,0xFF,0xFC,0x00,0x00,0x00}},
{0xB5, 28,{0x00,0x00,0x00,0x00,0x05,0x05,0xE3,0xE3,0xC1,0xC1,0x00,0x00,0x04,0x04,0x0E,0x0E,0x0C,0x0C,0xE3,0xE3,0x22,0x22,0xFF,0xFF,0xFC,0x00,0x00,0x00}},
{0xB8, 24,{0x00,0x00,0x0F,0x00,0x00,0xF0,0x00,0x00,0x0F,0x00,0x00,0xF0,0x00,0x00,0x0F,0x00,0x00,0xF0,0x00,0x00,0x0F,0x00,0x00,0xF0}},
{0xBB, 13,{0x01,0x05,0x09,0x11,0x0D,0x19,0x1D,0x55,0x25,0x69,0x00,0x21,0x25}},
{0xBC, 14,{0x00,0x00,0x00,0x00,0x02,0x20,0xFF,0x00,0x03,0x11,0x01,0x73,0x44,0x00}},
{0xBD, 10,{0x53,0x12,0x4F,0xCF,0x72,0xA7,0x08,0x44,0xAE,0x15}},

//{0xBE, 10,{0x69,0x69,0x0A,0x1E,0x0C,0x77,0x43,0x07,0x0E,0x0E}},
//{0xBF, 8,{0x07, 0x25, 0x07, 0x25, 0x7F, 0x00, 0x11, 0x04}},
//{0xC0, 9,{0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00}},
{0xC1, 19,{0xC0,0x20,0x20,0x7C,0x04,0x28,0x28,0x04,0x2A,0xF0,0x35,0x00,0x07,0xCF,0xFF,0x80,0xC0,0x00,0xC0}},
{0xC2, 1,{0x00}},

{0xC3, 9,{0x06,0x00,0xFF,0x00,0xFF,0x00,0x00,0x81,0x01}},
{0xC5, 11,{0x03,0x1C,0xB8,0xB8,0x30,0x10,0x86,0x44,0x08,0x09,0x14}},
{0xC6, 10,{0x87,0xA2,0x2A,0x29,0x29,0x31,0x7F,0x04,0x08,0x04}},
//{0xC7, 22,{0xF7, 0xBC, 0x97, 0x7B, 0x51, 0x31, 0x05, 0x5C, 0x2B, 0x06, 0xE1, 0xB6, 0x13, 0xE8, 0xCE, 0xA3, 0x89, 0x61, 0x1A, 0x7F, 0xE4, 0x00}},
//{0xC8, 22,{0xF7, 0xBC, 0x97, 0x7B, 0x51, 0x31, 0x05, 0x5C, 0x2B, 0x06, 0xE1, 0xB6, 0x13, 0xE8, 0xCE, 0xA3, 0x89, 0x61, 0x1A, 0x7F, 0xE4, 0x00}},
//{0xC7, 22,{0xF7, 0xB9, 0x92, 0x77, 0x4C, 0x2D, 0xFF, 0x56, 0x27, 0x01, 0xDB, 0xB0, 0x0B, 0xE0, 0xC4, 0x9A, 0x7F, 0x59, 0x1A, 0x7E, 0xE4, 0x00}},
//{0xC8, 22,{0xF7, 0xB9, 0x92, 0x77, 0x4C, 0x2D, 0xFF, 0x56, 0x27, 0x01, 0xDB, 0xB0, 0x0B, 0xE0, 0xC4, 0x9A, 0x7F, 0x59, 0x1A, 0x7E, 0xE4, 0x00}},
//{0xC7, 22,{0xF7, 0xB5, 0x8C, 0x70, 0x44, 0x24, 0xF6, 0x4D, 0x1E, 0xF8, 0xD1, 0xA1, 0xFA, 0xCD, 0xB0, 0x85, 0x6B, 0x48, 0x1A, 0x7E, 0xC0, 0x00}},
//{0xC8, 22,{0xF7, 0xB5, 0x8C, 0x70, 0x44, 0x24, 0xF6, 0x4D, 0x1E, 0xF8, 0xD1, 0xA1, 0xFA, 0xCD, 0xB0, 0x85, 0x6B, 0x48, 0x1A, 0x7E, 0xC0, 0x00}},
//{0xC7, 22,{0xF7, 0xB3, 0x8B, 0x6D, 0x41, 0x20, 0xF3, 0x4A, 0x19, 0xF3, 0xCB, 0x9C, 0xF3, 0xC4, 0xA7, 0x7D, 0x63, 0x40, 0x1A, 0x7E, 0xC0, 0x00}},
//{0xC8, 22,{0xF7, 0xB3, 0x8B, 0x6D, 0x41, 0x20, 0xF3, 0x4A, 0x19, 0xF3, 0xCB, 0x9C, 0xF3, 0xC4, 0xA7, 0x7D, 0x63, 0x40, 0x1A, 0x7E, 0xC0, 0x00}},
//{0xC7, 22,{0xF7, 0xB1, 0x87, 0x69, 0x3D, 0x1D, 0xEE, 0x45, 0x16, 0xEF, 0xC7, 0x96, 0xEB, 0xBC, 0x9E, 0x74, 0x5B, 0x3A, 0x1A, 0x7E, 0xC0, 0x00}},
//{0xC8, 22,{0xF7, 0xB1, 0x87, 0x69, 0x3D, 0x1D, 0xEE, 0x45, 0x16, 0xEF, 0xC7, 0x96, 0xEB, 0xBC, 0x9E, 0x74, 0x5B, 0x3A, 0x1A, 0x7E, 0xC0, 0x00}},
//{0xC7, 22,{0xF7, 0xB7, 0x91, 0x74, 0x49, 0x29, 0xFC, 0x53, 0x22, 0xFC, 0xD6, 0xA9, 0x02, 0xD5, 0xB9, 0x8E, 0x75, 0x4E, 0x1A, 0x7E, 0xC4, 0x00}},
//{0xC8, 22,{0xF7, 0xB7, 0x91, 0x74, 0x49, 0x29, 0xFC, 0x53, 0x22, 0xFC, 0xD6, 0xA9, 0x02, 0xD5, 0xB9, 0x8E, 0x75, 0x4E, 0x1A, 0x7E, 0xC4, 0x00}},
{0xCB, 20,{0x00,0x40,0x7F,0x40,0x14,0x40,0x55,0x07,0x07,0x14,0x14,0xFF,0x73,0x07,0x07,0x14,0x14,0xFF,0x73,0x22}},
{0xC7, 22,{0xF7,0x99,0x6C,0x4E,0x1B,0xF9,0xC8,0x1D,0xE9,0xC2,0x9B,0x71,0xCE,0xA8,0x8F,0x6B,0x56,0x39,0x1A,0x7C,0x80,0x00}},
{0xC8, 22,{0xF7,0x99,0x6C,0x4E,0x1B,0xF9,0xC8,0x1D,0xE9,0xC2,0x9B,0x71,0xCE,0xA8,0x8F,0x6B,0x56,0x39,0x1A,0x7C,0x80,0x00}},

{0xD0, 5,{0x80,0x0D,0xFF,0x0F,0x63}},
{0xD2, 1,{0x42}},

{0xCB, 1,{0x00}},
{0xF1, 2,{0x5A, 0x59}},
{0xF0, 2,{0xA5, 0xA6}},

{0x35, 1,{0x00}},
{0x11,1,{0x00}},
{REGFLAG_DELAY,120,{}},
{0x29,1,{0x00}},
{REGFLAG_DELAY,50,{}},
{0x26, 1, {0x01}},


              
};

// static struct LCM_setting_table lcm_initialization_setting[] = {
// {0x11,0,{}},  
// {REGFLAG_DELAY,120, {}},
// {0x29,0,{}},      
// {REGFLAG_DELAY,120, {}},            
// };


static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;
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
			dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
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

	params->physical_width = 65;//LCM_PHYSICAL_WIDTH/1000;
	params->physical_height = 140;//LCM_PHYSICAL_HEIGHT/1000;
	params->physical_width_um = 64801;//LCM_PHYSICAL_WIDTH;	= sqrt((size*25.4)^2/(18^2+9^2))*9*1000
	params->physical_height_um = 140402;//LCM_PHYSICAL_HEIGHT; = sqrt((size*25.4)^2/(18^2+9^2))*18*1000
	params->density = 480;//LCM_DENSITY;

#if (LCM_DSI_CMD_MODE)
	params->dsi.mode   = CMD_MODE;
#else
	params->dsi.mode   = BURST_VDO_MODE; //SYNC_PULSE_VDO_MODE;//BURST_VDO_MODE; 
#endif
	
	// DSI
	/* Command mode setting */
	//1 Three lane or Four lane
	params->dsi.LANE_NUM				= LCM_FOUR_LANE;//LCM_FOUR_LANE;
	//The following defined the fomat for data coming from LCD engine.
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	// Video mode setting		

	params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;


	params->dsi.vertical_sync_active = 4;
	params->dsi.vertical_backporch = 32;
	params->dsi.vertical_frontporch	= 124;
	params->dsi.vertical_active_line = FRAME_HEIGHT; 

	params->dsi.horizontal_sync_active = 4;
	params->dsi.horizontal_backporch = 40;//32
	params->dsi.horizontal_frontporch = 40;//78
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	/* params->dsi.ssc_disable                                                       = 1; */
#if (LCM_DSI_CMD_MODE)
	params->dsi.PLL_CLOCK = 245;
#else
	params->dsi.PLL_CLOCK = 245;//300
#endif
	params->dsi.esd_check_enable = 1;
	params->dsi.customization_esd_check_enable = 1;
	params->dsi.lcm_esd_check_table[0].cmd			= 0x0a;
	params->dsi.lcm_esd_check_table[0].count		= 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;
}

//static void lcm_set_bias(int enable)
//{
//	
//	if (enable){
//		display_bias_vpos_set(5800);
//		display_bias_vneg_set(5800);
//		display_bias_enable();
//
//	}else{
//
//		display_bias_disable();
//		
//	}
//}

#ifndef BUILD_LK
extern atomic_t ESDCheck_byCPU;
#endif
static unsigned int lcm_ata_check(unsigned char *buffer)
{
#if 1
#ifndef BUILD_LK 
unsigned int ret = 0 ,ret1=2; 
//unsigned int x0 = FRAME_WIDTH/4; 
//unsigned int x1 = FRAME_WIDTH*3/4; 
//unsigned int y0 = 0;
//unsigned int y1 = y0 + FRAME_HEIGHT - 1;
unsigned char x0_MSB = 0x5;//((x0>>8)&0xFF); 
unsigned char x0_LSB = 0x2;//(x0&0xFF); 
unsigned char x1_MSB = 0x1;//((x1>>8)&0xFF); 
unsigned char x1_LSB = 0x4;//(x1&0xFF); 
	//unsigned char y0_MSB = ((y0>>8)&0xFF);
	//unsigned char y0_LSB = (y0&0xFF);
	//unsigned char y1_MSB = ((y1>>8)&0xFF);
	//unsigned char y1_LSB = (y1&0xFF);
	
unsigned int data_array[6]; 
unsigned char read_buf[4]; 
unsigned char read_buf1[4]; 
unsigned char read_buf2[4]; 
unsigned char read_buf3[4]; 
#ifdef BUILD_LK 
printf("ATA check size = 0x%x,0x%x,0x%x,0x%x\n",x0_MSB,x0_LSB,x1_MSB,x1_LSB); 
#else 
printk("ATA check size = 0x%x,0x%x,0x%x,0x%x\n",x0_MSB,x0_LSB,x1_MSB,x1_LSB); 
#endif 
data_array[0]= 0x0002390A;//HS packet 
data_array[1]= 0x00002453; 
dsi_set_cmdq(data_array, 2, 1); 
 data_array[0]= 0x0002390A;//HS packet 
data_array[1]= 0x0000F05e; 
dsi_set_cmdq(data_array, 2, 1); 
data_array[0]= 0x0002390A;//HS packet 
data_array[1]= 0x00000355; 
dsi_set_cmdq(data_array, 2, 1); 
//data_array[1]= (x1_MSB<<24)|(x0_LSB<<16)|(x0_MSB<<8)|0x51; 
//data_array[2]= (x1_LSB); 
//dsi_set_cmdq(data_array, 3, 1); 
data_array[0] = 0x00013700; 
dsi_set_cmdq(data_array, 1, 1); 
atomic_set(&ESDCheck_byCPU, 1);
read_reg_v2(0X56, read_buf1, 1); 
read_reg_v2(0X54, read_buf2, 1); 
read_reg_v2(0X5F, read_buf3, 1);
atomic_set(&ESDCheck_byCPU, 0);
if((read_buf1[0] == 0x03)&& (read_buf2[0] == 0x24) && (read_buf3[0] == 0xf0)) 
    ret = 1; 
else 
    ret = 0; 
#ifdef BUILD_LK 
printf("ATA read buf size = 0x%x,0x%x,0x%x,0x%x,ret= %d\n",read_buf[0],read_buf[1],read_buf[2],read_buf[3],ret); 
#else 
printk("ATA read buf  size = 0x%x,0x%x,0x%x,0x%x,ret= %d ret1= %d\n",read_buf[0],read_buf1[0],read_buf2[0],read_buf3[0],ret,ret1); 
printk("ATA read buf new  size = 0x%x,0x%x,0x%x,0x%x,ret= %d ret1= %d\n",read_buf1[0],read_buf1[1],read_buf1[2],read_buf1[3],ret,ret1); 
#endif 
return ret; 
#endif //BUILD_LK
#endif
}

static void lcm_init(void)
{

    printk("LPP----init\n");
	//SET_RESET_PIN(0);
    display_ldo18_enable(1);
    display_bias_vpos_enable(1);
    display_bias_vneg_enable(1);
    MDELAY(10);
    display_bias_vpos_set(5800);
	display_bias_vneg_set(5800);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(60);

	push_table(lcm_initialization_setting,
		   sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static struct LCM_setting_table lcm_suspend_add_setting[] = {
	{0x28,0,{}},            
	{REGFLAG_DELAY,20, {}},
	{0x10,0,{}},  
	{REGFLAG_DELAY,30, {}},

	
};

static void lcm_suspend(void)
{

	push_table(lcm_suspend_add_setting,sizeof(lcm_suspend_add_setting) / sizeof(struct LCM_setting_table), 1);
	
	SET_RESET_PIN(1);
	MDELAY(1);
	SET_RESET_PIN(0);
	MDELAY(1);
//#if !defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
	display_ldo18_enable(0);
	display_bias_disable();
//#endif
}

#if defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
static void lcm_poweroff_ext(void){
	display_ldo18_enable(0);
	display_bias_disable();
}
#endif
static void lcm_resume(void)
{
	lcm_init();
}

#if (LCM_DSI_CMD_MODE)
static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	unsigned int x0 = x;
	unsigned int y0 = y;
	unsigned int x1 = x0 + width - 1;
	unsigned int y1 = y0 + height - 1;

	unsigned char x0_MSB = ((x0 >> 8) & 0xFF);
	unsigned char x0_LSB = (x0 & 0xFF);
	unsigned char x1_MSB = ((x1 >> 8) & 0xFF);
	unsigned char x1_LSB = (x1 & 0xFF);
	unsigned char y0_MSB = ((y0 >> 8) & 0xFF);
	unsigned char y0_LSB = (y0 & 0xFF);
	unsigned char y1_MSB = ((y1 >> 8) & 0xFF);
	unsigned char y1_LSB = (y1 & 0xFF);

	unsigned int data_array[16];

	data_array[0] = 0x00053902;
	data_array[1] = (x1_MSB << 24) | (x0_LSB << 16) | (x0_MSB << 8) | 0x2a;
	data_array[2] = (x1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x00053902;
	data_array[1] = (y1_MSB << 24) | (y0_LSB << 16) | (y0_MSB << 8) | 0x2b;
	data_array[2] = (y1_LSB);
	dsi_set_cmdq(data_array, 3, 1);

	data_array[0] = 0x002c3909;
	dsi_set_cmdq(data_array, 1, 0);
}
#endif

#define LCM_ID_NT35532 (0x32)

static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[4];
	unsigned int array[16];  
	unsigned int id = 0;

    display_ldo18_enable(1);
    display_bias_vpos_enable(1);
    display_bias_vneg_enable(1);
    MDELAY(10);
    display_bias_vpos_set(5800);
	display_bias_vneg_set(5800);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(20);
    SET_RESET_PIN(1);
    MDELAY(60);


	//array[0]=0x04B01500; 
	//dsi_set_cmdq(array, 1, 1);
	array[0] = 0x00043700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0xA1, buffer, 2);


 	id = (buffer[0] << 8) + buffer[1];     /* we only need ID */
	printk("lpp--- nl9911: %s, Kernel TDDI id = %d\n", __func__, id);
	
	if (id == 0x9911)
		return 1;
	else
		return 0;

}

struct LCM_DRIVER nl9911_fhdp_dsi_vdo_incell_lcm_drv =
{
	.name		= "nl9911_fhdp_dsi_vdo_incell",
	#if defined(CONFIG_PRIZE_HARDWARE_INFO) && !defined (BUILD_LK)
	.lcm_info = {
		.chip	= "nl9911",
		.vendor	= "huajiacai",
		.id = "0x9911",
		.more	= "720*1520",
	},
   #endif
	.set_util_funcs	= lcm_set_util_funcs,
	.get_params	= lcm_get_params,
	.init		= lcm_init,
	.suspend	= lcm_suspend,
	.resume         = lcm_resume,
	.compare_id 	= lcm_compare_id,
	//.esd_check = lcm_esd_check,
    #if (LCM_DSI_CMD_MODE)
    .update         = lcm_update,
    #endif
#if defined(CONFIG_PRIZE_LCM_POWEROFF_AFTER_TP)
	.poweroff_ext	= lcm_poweroff_ext,
#endif
    .ata_check	= lcm_ata_check,
};
