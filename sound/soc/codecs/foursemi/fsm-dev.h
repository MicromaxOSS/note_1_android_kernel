/**
 * Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
 * 2018-10-16 File created.
 */

#ifndef __FSM_DEV_H__
#define __FSM_DEV_H__

#define pr_fmt(fmt) "%s:%d: " fmt "\n", __func__, __LINE__
#if defined(__KERNEL__)
#include <linux/module.h>
#include <linux/regmap.h>
//#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#if defined(CONFIG_REGULATOR)
#include <linux/regulator/consumer.h>
#endif
#elif defined(FSM_HAL_SUPPORT)
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#if defined(CONFIG_FSM_STEREO)
#include "fsm_list.h"
#endif

/* debug info */
#ifdef FSM_DEBUG
#undef NDEBUG
#define LOG_NDEBUG 0  // open LOGV
#endif

#if defined(LOG_TAG)
#undef LOG_TAG
#endif
#define LOG_TAG "fsm_hal"


#if defined(FSM_APP)
#define pr_debug(fmt, args...) printf("%s: " fmt "\n", __func__, ##args)
#define pr_info(fmt, args...) printf("%s: " fmt "\n", __func__, ##args)
#define pr_err(fmt, args...) printf("%s: " fmt "\n", __func__, ##args)
#define pr_warning(fmt, args...) printf("%s: " fmt "\n", __func__, ##args)
#elif defined(__NDK_BUILD__)
#include<android/log.h>
#define pr_debug(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_info(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_err(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_warning(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s: " fmt, __func__, ##args)
#elif defined(__ANDROID__)
#include <utils/Log.h>
#define pr_debug(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_info(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_err(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s: " fmt, __func__, ##args)
#define pr_warning(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s: " fmt, __func__, ##args)
#else
#define pr_debug(fmt, args...)
#define pr_info(fmt, args...)
#define pr_err(fmt, args...)
#define pr_warning(fmt, args...)
#endif
#endif

/* VERSION INFORMATION */
#define FSM_CODE_VERSION ""
#define FSM_CODE_DATE ""
#define FSM_GIT_BRANCH ""
#define FSM_GIT_COMMIT ""

#define FSM_FIRMWARE_NAME  "fs16xx_fw.fsm"

#define FSM_FUNC_EXIT(ret) \
	do { \
		if (ret) \
			pr_err("ret: %d, exit", ret); \
	} while(0)

#define UNUSED(expr) do { (void)(expr); } while(0)

/* device id */
#define FS1601S_DEV_ID		0x03
#define FS1603_DEV_ID		0x05
#define FS1818_DEV_ID		0x06
#define FS1860_DEV_ID		0x07

#define CUST_NAME_SIZE		32

#define FSM_MAGNIF_FACTOR	(10) // magnification factor: 2^(x)
#define FSM_SPKR_ALLOWANCE	(30) // percentage: %
#define FSM_RCVR_ALLOWANCE	(20) // percentage: %
#define FSM_WAIT_STABLE_RETRY	(200)
#define FSM_I2C_RETRY		(20)
#define FSM_ZMDELTA_MAX		0x150
#define FS160X_RS2RL_RATIO	(2700)
#define FSM_RS_TRIM_DEFAULT	0x8f
#define FSM_MAGNIF_TEMPR_COEF	0xFFFF
#define FSM_OTP_ACC_KEY2	0xCA91

#define STEREO_COEF_LEN		(10)

#define FSM_DATA_TYPE_RE25	0
#define FSM_DATA_TYPE_ZMDATA	1

/* device mask */
#define BIT(nr)			(1UL << (nr))
#define HIGH8(val)		((val >> 8) & 0xff)
#define LOW8(val)		(val & 0xff)
#define FSM_DEV_MAX		(4)
#define FSM_ADDR_BASE		(0x34)

#define FSM_SCENE_UNKNOW	(0)
#define FSM_SCENE_MUSIC		BIT(0)
#define FSM_SCENE_VOICE		BIT(1)
#define FSM_SCENE_RING		BIT(2)
#define FSM_SCENE_VOIP		BIT(3)
#define FSM_SCENE_LOW_PWR	BIT(4)
#define FSM_SCENE_MMI_SPK	BIT(5)
#define FSM_SCENE_RCV		BIT(6)
#define FSM_SCENE_FM		BIT(7)
#define FSM_SCENE_COMMON	(0xffff)

#define FSM_DISABLE	0
#define FSM_ENABLE	1

#define STATE(tag)	FSM_STATE_##tag
#define FSM_STATE_UNKNOWN	0
#define FSM_STATE_FW_INITED	BIT(0)
#define FSM_STATE_DEV_INITED	BIT(1)
#define FSM_STATE_OTP_STORED	BIT(2)
#define FSM_STATE_CALIB_DONE	BIT(3)
#define FSM_STATE_AMP_ON	BIT(4)
#define FSM_STATE_MAX		0xffffffff// 32bit

#define ACS_COEF_COUNT		12
#define COEF_LEN		5
#define FREQ_START		200
#define F0_COUNT_MAX		20
#define FREQ_EXT_COUNT		300
#define F0_FREQ_STEP		100
#define F0_TEST_DELAY_MS	500

enum fsm_config_flag {
	USE_CODEC = 0,
	SKIP_DEVICE,
	FORCE_PRESET,
	FORCE_INIT,
	BYPASS_DSP,
	RCV_MODE,
	ATA_MODE, // Audio Test Analyzer
	FORCE_CALIB,
	CALIB_DONE,
	OTP_SAVED,
};

enum fsm_error {
	FSM_ERROR_OK = 0,
	FSM_ERROR_BAD_PARMS,
	FSM_ERROR_I2C_FATAL,
	FSM_ERROR_NO_SCENE_MATCH,
	FSM_ERROR_RE25_INVAILD,
	FSM_ERROR_MAX,
};

enum fsm_srate {
	FSM_RATE_08000 = 0,
	FSM_RATE_16000 = 3,
	FSM_RATE_32000 = 6,
	FSM_RATE_44100 = 7,
	FSM_RATE_48000 = 8,
	FSM_RATE_88200 = 9,
	FSM_RATE_96000 = 10,
	FSM_RATE_MAX,
};

enum fsm_channel {
	FSM_CHN_LEFT = 1,
	FSM_CHN_RIGHT = 2,
	FSM_CHN_MONO = 3,
};

enum fsm_format {
	FSM_FMT_DSP = 1,
	FSM_FMT_MSB = 2,
	FSM_FMT_I2S = 3,
	FSM_FMT_LSB_16 = 4,
	FSM_FMT_LSB_20 = 6,
	FSM_FMT_LSB_24 = 7,
};

enum fsm_bstmode {
	FSM_BSTMODE_FLW = 0,
	FSM_BSTMODE_BST = 1,
	FSM_BSTMODE_ADP = 2,
};

enum fsm_dsp_state {
	FSM_DSP_OFF = 0,
	FSM_DSP_ON = 1,
};

enum fsm_pll_state {
	FSM_PLL_OFF = 0,
	FSM_PLL_ON = 1,
};

enum fsm_mute_type {
	FSM_MUTE_UNKNOW = -1,
	FSM_UNMUTE = 0,
	FSM_MUTE = 1,
};

enum fsm_wait_type {
	FSM_WAIT_AMP_ON,
	FSM_WAIT_AMP_OFF,
	FSM_WAIT_AMP_ADC_OFF,
	FSM_WAIT_AMP_ADC_PLL_OFF,
	FSM_WAIT_TSIGNAL_ON,
	FSM_WAIT_TSIGNAL_OFF,
	FSM_WAIT_OTP_READY,
	FSM_WAIT_BOOST_SSEND,
};

enum fsm_eq_ram_type {
	FSM_EQ_RAM0 = 0,
	FSM_EQ_RAM1,
	FSM_EQ_RAM_MAX,
};

struct fsm_version {
	char git_branch[50];
	char git_commit[41];
	char code_date[18];
	char code_version[10];
};
typedef struct fsm_version fsm_version_t;

struct fsm_pll_config {
	unsigned int bclk;
	uint16_t c1;
	uint16_t c2;
	uint16_t c3;
};
typedef struct fsm_pll_config fsm_pll_config_t;

#define PRESET_VERSION1		0x03 // 1601S
#define PRESET_VERSION2		0x05 // 1603 series
#define PRESET_VERSION3		0x0A // 1801 series
#define PRESET_VERSION4		0x07 // 1860

enum fsm_dsc_type {
	FSM_DSC_DEV_INFO = 0,
	FSM_DSC_SPK_INFO,
	FSM_DSC_REG_COMMON,
	FSM_DSC_REG_SCENES,
	FSM_DSC_PRESET_EQ,
	FSM_DSC_STEREO_COEF,
	FSM_DSC_EXCER_RAM,
	FSM_DSC_MAX,
};

#pragma pack(push, 1)

struct fsm_date {
	uint32_t min: 6;
	uint32_t hour: 5;
	uint32_t day: 5;
	uint32_t month: 4;
	uint32_t year: 12;
};
typedef struct fsm_date fsm_date_t;

struct data32_list {
	uint16_t len;
	uint32_t data[];
};
typedef struct data32_list preset_data_t;
typedef struct data32_list ram_data_t;

struct uint24 {
	uint8_t DL;
	uint8_t DM;
	uint8_t DH;
};
typedef struct uint24 uint24_t;

struct data24_list {
	uint16_t len;
	uint24_t data[];
};

struct data16_list {
	uint16_t len;
	uint16_t data[];
};
typedef struct data16_list info_list_t;
typedef struct data16_list stereo_coef_t;

struct fsm_index {
	uint16_t offset;
	uint16_t type;
};
typedef struct fsm_index fsm_index_t;

struct dev_list {
	uint16_t preset_ver;
	char project[8];
	char customer[8];
	fsm_date_t date;
	uint16_t data_len;
	uint16_t crc16;
	uint16_t len;
	uint16_t bus;
	uint16_t addr;
	uint16_t dev_type;
	uint16_t npreset;
	uint16_t reg_scenes;
	uint16_t eq_scenes;
	fsm_index_t index[];
};
typedef struct dev_list dev_list_t;

struct preset_list {
	uint16_t len;
	uint16_t scene;
	uint32_t data[];
};
typedef struct preset_list preset_list_t;

struct reg_unit {
	uint16_t addr: 8;
	uint16_t pos: 4;
	uint16_t len: 4;
	uint16_t value;
};
typedef struct reg_unit reg_unit_t;

struct reg_comm {
	uint16_t len;
	reg_unit_t reg[];
};
typedef struct reg_comm reg_comm_t;

struct regs_unit {
	uint16_t scene;
	reg_unit_t reg;
};
typedef struct regs_unit regs_unit_t;

struct reg_scene {
	uint16_t len;
	regs_unit_t regs[];
};
typedef struct reg_scene reg_scene_t;

struct preset_header {
	uint16_t version;
	char customer[8];
	char project[8];
	fsm_date_t date;
	uint16_t size;
	uint16_t crc16;
	uint16_t ndev;
};

struct preset_file {
	struct preset_header hdr;
	fsm_index_t index[];
};

#pragma pack(pop)

struct fsm_dev;

struct fsm_ops {
	int (*init)(struct fsm_dev *dev);
	int (*switch_preset)(struct fsm_dev *dev);
	int (*start_up)(struct fsm_dev *dev);
	int (*wait_on)(struct fsm_dev *dev);
	int (*set_unmute)(struct fsm_dev *dev);
	int (*set_mute)(struct fsm_dev *dev);
	int (*wait_off)(struct fsm_dev *dev);
	int (*shut_down)(struct fsm_dev *dev);
	int (*deinit)(struct fsm_dev *dev);
	int (*pre_calib)(struct fsm_dev *dev);
	int (*post_calib)(struct fsm_dev *dev);
	int (*pre_f0_test)(struct fsm_dev *dev);
	int (*f0_test)(struct fsm_dev *dev);
	int (*post_f0_test)(struct fsm_dev *dev);
	int (*get_zmdata)(struct fsm_dev *dev);
};
typedef struct fsm_ops fsm_ops_t;

struct fsm_config {
	uint16_t next_scene;
	uint16_t flags;
	uint16_t test_freq;
	uint8_t volume;
	uint8_t channel;
	uint8_t state;
	uint8_t dev_sel;
	uint8_t store_otp;
	unsigned int i2s_bclk;
	unsigned int i2s_srate;
	uint16_t i2s_ratio;
	char *codec_name;
	char *codec_dai_name;
};
typedef struct fsm_config fsm_config_t;

struct fsm_calib {
	int re25_otp;
	int re25_new;
	uint8_t count;
};

#if !defined(CONFIG_I2C)
struct i2c_client {
	int addr;
};
#endif

struct fsm_msg {
	char *buf;
	uint32_t size;
};
typedef struct fsm_msg fsm_msg_t;

struct fsm_bpcoef {
	int freq;
	uint32_t coef[COEF_LEN];
};

struct fsm_state {
	uint16_t reg_init : 1;
	uint16_t fw_found : 1;
	uint16_t calib_ok : 1;
	uint16_t otp_store : 1;
	uint16_t amp_mute : 1;
	uint16_t dsp_bypass : 1;
};

struct fsm_dev {
	struct i2c_client *i2c;
#if defined(CONFIG_FSM_REGMAP)
	struct regmap *regmap;
#endif
#if defined(CONFIG_FSM_STEREO)
	struct list_head list;
#endif
	struct fsm_ops dev_ops;
	struct preset_file *preset;
	struct dev_list *dev_list;
	struct fsm_calib calib;
	int spkr;
	uint8_t dev_mask;
	uint16_t version;
	uint16_t flags;
	uint16_t ram_scene[FSM_EQ_RAM_MAX];
	uint16_t own_scene;
	uint16_t cur_scene;
	uint16_t zmdata;
	struct fsm_state dev_state;
	char const *dev_name;
};
typedef struct fsm_dev fsm_dev_t;

struct fsm_pdata {
#ifdef __KERNEL__
	//struct miscdevice misc_dev;
	struct regulator *vdd;
	struct snd_soc_codec *codec;
	struct workqueue_struct *fsm_wq;
	struct delayed_work init_work;
	struct delayed_work monitor_work;
	struct mutex i2c_lock;
	wait_queue_head_t wq;
	struct device *dev;
#endif
	struct fsm_dev fsm_dev;
};
typedef struct fsm_pdata fsm_pdata_t;

#endif
