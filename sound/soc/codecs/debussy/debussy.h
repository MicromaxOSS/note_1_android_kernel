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

#ifndef DEBUSSY_H
#define DEBUSSY_H

#include "debussy_inc.h"
#include "debussy_config.h"

#include <linux/slab.h>
#include <linux/debugfs.h>
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
#include <linux/regmap.h>
#include <linux/ioctl.h>
#include "debussy_snd_ctrl.h"
#include <linux/version.h>

#include <linux/input.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
// For Android 5.1
#define snd_soc_kcontrol_codec          snd_kcontrol_chip
#endif

#define IGO_CH_WAIT_ADDR            (0xFFFFFFFF)
#define IGO_DRIVER_CMID                 "3f6c4998"

#define IGO_RST_GPIO                    (-2)
#define IGO_KWS_INT                     (-2)

//#define ENABLE_GENETLINK
//#define ENABLE_CDEV
//#define ENABLE_SPI_INTF
//#define ENABLE_FACTORY_CHECK
//#define DEBUSSY_TYPE_PSRAM
#define DEBUSSY_ERR_RECOVER_LV          2   //0: disable; 1:reload FW; 2:reload FW and recover 


#ifdef CONFIG_REGMAP_I2C
#define ENABLE_DEBUSSY_I2C_REGMAP
#endif

#ifdef CONFIG_REGMAP_SPI
//#define ENABLE_DEBUSSY_SPI_REGMAP       // TBD
#endif

//#define IG_CH_CMD_USING_SPI             // TBD

#define IGO_RST_HOLD_TIME_MIN           (10)        // ms
#define IGO_RST_RELEASE_INIT_TIME       (5000)      // us
#define IGO_RST_RELEASE_TIME_MIN        (50)       // ms
#define POWER_MODE_DELAY_CHECK           (40)
#ifndef GPIO_LOW
#define GPIO_LOW        0
#endif

#ifndef GPIO_HIGH
#define GPIO_HIGH       1
#endif
#define SOUND_TRIGGER_KEY KEY_F1
#define IGO_CH_ACTION_OFFSET (28)

#define IGO_CH_OPT_ADDR     (IGO_CH_RSV_ADDR + 0x04)
#define IGO_CH_STATUS_ADDR  (IGO_CH_RSV_ADDR + 0x08)
#define IGO_CH_CMD_ADDR     (IGO_CH_RSV_ADDR + 0x0C)
#define IGO_CH_BUF_ADDR     (IGO_CH_RSV_ADDR + 0x10)

struct debussy_priv {
    struct regmap *i2c_regmap;
    struct device* dev;
    struct device* spi_dev;
    uint32_t max_data_length_i2c;
    uint32_t max_data_length_spi;
    uint8_t dma_mode_i2c;
    uint8_t dma_mode_spi;
    uint8_t isLittleEndian;
    uint8_t factory_check;
    struct snd_soc_codec* codec;

    struct workqueue_struct* debussy_wq;
    struct workqueue_struct* debussy_fw_log_wq;
    struct workqueue_struct* debussy_voice_rec_wq;
    struct work_struct fw_work;
    struct work_struct fw_log_work;
    struct work_struct voice_rec_work;
    struct work_struct irq_work;
    struct delayed_work poweron_update_fw_work;
    unsigned int request_fw_delaytime;

    struct workqueue_struct *debussy_pull_down_delay_wq;
    struct work_struct pull_down_delay_work;
    atomic_t pull_down_state;

    struct dentry* dir;
    struct mutex igo_ch_lock;
    atomic_t maskConfigCmd;
    atomic_t referenceCount;
    atomic_t kws_triggered;
    atomic_t kws_count;
    atomic_t vad_count;
    atomic_t reset_stage;
    atomic_t curr_op_mode;

    void (*reset_chip)(struct device *, uint8_t);
    void (*chip_pull_up)(struct device *);
    void (*chip_pull_down)(struct device *);
    void (*mcu_hold)(struct device *, uint32_t);

    uint32_t reg_address;
    int32_t reset_gpio;
    uint32_t reset_hold_time;           // Reset Active Time
    uint32_t reset_release_time;        // Waiting time after reset released
    int32_t mcu_hold_gpio;
	u32 kws_irq;
	uint32_t enable_kws;
	bool vad_buf_loaded;			//judge if VAD buffer is loaded by user layer.
    struct work_struct vadbuf_chk_work; 
    struct input_dev  *input_dev;
#if (SEND_VAD_CMD_IN_DRIVER==1)
    atomic_t vad_mode_stage;
    atomic_t vad_switch_flag;
    void (*set_vad_cmd)(struct device *);
#endif
    unsigned int voice_mode;
	const struct firmware *fw;//prize huarui
    unsigned int download_firmware_status;
};

enum {
    DEBUSSY_AIF,
};

typedef enum {
	RESET_STAGE_NONE = 0,
	RESET_STAGE_LOAD_FW,
	RESET_STAGE_CALI_DONE,
	RESET_STAGE_SKIP_RESET,
	RESET_STAGE_EXEC_RESET
} DEBUSSY_RESET_STAGE_e;

typedef enum {
	SYS_GUARD_DISABLE = 0,
	SYS_GUARD_ALIVE,
	SYS_GUARD_ENABLE,
	SYS_GUARD_ALIVE_HB,

	SYS_GUARD_DEFAU
} DEBUSSY_SYS_GUARD_e;

typedef enum {
    PULL_DOWN_ST_PULL_DOWN = 0,
    PULL_DOWN_ST_CNT_DOWN,
    PULL_DOWN_ST_KEEP_ALIVE,
    PULL_DOWN_ST_WAIT_CALI_DOWN,
} DEBUSSY_PULL_DOWN_STATE_e;

typedef enum{
    VAD_MODE_ST_DISABLE = 0,
    VAD_MODE_ST_ENABLE,
    VAD_MODE_ST_BYPASS,
    //VAD_MODE_ST_CMD_DONE,
    VAD_MODE_ST_OTHER_MODE
} DEBUSSY_VAD_MODE_STATE_e;

struct ig_ioctl_rg_arg {
    uint32_t address;
    uint32_t data;
};

struct ig_ioctl_kws_arg {
    uint32_t count;
    uint32_t status;
};

struct ig_ioctl_enroll_model_arg {
    uint32_t byte_len;
    uint32_t buf_addr;
};

#if (DEBUSSY_ERR_RECOVER_LV == 2)
typedef struct
{
    unsigned int    cmd;
    unsigned int    val;
} DEBUSSY_IGO_CMD_t;

typedef struct
{
    DEBUSSY_IGO_CMD_t   backup[2][32];
    unsigned char       in_idx[2];
    unsigned char       re_send_idx;
    unsigned char       bitmap;
    unsigned char       active;
	unsigned char		prev_act;
    unsigned char       rsvd[2];
} DEBUSSY_CMD_BACKUP_t;
#endif  /* end of DEBUSSY_ERR_RECOVER_LV */

typedef struct
{
	unsigned int	ready_ptn;
	unsigned int	fw_ver;
	unsigned int	fw_sub_ver;
	unsigned char	drv_cmid[9];
	unsigned char 	rsvd[3];
	unsigned int	cali_time;		// ms
	unsigned int    cali_ctr;
	unsigned short	resend_ctr;
	unsigned short	reload_ctr;
	unsigned short	manu_load_ctr;
	unsigned short	s1_hit_ctr;
	unsigned short	s2_hit_ctr;
} DEBUSSY_INFO_t;

#if DTS_SUPPORT == 0
typedef struct
{
    char device_name[32];
    int32_t mcu_hold_gpio;
    uint32_t reset_hold_time;           // Reset Active Time
    uint32_t reset_release_time;        // Waiting time after reset released
    int32_t reset_gpio;
    uint32_t request_fw_delaytime;
    uint32_t max_data_length_i2c;
    uint32_t max_data_length_spi;
    int32_t gpio_pin;
    int32_t gpio_kws_deb;
	uint32_t enable_kws;

    uint32_t enable_spi;
    uint32_t spi_reg_address;
    uint32_t spi_max_speed;
} DTS_REPLACE_t;

extern DTS_REPLACE_t debussyDtsReplace;
#endif  /* end of DTS_SUPPORT */


#define IOCTL_ID                    '\x66'
#define IOCTL_REG_SET               _IOW(IOCTL_ID, 0, struct ig_ioctl_rg_arg)
#define IOCTL_REG_GET               _IOR(IOCTL_ID, 1, struct ig_ioctl_rg_arg)
#define IOCTL_KWS_STATUS_CLEAR      _IO(IOCTL_ID, 2)
#define IOCTL_KWS_STATUS_GET        _IOR(IOCTL_ID, 3, struct ig_ioctl_kws_arg)
#define IOCTL_ENROLL_MOD_GET        _IOR(IOCTL_ID, 4, struct ig_ioctl_enroll_model_arg)
#define IOCTL_ENROLL_MOD_SET        _IOW(IOCTL_ID, 5, struct ig_ioctl_enroll_model_arg)

extern struct debussy_priv* p_debussy_priv;
extern DEBUSSY_INFO_t	debussyInfo;


extern int debussy_cdev_init(void *priv_data);
extern void debussy_cdev_exit(void);

extern int debussy_flash_update_firmware(struct device* dev, unsigned int faddr, const u8* data, size_t size);
extern int debussy_psram_update_firmware(struct device* dev, unsigned int faddr, const u8* data, size_t size);
extern void debussy_psram_readpage(struct device* dev, unsigned int start_addr, unsigned int read_len);

extern void memcpy_u32(uint32_t *target, uint32_t *src, uint32_t word_len);
extern void memset_u32(uint32_t *target, uint32_t data, uint32_t word_len);
extern void endian_swap(unsigned int *target, unsigned int *source, unsigned int word_len);
#if (DEBUSSY_ERR_RECOVER_LV == 2)
extern void _debussy_alive_check_proc(struct debussy_priv* debussy);
#endif
#endif
