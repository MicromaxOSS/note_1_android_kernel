#ifndef __GSL_TS_DRIVER_H__
#define __GSL_TS_DRIVER_H__
/*********************************/
#define TPD_HAVE_BUTTON		//按键宏
#define GSL_ALG_ID			//有没有id算法
//#define GSL_COMPATIBLE_CHIP	//芯片兼容宏
//#define GSL_THREAD_EINT		//线程中断
#define GSL_DEBUG			//调试
#define TPD_PROC_DEBUG		//adb调试
//#define GSL_TIMER			//定时器宏


//#define GSL_IRQ_CHECK

#if  0//defined (BIRD_TPD_PROXIMITY)
#define TPD_PROXIMITY		//距离传感器宏
#else
//#define TPD_PROXIMITY		//距离传感器宏
#endif


//#define GSL_GESTURE      //手势唤醒宏

//#define GSL_GPIO_IDT_TP	//GPIO兼容	
#if defined(MX2116_WPF_D5015)
#define GSL_DRV_WIRE_IDT_TP	//驱动线兼容
#endif

//#define GSL9XX_VDDIO_1800 1   //9系列IC宏

#if ( defined(MX2116_SMT_FW) ||defined(MX2103_SMT_FW)|| defined(MX2116_ARES_A5086_FW)|| defined(MX2116_ARES_A5086_QHD) || defined(MX2116_WPF_D5015)|| defined(MX2116_WPF_D5015_TEST) || defined(MX2116_BER_S4513) || defined(MX2116_BER_S5075) ||defined(MX2116_WPF_D5012)||defined(MX2116_NF_S5051)|| defined(MX2103_JC_C09_FWVGA)|| defined(MX2116_WPF_D5019)||defined (MX2116_ARES_A5083B_QHD) ||defined(MX2109F_ARES_A5085)||defined(MX2116G_BER_S5088_FW)||defined(MX2116G_WPF_D5023_HD720))
#define TPD_POWER_VTP28_USE_VCAMA   //[add for mx2116 2015-11-03]
#endif

#define GSL_PAGE_REG    0xf0
#define GSL_CLOCK_REG   0xe4
#define GSL_START_REG   0xe0
#define POWE_FAIL_REG   0xbc
#define TOUCH_INFO_REG  0x80
#define TPD_DEBUG_TIME	0x20130424
struct gsl_touch_info
{
	int x[10];
	int y[10];
	int id[10];
	int finger_num;	
};

struct gsl_ts_data {
	struct i2c_client *client;
	struct workqueue_struct *wq;
	struct work_struct work;
	unsigned int irq;
	//struct early_suspend pm;
};

/*button*/
#if defined (MX1092_ARES_5091)
#define TPD_KEY_COUNT           3
#define TPD_KEYS                {KEY_MENU,KEY_HOMEPAGE,KEY_BACK}
#define TPD_KEYS_DIM            {{80,1030,20,20},{240,1030,20,20},{460,1030,20,20}}
#elif defined(MX1092_HYF_V7)
#define TPD_KEY_COUNT           3
#define TPD_KEYS                {KEY_MENU,KEY_HOMEPAGE,KEY_BACK}
#define TPD_KEYS_DIM            {{80,1030,20,20},{240,1030,20,20},{460,1030,20,20}}
 
#else
#define TPD_KEY_COUNT           3
#define TPD_KEYS                {KEY_MENU,KEY_HOMEPAGE,KEY_BACK}
#define TPD_KEYS_DIM            {{72,884,20,20},{264,884,20,20},{408,884,20,20}}
#endif


#ifdef GSL_ALG_ID 
extern unsigned int gsl_mask_tiaoping(void);
extern unsigned int gsl_version_id(void);
extern void gsl_alg_id_main(struct gsl_touch_info *cinfo);
extern void gsl_DataInit(int *ret);


#endif
/* Fixme mem Alig */
struct fw_data
{
    u32 offset : 8;
    u32 : 0;
    u32 val;
};
#ifdef GSL_DRV_WIRE_IDT_TP
#include "gsl_idt_tp.h"
#endif


#include "gsl_ts_fw.h"

static unsigned char gsl_cfg_index = 0;

struct fw_config_type
{
	const struct fw_data *fw;
	unsigned int fw_size;
	unsigned int *data_id;
	unsigned int data_size;
};
static struct fw_config_type gsl_cfg_table[9] = {
#if defined( MX2116_WPF_D5015)
/*1*/{GSLX680_FW_HSD,(sizeof(GSLX680_FW_HSD)/sizeof(struct fw_data)),gsl_config_data_id_hsd,(sizeof(gsl_config_data_id_hsd)/4)},
/*2*/{GSLX680_FW_MWD,(sizeof(GSLX680_FW_MWD)/sizeof(struct fw_data)),gsl_config_data_id_mwd,(sizeof(gsl_config_data_id_mwd)/4)},
#else
/*0*/{GSLX680_FW,(sizeof(GSLX680_FW)/sizeof(struct fw_data)),gsl_config_data_id,(sizeof(gsl_config_data_id)/4)},
#endif
};

#endif
