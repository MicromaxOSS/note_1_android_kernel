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

#include "debussy_config.h"

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
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
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/ctype.h>

#if (XP_ENV == 1)
#include "bspchip.h"
#endif

#include "debussy.h"
#include "debussy_intf.h"
#include "debussy_snd_ctrl.h"
#ifdef ENABLE_GENETLINK
#include "debussy_genetlink.h"
#endif
#include "debussy_kws.h"
#include "debussy_customer.h"

#define DEBUSSY_FW_NAME         "debussy.bin"  
#define DEBUSSY_FW_ADDR         (0x0)

// #define ENABLE_CODEC_DAPM_CB

#define DEBUSSY_RATES SNDRV_PCM_RATE_48000
#define DEBUSSY_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define DEBUSSY_FW_DUMP_DELAY_MIN			50	// ms
#define DEBUSSY_FW_LOG_PARAM_MAX			12
#define DEBUSSY_FW_DBG_TBL							"./dbg.json"

#define DEBUSSY_READY_PATTERN         0x53595343


typedef struct
{
	unsigned short  param_num;
	unsigned short  rsvd;
	char		   msg[256];
} MSG_TBL_t;

typedef struct
{
	unsigned int	base;
	unsigned short	size;
	unsigned short	vbf_id;
	unsigned short	msg_cnt;
	unsigned short	rsvd;
	MSG_TBL_t msg_tbl[512];
} JSON_t;

#if DTS_SUPPORT == 0
/* below table must be updated by customer */
DTS_REPLACE_t debussyDtsReplace = {
    .device_name = "debussy.5d",
    .mcu_hold_gpio = BSP_GPIO_PIN_B3,
    .reset_hold_time = 5,
    .reset_release_time = 50,
    .reset_gpio = BSP_GPIO_PIN_B7,
    .loadfw_led_gpio = BSP_GPIO_PIN_B2,            // FRK
    .request_fw_delaytime = 0,
    .max_data_length_i2c = 256,
    .max_data_length_spi = 0,
    .gpio_pin = BSP_GPIO_PIN_D6,
    .gpio_kws_deb = 2,    
    .enable_kws = 1,

    .enable_spi = 0,
    .spi_reg_address = 0x2a000000,
    .spi_max_speed = 24000000,
};
#endif


#if (DEBUSSY_ERR_RECOVER_LV == 2)
DEBUSSY_CMD_BACKUP_t  debussyCmdBackup;
#endif

struct debussy_priv* p_debussy_priv = NULL;
char debussyFwDumping = 0, debussyVoiceRecording = 0, debussyVoiceRecVbId = 0;
int reset_chip_is_processing = 0;
int  debussyFwDumpDelay = DEBUSSY_FW_DUMP_DELAY_MIN, debussyVoiceRecordDelay = 5, debussyClkRateRepeat = 0;
JSON_t	debussyDbgJson;
DEBUSSY_INFO_t	debussyInfo;

//extern int lcm_suspend_flag;

//extern int debussy_cdev_init(void *priv_data);
//extern void debussy_cdev_exit(void);
//extern int debussy_kws_init(struct debussy_priv* debussy);
static int _debussy_hif_qck_cali(struct debussy_priv* debussy);
static void _debussy_manual_load_firmware(struct work_struct* work);
static void _debussy_pull_down_chip(struct device* dev);
static void _debussy_chip_pull_up_ctrl(struct device* dev);
#ifdef ENABLE_DEBUSSY_I2C_REGMAP
static bool debussy_readable(struct device *dev, unsigned int reg)
{
    return true;
}

static bool debussy_writeable(struct device *dev, unsigned int reg)
{
    return true;
}

static bool debussy_volatile(struct device *dev, unsigned int reg)
{
    return true;
}

static bool debussy_precious(struct device *dev, unsigned int reg)
{
    return false;
}

const struct regmap_config debussy_i2c_regmap = {
    .reg_bits = 32,
    .val_bits = 32,
    .reg_stride = 4,

    //.cache_type = REGCACHE_NONE,
    .cache_type = REGCACHE_RBTREE,
    .reg_format_endian = REGMAP_ENDIAN_BIG,
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    .val_format_endian = REGMAP_ENDIAN_NATIVE,
    #else
    .val_format_endian = REGMAP_ENDIAN_LITTLE,
    #endif

    .max_register = (unsigned int) -1,
    .readable_reg = debussy_readable,
    .writeable_reg = debussy_writeable,
    .volatile_reg = debussy_volatile,
    .precious_reg = debussy_precious,
};
#endif
#if (SEND_VAD_CMD_IN_DRIVER==1)
static ssize_t _debussy_vad_mode_switch(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_vad_mode_switch_fops = {
    .open = simple_open,
    .write = _debussy_vad_mode_switch,
};
#endif 
static ssize_t _debussy_download_firmware(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_download_firmware_fops = {
    .open = simple_open,
    .write = _debussy_download_firmware,
};
static ssize_t _debussy_download_firmware_status(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);

static const struct file_operations download_firmware_status_fops = {
    .open = simple_open,
    .write = _debussy_download_firmware_status,
};

static ssize_t _debussy_reset_chip(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_reset_chip_fops = {
    .open = simple_open,
    .write = _debussy_reset_chip,
};

static ssize_t _debussy_reset_gpio_pull_down(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_reset_gpio_pull_down_fops = {
    .open = simple_open,
    .write = _debussy_reset_gpio_pull_down,
};

static ssize_t _debussy_reset_gpio_pull_up(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_reset_gpio_pull_up_fops = {
    .open = simple_open,
    .write = _debussy_reset_gpio_pull_up,
};

static ssize_t _debussy_mcu_hold(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_mcu_hold_fops = {
    .open = simple_open,
    .write = _debussy_mcu_hold,
};

static ssize_t _debussy_get_fw_version(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_get_fw_version_fops = {
    .open = simple_open,
    .write = _debussy_get_fw_version,
};

static ssize_t _debussy_reg_get(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_reg_get_fops = {
    .open = simple_open,
    .write = _debussy_reg_get,
};

static ssize_t _debussy_reg_put(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_reg_put_fops = {
    .open = simple_open,
    .write = _debussy_reg_put,
};

static ssize_t _debussy_fw_log(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_fw_log_fops = {
    .open = simple_open,
    .write = _debussy_fw_log,
};

static ssize_t _debussy_voice_rec(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_voice_rec_fops = {
    .open = simple_open,
    .write = _debussy_voice_rec,
};

static ssize_t _debussy_clk_rate(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_clk_rate_fops = {
    .open = simple_open,
    .write = _debussy_clk_rate,
};

static ssize_t _debussy_sys_info(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_sys_info_fops = {
    .open = simple_open,
    .write = _debussy_sys_info,
};

#ifdef ENABLE_FACTORY_CHECK
static ssize_t _debussy_factory_check(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static ssize_t _debussy_factory_check_read(struct file* file,
    char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_factory_check_fops = {
    .open = simple_open,
    .write = _debussy_factory_check,
    .read = _debussy_factory_check_read,
};
#endif  


#ifdef DEBUSSY_TYPE_PSRAM
static ssize_t _debussy_psram_dump(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_psram_dump_fops = {
    .open = simple_open,
    .write = _debussy_psram_dump,
};
#endif

static ssize_t _debussy_alive_check(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_alive_check_fops = {
    .open = simple_open,
    .write = _debussy_alive_check,
};

static ssize_t _debussy_hif_calibration(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_hif_calibration_fops = {
    .open = simple_open,
    .write = _debussy_hif_calibration,
};

static loff_t _debussy_reg_seek(struct file *file,
    loff_t p, int orig);
static ssize_t _debussy_reg_read(struct file* file,
    char __user* user_buf,
    size_t count, loff_t* ppos);
static ssize_t _debussy_reg_write(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_reg_fops = {
    .open = simple_open,
    .llseek = _debussy_reg_seek,
    .read = _debussy_reg_read,
    .write = _debussy_reg_write,
};

#ifdef ENABLE_GENETLINK
static ssize_t _debussy_igal_put(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_igal_put_fops = {
    .open = simple_open,
    .write = _debussy_igal_put,
};

static ssize_t _debussy_igal_get(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_igal_get_fops = {
    .open = simple_open,
    .write = _debussy_igal_get,
};
#endif

#ifdef ENABLE_SPI_INTF
static ssize_t _debussy_spi_en(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_spi_en_fops = {
    .open = simple_open,
    .write = _debussy_spi_en,
};
#endif


#if (DTS_SUPPORT == 0)
static const struct platform_device_id igo_i2c_id[] = {
#else
static const struct i2c_device_id igo_i2c_id[] = {
#endif
    { "debussy", 0 },
    {}
};
MODULE_DEVICE_TABLE(i2c, igo_i2c_id);

static const struct of_device_id debussy_of_match[] = {
    { .compatible = "intelligo,debussy" },
    {},
};
MODULE_DEVICE_TABLE(of, debussy_of_match);


static void _debussy_pull_up_chip(struct device* dev)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dev));    
    struct i2c_client* client = i2c_verify_client(debussy->dev);
	//prize--huangjiwu-?????????-begin
	debussy_power_enable(1);
	//prize--huangjiwu-?????????--end
    if (gpio_is_valid(debussy->reset_gpio)) {
        gpio_direction_output(debussy->reset_gpio, GPIO_HIGH);
        gpio_set_value(debussy->reset_gpio, GPIO_HIGH);
        usleep_range(IGO_RST_RELEASE_INIT_TIME - 1, IGO_RST_RELEASE_INIT_TIME);
#ifdef DEBUSSY_TYPE_PSRAM
        {
            /* PSRAM exit QPI */
            igo_i2c_write(client, 0x2a00003c, 0x830001);
            igo_i2c_write(client, 0x2a013024, 0xc00e00f5);
            igo_i2c_write(client, 0x2a013028, 0);
            igo_i2c_write(client, 0x2a01302c, 0);
            igo_i2c_write(client, 0x2a013030, 0);
            igo_i2c_write(client, 0x2a013034, 0);
        }
#endif	/* end of DEBUSSY_TYPE_PSRAM */

        igo_i2c_write(client, 0x2A000018, 0);
        msleep(debussy->reset_release_time);
        igo_spi_intf_enable(1);

        queue_work(debussy->debussy_pull_down_delay_wq, &debussy->pull_down_delay_work);

        debussyCmdBackup.active = debussyCmdBackup.prev_act;
        if (atomic_read(&debussy->reset_stage) != RESET_STAGE_LOAD_FW)
        {
            atomic_set(&debussy->reset_stage, RESET_STAGE_CALI_DONE);
        }
        dev_info(debussy->dev, "%s:IG exit sleep mode!!!\n", __func__);
    }
    else {
        dev_err(debussy->dev, "%s: Reset GPIO-%d is invalid\n", __func__, debussy->reset_gpio);
    }

}

static void _debussy_pull_down_chip(struct device* dev)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dev));    
    struct i2c_client* client = i2c_verify_client(debussy->dev);

    if (gpio_is_valid(debussy->reset_gpio)) {
		debussyCmdBackup.prev_act = debussyCmdBackup.active;
		debussyCmdBackup.active = SYS_GUARD_DISABLE;
        atomic_set(&debussy->reset_stage, RESET_STAGE_CALI_DONE);
        igo_i2c_write(client, 0x2A000018, 1);
        igo_i2c_write(client, 0x2A00003C, 0xFE83FFFF);
        msleep(10);

        dev_info(debussy->dev, "%s:IG enter sleep mode!!!\n", __func__);
        gpio_direction_output(debussy->reset_gpio, GPIO_LOW);
        gpio_set_value(debussy->reset_gpio, GPIO_LOW);
//prize--huangjiwu-?????????-begin
		debussy_power_enable(0);
//prize--huangjiwu-?????????-end		
        usleep_range(IGO_RST_RELEASE_INIT_TIME - 1, IGO_RST_RELEASE_INIT_TIME);
    }
    else {
        dev_err(debussy->dev, "%s: Reset GPIO-%d is invalid\n", __func__, debussy->reset_gpio);
    }
}

#if (SEND_VAD_CMD_IN_DRIVER==1)
static void _debussy_set_vad_cmd(struct device* dev)
{
    int status;
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dev));    

    dev_info(debussy->dev, "%s(+)!!!!!!!!) \n", __func__);

    igo_spi_intf_enable(0);
    status = igo_ch_write(debussy->dev, IGO_CH_POWER_MODE_ADDR, POWER_MODE_STANDBY);
    dev_info(debussy->dev, "%s():  IGO_CH_POWER_MODE_ADDR cmd write, ret no : %d\n", __func__,status);
    status = igo_ch_write(debussy->dev, IGO_CH_POWER_MODE_ADDR, POWER_MODE_WORKING);
    dev_info(debussy->dev, "%s():  IGO_CH_POWER_MODE_ADDR cmd write, ret no : %d\n", __func__,status);

    status = igo_ch_write(debussy->dev, IGO_CH_DMIC_M_CLK_SRC_ADDR, DMIC_M_CLK_SRC_INTERNAL);
    dev_info(debussy->dev, "%s():  IGO_CH_DMIC_M_CLK_SRC_ADDR cmd write, ret : %d\n", __func__,status);

    status = igo_ch_write(debussy->dev, IGO_CH_DMIC_M_BCLK_ADDR, 768);
    dev_info(debussy->dev, "%s():  IGO_CH_DMIC_M_BCLK_ADDR cmd write, ret : %d\n", __func__,status);

    status = igo_ch_write(debussy->dev, IGO_CH_UL_RX_PRI_ADDR, UL_RX_PRI_DMIC_M0_P);
    dev_info(debussy->dev, "%s():  IGO_CH_UL_RX_PRI_ADDR cmd write, ret : %d\n", __func__,status);

    status = igo_ch_write(debussy->dev, IGO_CH_VAD_INT_PIN_ADDR, VAD_INT_PIN_DAI2_RXDAT);
    dev_info(debussy->dev, "%s():  IGO_CH_VAD_INT_PIN_ADDR cmd write, ret  : %d\n", __func__,status);

    status = igo_ch_write(debussy->dev, IGO_CH_VAD_INT_MOD_ADDR, VAD_INT_MOD_EDGE);
    dev_info(debussy->dev, "%s():  IGO_CH_VAD_INT_MOD_ADDR cmd write, ret : %d\n", __func__,status);
    
    status = igo_ch_write(debussy->dev, IGO_CH_OP_MODE_ADDR, OP_MODE_LPVAD);
    dev_info(debussy->dev, "%s():  IGO_CH_OP_MODE_ADDR cmd write , ret : %d\n", __func__,status);
    

    dev_info(debussy->dev, "%s(-)!!!!!!!!) \n", __func__);
}
#endif
static void _debussy_chip_pull_up_ctrl(struct device* dev)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dev));    
    unsigned int pullDownState = atomic_read(&debussy->pull_down_state);

    dev_info(debussy->dev, "%s: pullDownState=%d \n", __func__, pullDownState);

    if (pullDownState > PULL_DOWN_ST_PULL_DOWN)
    {
        debussy->reset_chip(debussy->dev, 1);
        atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_KEEP_ALIVE);
        return;
    }

    _debussy_pull_up_chip(debussy->dev);
}
static void _debussy_chip_pull_down_ctrl(struct device* dev)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dev));    
    unsigned int pullDownState = atomic_read(&debussy->pull_down_state);

    debussy->reset_chip(debussy->dev, 1);

    if (pullDownState != PULL_DOWN_ST_PULL_DOWN)
    {
        dev_info(debussy->dev, "%s: pull_down_state=>%d\n", __func__, PULL_DOWN_ST_CNT_DOWN);
        atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_CNT_DOWN);
    }
}

static void _debussy_reset_chip_ctrl(struct device* dev, uint8_t free_run)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dev));
    struct i2c_client* client = i2c_verify_client(debussy->dev);
#if (DEBUSSY_ERR_RECOVER_LV == 1)
    static char reload_fw_flag = 0;
#endif  /* end of DEBUSSY_ERR_RECOVER_LV */       
    unsigned int resetStage = atomic_read(&debussy->reset_stage);

    if (resetStage == RESET_STAGE_SKIP_RESET)
    {
        dev_info(debussy->dev, "%s - reset_stage = %d, skip reset", __func__, resetStage);
        return;
    }
    else if (resetStage == RESET_STAGE_EXEC_RESET || resetStage == RESET_STAGE_CALI_DONE)
    {
        atomic_set(&debussy->reset_stage, RESET_STAGE_SKIP_RESET);
        dev_info(debussy->dev, "%s - reset chip, reset_stage change to %d", __func__, RESET_STAGE_SKIP_RESET);
    }

	dev_info(debussy->dev, "%s: <<<reset_chip running>>> active=%d, prev=%d\n", __func__, debussyCmdBackup.active, debussyCmdBackup.prev_act);
	
#ifdef ENABLE_CODEC_DAPM_CB
    if (free_run)
        atomic_set(&debussy->referenceCount, 0);
    else
        atomic_set(&debussy->referenceCount, 1);
#endif

    if (gpio_is_valid(debussy->reset_gpio)) {
		reset_chip_is_processing = true;
        igo_i2c_write(client, 0x2A000018, 1);
        igo_i2c_write(client, 0x2A00003C, 0xFE83FFFF);
        msleep(10);
        
        gpio_direction_output(debussy->reset_gpio, GPIO_LOW);
        gpio_set_value(debussy->reset_gpio, GPIO_LOW);

        if (debussy->reset_hold_time >= 20) {
            usleep_range(debussy->reset_hold_time * 1000 - 1, debussy->reset_hold_time * 1000);
        }
        else {
            msleep(debussy->reset_hold_time);
        }

        gpio_direction_output(debussy->reset_gpio, GPIO_HIGH);
        gpio_set_value(debussy->reset_gpio, GPIO_HIGH);
        usleep_range(IGO_RST_RELEASE_INIT_TIME - 1, IGO_RST_RELEASE_INIT_TIME);

#ifdef DEBUSSY_TYPE_PSRAM
	{
        /* PSRAM exit QPI */
		igo_i2c_write(client, 0x2a00003c, 0x830001);
		igo_i2c_write(client, 0x2a013024, 0xc00e00f5);
        igo_i2c_write(client, 0x2a013028, 0);
        igo_i2c_write(client, 0x2a01302c, 0);
        igo_i2c_write(client, 0x2a013030, 0);
        igo_i2c_write(client, 0x2a013034, 0);
	}
#endif	/* end of DEBUSSY_TYPE_PSRAM */


        if (free_run) {
            igo_i2c_write(client, 0x2A000018, 0);
        }

        msleep(debussy->reset_release_time);
        igo_spi_intf_enable(1);

		reset_chip_is_processing = false;
        dev_info(dev, "%s: Reset debussy chip done\n", __func__);
    }
    else {
        dev_err(dev, "%s: Reset GPIO-%d is invalid\n", __func__, debussy->reset_gpio);
    }
	
	debussyCmdBackup.active = debussyCmdBackup.prev_act;	
	dev_info(debussy->dev, "%s: active=%d, prev=%d\n", __func__, debussyCmdBackup.active, debussyCmdBackup.prev_act);
}

static ssize_t _debussy_reset_chip(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
	unsigned char input[8] = {0};
	int input_val = 0;

	if (copy_from_user(&input, user_buf, count)) {
        return -EFAULT;
    }

	input[count] = 0;
	sscanf(input, "%d", &input_val);

	if (input_val == 2)		// force reset
	{
		atomic_set(&debussy->reset_stage, RESET_STAGE_CALI_DONE);
		dev_info(debussy->dev, "%s - reset_stage = %d", __func__, RESET_STAGE_CALI_DONE);
	}

    mutex_lock(&debussy->igo_ch_lock);
    debussy->reset_chip(debussy->dev, 1);
    igo_spi_intf_enable(1);
    mutex_unlock(&debussy->igo_ch_lock);

    return count;
}
#define PULL_DOWN_WAIT_TIME     (3)
static void _debussy_pull_down_delay(struct work_struct* work)
{
    struct debussy_priv* debussy = container_of(work, struct debussy_priv, pull_down_delay_work);
    unsigned char pull_down_ctr = PULL_DOWN_WAIT_TIME;
    unsigned int pullDownState = 0;

    atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_KEEP_ALIVE);

    while (1)
    {            
        if (debussyVoiceRecording){
            return;
        }
        msleep(1000);

        if (debussy->voice_mode == InPhoneCall)
        {
            atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_KEEP_ALIVE);
            pull_down_ctr = PULL_DOWN_WAIT_TIME;
        }

        pullDownState = atomic_read(&debussy->pull_down_state);

#if (SEND_VAD_CMD_IN_DRIVER==1)
        if (1 == atomic_read(&debussy->vad_switch_flag))
        {
            dev_info(debussy->dev, "%s: VAD enabled, loop terminate.\n", __func__);
            return;
        }
#endif            
        if (pullDownState == PULL_DOWN_ST_WAIT_CALI_DOWN)
        {
            if (atomic_read(&debussy->reset_stage) < RESET_STAGE_CALI_DONE)
                pull_down_ctr = 5;
            else
                atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_CNT_DOWN);
        }

        if (pullDownState == PULL_DOWN_ST_KEEP_ALIVE)
        {
            pull_down_ctr = PULL_DOWN_WAIT_TIME;
            continue;
        }
		
        if (pullDownState <= PULL_DOWN_ST_CNT_DOWN && --pull_down_ctr == 0)
        {
            dev_info(debussy->dev, "%s: break loop and start to pull down !!! \n", __func__);
            atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_PULL_DOWN);
            break;
        }
    }; 

    /* pull down debussy reset pin */
    mutex_lock(&debussy->igo_ch_lock);
	_debussy_pull_down_chip(debussy->dev);
    mutex_unlock(&debussy->igo_ch_lock);
}

static ssize_t _debussy_reset_gpio_pull_down(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    
    mutex_lock(&debussy->igo_ch_lock);
	_debussy_chip_pull_down_ctrl(debussy->dev);
    mutex_unlock(&debussy->igo_ch_lock);
	
    return count;
}

static ssize_t _debussy_reset_gpio_pull_up(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;

    mutex_lock(&debussy->igo_ch_lock);
    _debussy_chip_pull_up_ctrl(debussy->dev);
    mutex_unlock(&debussy->igo_ch_lock);

    return count;
}

static void _debussy_mcu_hold_ctrl(struct device* dev, uint32_t hold)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dev));

    if (gpio_is_valid(debussy->mcu_hold_gpio)) {
#ifdef DEBUSSY_TYPE_PSRAM
        gpio_direction_output(debussy->mcu_hold_gpio, GPIO_HIGH);
#else
        gpio_direction_output(debussy->mcu_hold_gpio, hold ? GPIO_HIGH : GPIO_LOW);
        dev_info(dev, "%s: MCU Hold - Level-%d\n", __func__, hold);
#endif
        usleep_range(499, 500);
    }
    else {
        dev_err(debussy->dev, "%s: MCU Hold GPIO-%d is invalid\n", __func__, debussy->mcu_hold_gpio);
    }
}

static ssize_t _debussy_mcu_hold(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    unsigned int data, position;
    char *input_data;

    if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {  
        dev_err(debussy->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(debussy->dev, input_data);
        return -EFAULT;
    }

    position = strcspn(input_data, "1234567890abcdefABCDEF");
    data = simple_strtoul(&input_data[position], NULL, 10);
    debussy->mcu_hold(debussy->dev, data ? 1 : 0);
    devm_kfree(debussy->dev, input_data);

    return count;
}

static void _debussy_get_fw_ver(struct debussy_priv* debussy)
{
    //mutex_lock(&debussy->igo_ch_lock);
    igo_ch_read(debussy->dev, IGO_CH_FW_VER_ADDR, &debussyInfo.fw_ver);
    dev_info(debussy->dev, "CHIP FW VER: %08X\n", debussyInfo.fw_ver);
    igo_ch_read(debussy->dev, IGO_CH_FW_SUB_VER_ADDR, &debussyInfo.fw_sub_ver);
    dev_info(debussy->dev, "CHIP FW SUB_VER: 0x%08X\n", debussyInfo.fw_sub_ver);
    //mutex_unlock(&debussy->igo_ch_lock);
}

static ssize_t _debussy_get_fw_version(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;

    mutex_lock(&debussy->igo_ch_lock);
    _debussy_get_fw_ver(debussy);
    mutex_unlock(&debussy->igo_ch_lock);

    return count;
}

// Usage: echo address > /d/debussy/reg_get
static ssize_t _debussy_reg_get(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    struct i2c_client* client;
    unsigned int reg, data, position;
    char *input_data;

    client = i2c_verify_client(debussy->dev);

    //dev_info(debussy->dev, "%s -\n", __func__);

    if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {  
        dev_err(debussy->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(debussy->dev, input_data);
        return -EFAULT;
    }

    //dev_info(debussy->dev, "%s: input_data = %s\n", __func__, input_data);

    position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
    reg = simple_strtoul(&input_data[position], NULL, 16);
    mutex_lock(&debussy->igo_ch_lock);
    igo_i2c_read(client, reg, &data);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(debussy->dev, "%s: Reg0x%X = 0x%X\n", __func__, reg, data);
    devm_kfree(debussy->dev, input_data);

    return count;
}

// Usage: echo address, data > /d/debussy/reg_put
static ssize_t _debussy_reg_put(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    struct i2c_client* client;
    unsigned int reg, data;
    size_t position;
    char *input_data, *next_data;

    client = i2c_verify_client(debussy->dev);

    if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {  
        dev_err(debussy->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(debussy->dev, input_data);
        return -EFAULT;
    }

    dev_info(debussy->dev, "%s: input_data = %s\n", __func__, input_data);

    position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
    reg = simple_strtoul(&input_data[position], &next_data, 16);
    position = strcspn(next_data, "1234567890abcdefABCDEF");                // find next number
    data = simple_strtoul(&next_data[position], NULL, 16);
    dev_info(debussy->dev, "%s: reg = 0x%X, data = 0x%X\n", __func__, reg, data);

    mutex_lock(&debussy->igo_ch_lock);
    igo_i2c_write(client, reg, data);
    devm_kfree(debussy->dev, input_data);
    mutex_unlock(&debussy->igo_ch_lock);

    return count;
}

// Usage: echo input_delay > /d/debussy/fw_log
static ssize_t _debussy_fw_log(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy;
	unsigned char input[16] = {0};
	int input_delay = 0;

    debussy = file->private_data;

	if (copy_from_user(&input, user_buf, count)) {
        return -EFAULT;
    }

	input[count] = 0;
	sscanf(input, "%d", &input_delay);

    dev_info(debussy->dev, "%s - input=%s,count=%d, input_delay=%d\n", __func__, input, (int)count, input_delay);

	if (input_delay == 0)
	{
		debussyFwDumping = 0;
		dev_info(debussy->dev, "%s - FW log dump stop\n", __func__);
    	return count;
	}
	else if (input_delay < DEBUSSY_FW_DUMP_DELAY_MIN)
	{
		input_delay = DEBUSSY_FW_DUMP_DELAY_MIN;
	}

	debussyFwDumpDelay = DEBUSSY_FW_DUMP_DELAY_MIN;
	if (input_delay > DEBUSSY_FW_DUMP_DELAY_MIN)
	{
		debussyFwDumpDelay = input_delay;
	}
   	dev_info(debussy->dev, "%s - debussyFwDumpDelay=%d\n", __func__, debussyFwDumpDelay);
	
	if (debussyFwDumping == 0)
	{
		debussyFwDumping = 1;
	    queue_work(debussy->debussy_fw_log_wq, &debussy->fw_log_work);
		dev_info(debussy->dev, "%s - FW log dump start\n", __func__);
	}
	else
	{
		dev_info(debussy->dev, "%s - FW log is dumping\n", __func__);
	}

    return count;
}

// Usage: echo vbid > /d/debussy/voice_rec
static ssize_t _debussy_voice_rec(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy;
	unsigned char input[16] = {0};
	int vbid = 0;

    debussy = file->private_data;

	if (copy_from_user(&input, user_buf, count)) {
        return -EFAULT;
    }

	input[count] = 0;
	sscanf(input, "%d", &vbid);

    dev_info(debussy->dev, "%s - input=%s,count=%d, vb_id=%d\n", __func__, input, (int)count, vbid);

	if (vbid == 0)
	{
		debussyVoiceRecording = 0;
		dev_info(debussy->dev, "%s - voice record stop\n", __func__);
    	return count;
	}
	else
	{
		debussyVoiceRecVbId = vbid;
	}

	
	if (debussyVoiceRecording == 0)
	{
		debussyVoiceRecording = 1;
	    queue_work(debussy->debussy_voice_rec_wq, &debussy->voice_rec_work);
        dev_info(debussy->dev, "%s - voice record start, vbid=%d\n", __func__, debussyVoiceRecVbId);
	}
	else
	{
		dev_info(debussy->dev, "%s - voice record is running\n", __func__);
	}

    return count;
}


static void _debussy_clock_rate_cal(struct debussy_priv* debussy, unsigned int repeat_cnt)
{
    struct timeval ts1, ts2;
    struct i2c_client* client;
    //struct rtc_time tm;
    unsigned int data1, data2, data, time, clk_rate;
	unsigned int reg_addr = 0x2a000108;
	unsigned int idx = 0, clk_ctr[4] = {0, 0, 0, 0};
    client = i2c_verify_client(debussy->dev);

    /*if (!copy_from_user(&input, user_buf, count)) {
        dev_info(debussy->dev, "input=0x%x", input);
    }*/

	debussyClkRateRepeat = repeat_cnt;

	while (debussyClkRateRepeat)
	{
	    mutex_lock(&debussy->igo_ch_lock);
	    do_gettimeofday(&ts1);
	    igo_i2c_read(client, reg_addr, &data1);
	    mutex_unlock(&debussy->igo_ch_lock);

		msleep(20);

	    mutex_lock(&debussy->igo_ch_lock);
	    do_gettimeofday(&ts2);
	    igo_i2c_read(client, reg_addr, &data2);
	    mutex_unlock(&debussy->igo_ch_lock);

	    /*dev_info(debussy->dev, "%s - sec=%ld, usec=%ld, %ld\n", __func__, 
					ts1.tv_sec, ts1.tv_usec, data1);
	    dev_info(debussy->dev, "%s - sec=%ld, usec=%ld, %ld\n", __func__, 
					ts2.tv_sec, ts2.tv_usec, data2);*/

		data = data2-data1;
		ts2.tv_usec = ts2.tv_usec-ts1.tv_usec;
		ts2.tv_sec = ts2.tv_sec-ts1.tv_sec;
		time = (ts2.tv_sec*1000000+ts2.tv_usec);
		clk_rate = data/time;
		
	    dev_info(debussy->dev, "debussy - <%d:clock_rate=%d.%d>\n",
					++idx,
					clk_rate,
					(data*1000/time)-(clk_rate*1000));

		if (clk_rate < 100)
			clk_ctr[0]++;
		else if (clk_rate < 200)
			clk_ctr[1]++;
		else if (clk_rate <= 234)
			clk_ctr[2]++;
		else
			clk_ctr[3]++;

		if (--debussyClkRateRepeat > 0)
			msleep(30);
	}

	dev_info(debussy->dev, "%s - stop : 0~100:%d, 101~200:%d, 200~234:%d\n", 
				__func__, clk_ctr[0], clk_ctr[1], clk_ctr[2]);
}

// Usage: echo count > /d/debussy/clk_rate
static ssize_t _debussy_clk_rate(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;

	unsigned char input[8] = {0};
	int input_repeat = 0;

    debussy = file->private_data;

	if (copy_from_user(&input, user_buf, count)) {
        return -EFAULT;
    }
    
    dev_info(debussy->dev, "%s - input=%s,count=%d\n", __func__, input, (int)count);

	input[count] = 0;
	sscanf(input, "%d", &input_repeat);

	if (input_repeat > 0)
		_debussy_clock_rate_cal(debussy, input_repeat);
	else
		debussyClkRateRepeat = 0;
		
    return count;
}

// Usage: echo 1 > /d/debussy/sys_info
static ssize_t _debussy_sys_info(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;

	dev_info(debussy->dev, "[DebussyInfo] Ready Pattern    : %08x\n",    debussyInfo.ready_ptn);
	dev_info(debussy->dev, "[DebussyInfo] FW version       : %08x\n",    debussyInfo.fw_ver);
	dev_info(debussy->dev, "[DebussyInfo] FW sub-version   : %08x\n",    debussyInfo.fw_sub_ver);
	dev_info(debussy->dev, "[DebussyInfo] Driver Commit ID : %s\n",    	 debussyInfo.drv_cmid);
	dev_info(debussy->dev, "[DebussyInfo] cali time        : %d (ms)\n", debussyInfo.cali_time);
	dev_info(debussy->dev, "[DebussyInfo] cali counter     : %d \n",     debussyInfo.cali_ctr);
	dev_info(debussy->dev, "[DebussyInfo] re-send ctr      : %d \n",	 debussyInfo.resend_ctr);
	dev_info(debussy->dev, "[DebussyInfo] re-load ctr      : %d \n",	 debussyInfo.reload_ctr);
	dev_info(debussy->dev, "[DebussyInfo] manu-load ctr    : %d \n",	 debussyInfo.manu_load_ctr);
	dev_info(debussy->dev, "[DebussyInfo] starge 1 hit ctr : %d \n",	 debussyInfo.s1_hit_ctr);
	dev_info(debussy->dev, "[DebussyInfo] starge 2 hit ctr : %d \n",	 debussyInfo.s2_hit_ctr);

    return count;
}

#ifdef ENABLE_FACTORY_CHECK
static int _igo_ch_igo_cmd_put(struct debussy_priv* debussy,
    int mode_index)
{
    int status = IGO_CH_STATUS_DONE;
    int index;
    struct ST_IGO_DEBUSSY_CFG *pst_scenarios_tab;

    mutex_lock(&debussy->igo_ch_lock);

    pst_scenarios_tab = (struct ST_IGO_DEBUSSY_CFG *) mode_check_table[mode_index];

    index = 0;
    while (1) {
        if ((IGO_CH_WAIT_ADDR == (pst_scenarios_tab + index)->cmd_addr) &&
            (0xFFFFFFFF == (pst_scenarios_tab + index)->cmd_data)) {
            dev_info(debussy->dev, "Wrong delay time\n");
            break;
        }

        if ((IGO_CH_WAIT_ADDR == (pst_scenarios_tab + index)->cmd_addr) &&
            (0xFFFFFFFF > (pst_scenarios_tab + index)->cmd_data)) {

            if ((pst_scenarios_tab + index)->cmd_data < 20) {
                usleep_range((pst_scenarios_tab + index)->cmd_data * 1000 - 1,
                             (pst_scenarios_tab + index)->cmd_data * 1000);
            }
            else {
                msleep((pst_scenarios_tab + index)->cmd_data);
            }
        }
        else {
            status = igo_ch_write(debussy->dev, (unsigned int) (pst_scenarios_tab + index)->cmd_addr, (unsigned int) (pst_scenarios_tab + index)->cmd_data);
			if(IGO_CH_STATUS_DONE!=status){
				dev_err(debussy->dev, "%s : igo_ch_write failed!!!\n",__func__);
				return status;
			}
        }

        index++;
    }

    mutex_unlock(&debussy->igo_ch_lock);

    return status;
}

// Usage: echo 1 > /d/debussy/factory_check
static ssize_t _debussy_factory_check(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
	unsigned int fw_sub_ver,ready_ptn;
	int i;
	int status = IGO_CH_STATUS_DONE;
	struct i2c_client* client;
    struct debussy_priv* debussy = file->private_data;
	
    client = i2c_verify_client(debussy->dev);

    dev_info(debussy->dev, "%s -\n", __func__);
	
    debussyCmdBackup.bitmap |= 0x40;
    count = 0;

	igo_i2c_read(client, 0x0a000ff4, &ready_ptn);
	if(DEBUSSY_READY_PATTERN != ready_ptn){
		dev_err(debussy->dev, "%s :[ready_ptn]  invalid : 0x%x \n",__func__,ready_ptn);
        goto factory_check_err;
	}

    mutex_lock(&debussy->igo_ch_lock);
    debussy->reset_chip(debussy->dev, 1);
    mutex_unlock(&debussy->igo_ch_lock);
	status = igo_ch_read(debussy->dev, IGO_CH_FW_SUB_VER_ADDR, &fw_sub_ver);
	if (IGO_CH_STATUS_DONE != status){
		dev_err(debussy->dev, "%s : read sub-version failed ret = %d \n",__func__ ,status);
        goto factory_check_err;
	}
		
    for(i=1;i<SCENARIOS_MAX_CHECK;i++){
		status = _igo_ch_igo_cmd_put(debussy,SCENARIOS_STANDBY_CHECK);
		if (IGO_CH_STATUS_DONE != status){
			dev_err(debussy->dev, "%s : _igo_ch_igo_cmd_put STANDBY failed ret = %d \n",__func__ ,status);
			goto factory_check_err;
		}
		status = _igo_ch_igo_cmd_put(debussy,i);
		if (IGO_CH_STATUS_DONE != status){
			dev_err(debussy->dev, "%s : _igo_ch_igo_cmd_put mode[%d] failed ret = %d \n",__func__ ,i,status);
			goto factory_check_err;
		}
	}
	dev_info(debussy->dev, "%s : Debussy factory check Pass [%x] !!!!!!!!!!!!!!!!!!!!!!!!!!!  \n",__func__, fw_sub_ver);
    count = 1;
    debussy->factory_check = 1;
    
factory_check_err:
    debussyCmdBackup.bitmap &= ~0x40;

    return count;
}

// Usage: cat /d/debussy/factory_check
static ssize_t _debussy_factory_check_read(struct file* file,
    char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    char buf[16];
    int ret;

    ret = snprintf(buf, sizeof(buf) - 1, "%d\n", debussy->factory_check);

    return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}
#endif  /* end of ENABLE_FACTORY_CHECK */

#ifdef DEBUSSY_TYPE_PSRAM
// Usage: echo address len > /d/debussy/psram_dump
static ssize_t _debussy_psram_dump(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;

	unsigned int dump_addr = 0, dump_len = 0;

	size_t position;
	char *input_data, *next_data;

    debussy = file->private_data;
	
    if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {
        dev_err(debussy->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(debussy->dev, input_data);
        return -EFAULT;
    }

    dev_info(debussy->dev, "%s: input_data = %s\n", __func__, input_data);

    position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
    dump_addr = simple_strtoul(&input_data[position], &next_data, 16);      // TODO:// 512 bytes alignment
    position = strcspn(next_data, "1234567890abcdefABCDEF");                // find next number
    dump_len = simple_strtoul(&next_data[position], NULL, 16);
    dev_info(debussy->dev, "%s: addr = 0x%X, len = 0x%X\n", __func__, dump_addr, dump_len);

    mutex_lock(&debussy->igo_ch_lock);
    debussy->reset_chip(debussy->dev, 0);
    mutex_unlock(&debussy->igo_ch_lock);
    //debussy->mcu_hold(debussy->dev, 1);  // halt
    
	debussy_psram_readpage(debussy->dev, dump_addr, dump_len);

    //debussy->mcu_hold(debussy->dev, 0);  // run

	devm_kfree(debussy->dev, input_data);
		
    return count;
}
#endif  /* end of DEBUSSY_TYPE_PSRAM */

// Usage: echo address, data > /d/debussy/hif_cali
static ssize_t _debussy_hif_calibration(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    uint32_t crc;

    mutex_lock(&debussy->igo_ch_lock);

    if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_CRC_CHECK_ADDR, &crc)) {
        // Calibration
        dev_info(debussy->dev, "CHIP FW VER: %08X\n", crc);
        _debussy_hif_qck_cali(debussy);
    }
    else {
        dev_err(debussy->dev, "CRC check fail!!\n");
    }

    mutex_unlock(&debussy->igo_ch_lock);

    return count;
}

static loff_t _debussy_reg_seek(struct file *file,
    loff_t p, int orig)
{
    struct debussy_priv* debussy = file->private_data;

    debussy->reg_address = p & 0xFFFFFFFC;
    dev_info(debussy->dev, "%s - 0x%X\n", __func__, debussy->reg_address);

    return p;
}

static ssize_t _debussy_reg_read(struct file* file,
    char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    struct i2c_client* client;
    char *input_data;

    client = i2c_verify_client(debussy->dev);
    dev_info(debussy->dev, "%s -\n", __func__);
    count &= 0xFFFFFFFC;

    if (count) {
        if ((input_data = devm_kzalloc(debussy->dev, count, GFP_KERNEL)) == NULL) {  
            dev_err(debussy->dev, "%s: alloc fail\n", __func__);
            return -EFAULT;
        }

        memset(input_data, 0, count);

        mutex_lock(&debussy->igo_ch_lock);
        igo_i2c_read_buffer(client, debussy->reg_address, (unsigned int *) input_data, count >> 4);
        mutex_unlock(&debussy->igo_ch_lock);

        if (copy_to_user(user_buf, input_data, count)) {
            count = -EFAULT;
        }

        devm_kfree(debussy->dev, input_data);
    }

    return count;
}

static ssize_t _debussy_reg_write(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    struct i2c_client* client;
    unsigned int *input_data;

    client = i2c_verify_client(debussy->dev);
    dev_info(debussy->dev, "%s -\n", __func__);
    count &= 0xFFFFFFFC;

    if (count) {
        if ((input_data = devm_kzalloc(debussy->dev, count, GFP_KERNEL)) == NULL) {  
            dev_err(debussy->dev, "%s: alloc fail\n", __func__);
            return -EFAULT;
        }

        if (copy_from_user((char *) input_data, user_buf, count)) {
            count = -EFAULT;
        }
        else {
            mutex_lock(&debussy->igo_ch_lock);
            igo_i2c_write_buffer(client, debussy->reg_address, input_data, count >> 2);
            debussy->reg_address += count;
            mutex_unlock(&debussy->igo_ch_lock);
        }

        devm_kfree(debussy->dev, input_data);
    }

    return count;
}

#if (SEND_VAD_CMD_IN_DRIVER==1)
#define VAD_SWITCH_ON   1
#define VAD_SWITCH_OFF  0
static ssize_t _debussy_vad_mode_switch(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
	char *input_data;
    unsigned int mInput = 0 ;
    unsigned int position = 0 ; 
    dev_info(debussy->dev, "%s(+)\n", __func__);
    
    if(count > 0){
        if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {  
            dev_err(debussy->dev, "%s: alloc fail\n", __func__);
            return -EFAULT;
        }

        memset(input_data, 0, count + 1);
        if (copy_from_user(input_data, user_buf, count)) {
            devm_kfree(debussy->dev, input_data);
            return -EFAULT;
        }

        dev_info(debussy->dev, "%s: input_data = %s\n", __func__, input_data);

        position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
        mInput = simple_strtoul(&input_data[position], NULL, 16);
        dev_info(debussy->dev, "%s - input_data=%s,count=%d, mInput=%d\n", __func__, input_data, (int)count, mInput);
        devm_kfree(debussy->dev, input_data);
        
        mutex_lock(&debussy->igo_ch_lock);
        if(VAD_SWITCH_ON == mInput){
            atomic_set(&debussy->vad_switch_flag, 1);
        }else if(VAD_SWITCH_OFF == mInput){
            atomic_set(&debussy->vad_switch_flag, 0);
            atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_CNT_DOWN);
        }
        mutex_unlock(&debussy->igo_ch_lock);
        
    
    }
    dev_info(debussy->dev, "%s(-)\n", __func__);
    return count;
}
#endif  /* end of SEND_VAD_CMD_IN_DRIVER */

static ssize_t _debussy_download_firmware(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy;

    debussy = file->private_data;
    dev_info(debussy->dev, "%s\n", __func__);

    queue_work(debussy->debussy_wq, &debussy->fw_work);

    return count;
}
// Usage: echo 1 > d/debussy/download_firmware_status
static ssize_t _debussy_download_firmware_status(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy;	
    
    debussy = file->private_data;
    dev_info(debussy->dev, "%s download_firmware_status=%d\n", __func__, debussy->download_firmware_status);

    return debussy->download_firmware_status;
}

#ifdef ENABLE_GENETLINK     //Franky temp use ENABLE_GENETLINK flag
// Usage: echo cid, value > /d/debussy/igal_put
static ssize_t _debussy_igal_put(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    struct i2c_client* client;
    unsigned int cid, value;
    size_t position;
    char *input_data, *next_data;

    client = i2c_verify_client(debussy->dev);

    if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {  
        dev_err(debussy->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(debussy->dev, input_data);
        return -EFAULT;
    }

    dev_info(debussy->dev, "%s: input_data = %s\n", __func__, input_data);

    position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
    cid = simple_strtoul(&input_data[position], &next_data, 16);
    position = strcspn(next_data, "1234567890abcdefABCDEF");                // find next number
    value = simple_strtoul(&next_data[position], NULL, 16);
    dev_info(debussy->dev, "%s: cid = %d, value = %d (0x%X)\n", __func__, cid, value, value);

#ifdef ENABLE_GENETLINK
    {
        char s[80];
        dev_info(debussy->dev, "Send: debussy_genetlink_multicast\n");
        sprintf(s,"GENL:IGAL_CTRL#%d#%d",cid, value);
        debussy_genetlink_multicast(s, 0);
    }
#endif    

    return count;
}

// Usage: echo cid > /d/debussy/igal_get
static ssize_t _debussy_igal_get(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    struct i2c_client* client;
    unsigned int cid;
    size_t position;
    char *input_data, *next_data;

    client = i2c_verify_client(debussy->dev);

    if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {  
        dev_err(debussy->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(debussy->dev, input_data);
        return -EFAULT;
    }

    dev_info(debussy->dev, "%s: input_data = %s\n", __func__, input_data);

    position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
    cid = simple_strtoul(&input_data[position], &next_data, 16);
    dev_info(debussy->dev, "%s: cid = %d\n", __func__, cid);

#ifdef ENABLE_GENETLINK
    {
        char s[80];
        dev_info(debussy->dev, "Send: debussy_genetlink_multicast\n");
        sprintf(s,"GENL:IGAL_CTRL#%d",cid);
        debussy_genetlink_multicast(s, 0);
    }
#endif    

    return count;
}
#endif  

#ifdef ENABLE_SPI_INTF
// Usage: echo 1 > /d/debussy/spi_en
static ssize_t _debussy_spi_en(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
    char *input_data;
    unsigned int mInput = 0 ;
    unsigned int position = 0 ; 
    dev_info(debussy->dev, "%s\n", __func__);
    
    if(count > 0){
        if ((input_data = devm_kzalloc(debussy->dev, count + 1, GFP_KERNEL)) == NULL) {  
            dev_err(debussy->dev, "%s: alloc fail\n", __func__);
            return -EFAULT;
        }

        memset(input_data, 0, count + 1);
        if (copy_from_user(input_data, user_buf, count)) {
            devm_kfree(debussy->dev, input_data);
            return -EFAULT;
        }

        dev_info(debussy->dev, "%s: input_data = %s\n", __func__, input_data);

        position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
        mInput = simple_strtoul(&input_data[position], NULL, 16);
        dev_info(debussy->dev, "%s - input_data=%s,count=%d, mInput=%d\n", __func__, input_data, (int)count, mInput);
        devm_kfree(debussy->dev, input_data);
        
        mutex_lock(&debussy->igo_ch_lock);
        if(1 == mInput){
            igo_spi_intf_enable(1);
        }else if(0 == mInput){
            igo_spi_intf_enable(0);
        }
        mutex_unlock(&debussy->igo_ch_lock);
        
    
    }
    dev_info(debussy->dev, "%s\n", __func__);
    return count;
}
#endif


static int debussy_hw_params(struct snd_pcm_substream* substream,
    struct snd_pcm_hw_params* params, struct snd_soc_dai* dai)
{
    return 0;
}

static int debussy_set_dai_fmt(struct snd_soc_dai* dai, unsigned int fmt)
{
    return 0;
}

static int debussy_set_dai_sysclk(struct snd_soc_dai* dai,
    int clk_id, unsigned int freq, int dir)
{
    return 0;
}

static int debussy_set_dai_pll(struct snd_soc_dai* dai, int pll_id, int source,
    unsigned int freq_in, unsigned int freq_out)
{
    return 0;
}

static void debussy_shutdown(struct snd_pcm_substream* stream,
    struct snd_soc_dai* dai)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(dai->codec->dev));

#if (SEND_VAD_CMD_IN_DRIVER==1)
    if (atomic_read(&debussy->vad_switch_flag) == 1)
    {
        dev_info(debussy->dev, "%s: skip shutdown when VAD enable\n", __func__);
    }
    else
#endif
    {
#if (XP_ENV == 1)
    //Franky - 20190719
    //igo_ch_write(debussy->dev, IGO_CH_POWER_MODE_ADDR, POWER_MODE_STANDBY);
    dev_info(debussy->dev, "%s: Start to mask iGo CMD (skip)\n", __func__);
#else
        if (debussy->download_firmware_status == 0)
        {
            dev_info(debussy->dev, "%s: debussy not ready yet, ignore it\n", __func__);
            return;
        }
        pr_info("%s\n", __func__);
        
        mutex_lock(&debussy->igo_ch_lock);
        igo_ch_write(debussy->dev, IGO_CH_POWER_MODE_ADDR, POWER_MODE_STANDBY);
        dev_info(debussy->dev, "%s: Start to mask iGo CMD\n", __func__);
        mutex_unlock(&debussy->igo_ch_lock);
#endif
    }

}

static const struct snd_soc_dai_ops debussy_aif_dai_ops = {
    .hw_params = debussy_hw_params,
    .set_fmt = debussy_set_dai_fmt,
    .set_sysclk = debussy_set_dai_sysclk,
    .set_pll = debussy_set_dai_pll,
    .shutdown = debussy_shutdown,
};

static struct snd_soc_dai_driver debussy_dai[] = {
    {
        .name = "debussy-aif",
        .id = DEBUSSY_AIF,
        .playback = {
            .stream_name = "AIF Playback",
            .channels_min = 1,
            .channels_max = 2,
            .rates = DEBUSSY_RATES,
            .formats = DEBUSSY_FORMATS,
        },
        .capture = {
            .stream_name = "AIF Capture",
            .channels_min = 1,
            .channels_max = 2,
            .rates = DEBUSSY_RATES,
            .formats = DEBUSSY_FORMATS,
        },
        .ops = &debussy_aif_dai_ops,
    },
};
EXPORT_SYMBOL_GPL(debussy_dai);
///////////////////////////////////////////////////////////////////////
static int _debussy_hif_qck_cali(struct debussy_priv* debussy)
{
    struct i2c_client* client;
    int readStatus;
    unsigned int config, nTimeTest;
    unsigned int tar_start, tar_stop, tar_diff, tar_freq;
    struct timeval tvBegin, tvNow;
    //unsigned int data;
    struct timeval ts1, ts2;
    int ret_status = 1;
    if (!debussy) {
        //dev_err(debussy->dev, "%s: debussy is NULL\n", __func__);
        return 1;
    }

    do_gettimeofday(&ts1);

	dev_info(debussy->dev, "%s: HIF calibration mode start.\n", __func__);

    client = i2c_verify_client(debussy->dev);

    // Initial 0x2a012088 to not ready
    igo_i2c_write(client, 0x2a012088, 1);

    if (IGO_CH_STATUS_DONE != igo_ch_write(debussy->dev, IGO_CH_HIF_CALI_EN_ADDR, HIF_CALI_EN_QCK_EN)) {
        igo_ch_write(debussy->dev, IGO_CH_POWER_MODE_ADDR, POWER_MODE_STANDBY);
        debussy->reset_chip(debussy->dev, 1);
        dev_err(debussy->dev, "%s: Unable to enable HIF calibration mode\n", __func__);

        return 1;
    }

    readStatus = 30;
    while (--readStatus) {
        config = 1;
        igo_i2c_read(client, 0x2A012088, &config);
        if (0 == config) {
            break;
        }
        msleep(50);
    }
    if (readStatus == 0) {
        dev_info(debussy->dev, "%s: HIF calibration sync timeout. val = %d\n", __func__, config);
        igo_ch_write(debussy->dev, IGO_CH_POWER_MODE_ADDR, POWER_MODE_STANDBY);
        debussy->reset_chip(debussy->dev, 1);
        return 1;
    }

    do_gettimeofday(&tvBegin);
    igo_i2c_write(client, 0x2A012098, 0);
    igo_i2c_read(client, 0x2A000144, &tar_start);
    igo_i2c_write(client, 0x2A012088, 2);
    msleep(100);
    do_gettimeofday(&tvNow);
    igo_i2c_write(client, 0x2A012098, 0);
    igo_i2c_read(client, 0x2A000144, &tar_stop);
    dev_info(debussy->dev, "%s: Dev cnt = %d - %d\n", __func__, tar_start, tar_stop);

    nTimeTest = (tvNow.tv_sec - tvBegin.tv_sec) * 1000000 + tvNow.tv_usec - tvBegin.tv_usec;
    igo_i2c_write(client, 0x2A012088, nTimeTest);
    dev_info(debussy->dev, "%s: nTimeTest = %d\n", __func__, nTimeTest);

    /* For debug : check target sysclk */
    if (tar_stop > tar_start)
        tar_diff = tar_stop - tar_start;
    else
        tar_diff = 0xFFFFFFFF - tar_start + tar_stop + 1;
    tar_freq = (tar_diff*10) / (nTimeTest/100);
    dev_info(debussy->dev, "%s: Tar_freq = %dKHz\n", __func__, tar_freq);

    if ( (nTimeTest > 120000) || (nTimeTest < 80000) ) {
        dev_info(debussy->dev, "%s: Host calibration time fail = %d.\n", __func__, nTimeTest);
        return 1;
    }

    readStatus = 60;
    while (--readStatus) {
        config = 0;
        igo_i2c_read(client, 0x2A012088, &config);

        if (0xFFFFFFFF == config) {
            ret_status = 0;
            dev_info(debussy->dev, "%s: HIF calibration mode done.\n", __func__);
            break;
        }
        ret_status = 1;
        msleep(20);
    }
    if (readStatus == 0) {
        dev_info(debussy->dev, "%s: HIF calibration end timeout.\n", __func__);
        ret_status = 1 ;
    }

    igo_ch_write(debussy->dev, IGO_CH_POWER_MODE_ADDR, POWER_MODE_STANDBY);
    debussy->reset_chip(debussy->dev, 1);

    do_gettimeofday(&ts2);

	debussyInfo.cali_time = ((ts2.tv_sec-ts1.tv_sec)*1000000+ts2.tv_usec-ts1.tv_usec)/1000;
    dev_info(debussy->dev, "%s: HIF calibration mode is complete. (time cost=0.%ds)\n", 
				__func__, debussyInfo.cali_time);

	debussyCmdBackup.active = debussyCmdBackup.prev_act;	
	dev_info(debussy->dev, "%s: active=%d, prev=%d\n", __func__, debussyCmdBackup.active, debussyCmdBackup.prev_act);
	atomic_set(&debussy->reset_stage, RESET_STAGE_CALI_DONE);
	dev_info(debussy->dev, "%s - reset_stage = %d, active = %d", __func__, 
		atomic_read(&debussy->reset_stage), debussyCmdBackup.active);

    if (ret_status == 0){
        _debussy_get_fw_ver(debussy);
		debussy->download_firmware_status = 1;
	}else{
		debussy->download_firmware_status = 0;
	}
    
    return ret_status ;
}

typedef enum
{
    _FW_UPDATE_FW_READY = 0,
    _FW_UPDATE_NEED_CALI,
    _FW_UPDATE_TRY_AGAIN,   
    _FW_UPDATE_FILE_NOT_READY,
    _FW_UPDATE_BUSY,
    _FW_UPDATE_UNINIT
} _FW_UPDATE_RES_e;

static int _debussy_fw_update(struct debussy_priv *debussy, int force_update)
{
    const struct firmware *fw = NULL;
    char fw_name[16]; 
    int ret = 0, crc = 0;
    unsigned int fw_ver = 0, data = 0;
    static char load_fw_flag = 0;   //prevent double entry
    struct i2c_client* client = i2c_verify_client(debussy->dev);
    _FW_UPDATE_RES_e fw_update_res = _FW_UPDATE_UNINIT;
        
    if (load_fw_flag == 1)
        return _FW_UPDATE_BUSY;
    
    igo_i2c_read(client, 0x2A011074, &data);
    dev_info(debussy->dev, "Hold pin status: 0x%08X\n", data);
#if XP_ENV == 1            
    gpio_request(debussyDtsReplace.loadfw_led_gpio, "IGO_LOADFW_LED");
    gpio_direction_output(debussyDtsReplace.loadfw_led_gpio, GPIO_LOW);
    gpio_set_value(debussyDtsReplace.loadfw_led_gpio, GPIO_LOW);
#endif    
	debussyCmdBackup.prev_act = debussyCmdBackup.active;
	debussyCmdBackup.active = SYS_GUARD_DISABLE;
	dev_info(debussy->dev, "%s: active=%d, prev=%d\n", __func__, debussyCmdBackup.active, debussyCmdBackup.prev_act);
	
	atomic_set(&debussy->reset_stage, RESET_STAGE_LOAD_FW);
	dev_info(debussy->dev, "%s - reset_stage = %d", __func__, atomic_read(&debussy->reset_stage));
	
	snprintf(fw_name, sizeof(fw_name), DEBUSSY_FW_NAME);
	dev_info(debussy->dev, "debussy fw name = %s", fw_name);
	
#if 0
	ret = request_firmware(&fw, fw_name, debussy->dev);
	if (ret) {
		dev_err(debussy->dev, "%s: Failed to locate firmware %s errno = %d\n",
				__func__, fw_name, ret);
		return _FW_UPDATE_FILE_NOT_READY;
	}
#else
	if (IS_ERR(debussy->fw)){//prize huarui
		ret = request_firmware(&fw, fw_name, debussy->dev);
		if (ret) {
			dev_err(debussy->dev, "%s: Failed to locate firmware %s errno = %d\n",
					__func__, fw_name, ret);
			return _FW_UPDATE_FILE_NOT_READY;
		}
	}else{
		fw = debussy->fw;
		dev_info(debussy->dev,"%s already get fw\n",__func__);
	}
#endif

    load_fw_flag = 1;

#ifdef ENABLE_CODEC_DAPM_CB
    atomic_set(&debussy->referenceCount, 1);
#endif

	dev_info(debussy->dev, "%s size = %d\n", fw_name, (int)(fw->size)); 
	fw_ver = *(unsigned int *) &fw->data[DEBUSSY_FW_VER_OFFSET];
    dev_info(debussy->dev, "BIN VER: %08X\n", fw_ver);

#ifdef ENABLE_CODEC_DAPM_CB
    if (gpio_is_valid(debussy->reset_gpio)) {
        gpio_direction_output(debussy->reset_gpio, GPIO_HIGH);
        gpio_set_value(debussy->reset_gpio, GPIO_HIGH);
        usleep_range(IGO_RST_RELEASE_INIT_TIME - 1, IGO_RST_RELEASE_INIT_TIME);
    }
#endif

    if (0 == force_update) {
        if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_CRC_CHECK_ADDR, &crc)) {
            ret = igo_ch_read(debussy->dev, IGO_CH_FW_VER_ADDR, &data);
            if (ret != IGO_CH_STATUS_DONE) {
                data = 0;
                dev_err(debussy->dev, "FW VER get fail!!\n");
            }
            else {
                dev_info(debussy->dev, "build-in VER: %08X\n", data);
            }
        }        
        else {
            crc = 0;
            dev_err(debussy->dev, "CRC check fail!!\n");
        }
    }

	dev_info(debussy->dev,"chip fw ver %08X,force_update %d,crc %d\n",data,force_update,crc);
    if ((fw_ver > data) || (force_update == 1) || (crc == 0)) {
        struct i2c_client* client = i2c_verify_client(debussy->dev);
        /*debussy->mcu_hold(debussy->dev, 1);
        debussy->reset_chip(debussy->dev, 0);
        debussy->mcu_hold(debussy->dev, 0);*/
        /* SW hold mcu before fw update */
        igo_i2c_write(client, 0x2A000010, 0x30000000);
        igo_i2c_write(client, 0x2A000018, 1);
        igo_i2c_write(client, 0x2A00003C, 0);
        
        dev_info(debussy->dev, "Update FW to %08x\n", fw_ver);
#ifdef DEBUSSY_TYPE_PSRAM
		ret = debussy_psram_update_firmware(debussy->dev,
									  DEBUSSY_FW_ADDR,
									  fw->data, fw->size);

#else
		ret = debussy_flash_update_firmware(debussy->dev,
									 DEBUSSY_FW_ADDR,
									 (const u8 *) fw->data, fw->size);

#endif

        if (ret == 0) {
            fw_update_res = _FW_UPDATE_NEED_CALI;
        }
        else {
            fw_update_res = _FW_UPDATE_TRY_AGAIN;
            crc = 1;
        }
    }
    else {
        fw_update_res = _FW_UPDATE_FW_READY;
        _debussy_get_fw_ver(debussy);
        dev_info(debussy->dev, "Use chip built-in FW\n");
#if XP_ENV == 1        
        gpio_set_value(debussyDtsReplace.loadfw_led_gpio, GPIO_HIGH); //FRK
        gpio_free(debussyDtsReplace.loadfw_led_gpio);      //FRK
#endif
    }

#ifdef ENABLE_CODEC_DAPM_CB
    atomic_set(&debussy->referenceCount, 0);
#endif

    if (fw)
        release_firmware(fw);
	
	if (debussy->fw){//prize huarui
		debussy->fw = NULL;
	}

    if (!crc) {        
        if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_CRC_CHECK_ADDR, &crc)) {
            dev_info(debussy->dev, "CRC: %s\n", crc ? "Pass":"Fail");
            if (crc) {
                _debussy_get_fw_ver(debussy);
            }
            else {
                fw_update_res = _FW_UPDATE_TRY_AGAIN;
            }
        }
    }

    load_fw_flag = 0;

    return fw_update_res;
}

static void _debussy_manual_load_firmware(struct work_struct* work)
{
    struct debussy_priv* debussy;
    //uint32_t crc = 0;
    _FW_UPDATE_RES_e fw_update_res = _FW_UPDATE_UNINIT;

    debussy = container_of(work, struct debussy_priv, fw_work);

#if (DEBUSSY_ERR_RECOVER_LV == 2)
	mutex_lock(&debussy->igo_ch_lock);
    debussy->chip_pull_up(debussy->dev);  //"EXIT_SLEEP_MODE"
#endif

    fw_update_res = _debussy_fw_update(debussy, 1);

    if (fw_update_res == _FW_UPDATE_NEED_CALI) {
        if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_FW_SUB_VER_ADDR, &debussyInfo.fw_sub_ver)) {
            // Calibration
            dev_info(debussy->dev, "CHIP FW SUB VER: %08X\n", debussyInfo.fw_sub_ver);
            _debussy_hif_qck_cali(debussy);
        }
    }
    //if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_CRC_CHECK_ADDR, &crc)) {
	//	dev_info(debussy->dev, "CRC : %08X\n", crc);
        if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_FW_SUB_VER_ADDR, &debussyInfo.fw_sub_ver)) {
            // Calibration
            dev_info(debussy->dev, "CHIP FW SUB VER: %08X\n", debussyInfo.fw_sub_ver);
            _debussy_hif_qck_cali(debussy);
        }
    //}
#if (DEBUSSY_ERR_RECOVER_LV == 2)
	/*else
	{
		debussyCmdBackup.active = debussyCmdBackup.prev_act;
		dev_info(debussy->dev, "%s: active=%d, prev=%d\n", __func__, debussyCmdBackup.active, debussyCmdBackup.prev_act);
		dev_info(debussy->dev, "%s: active=%d\n", __func__, debussyCmdBackup.active);
	}*/
	debussy->chip_pull_down(debussy->dev);  //"ENTER_SLEEP_MODE"
	mutex_unlock(&debussy->igo_ch_lock);

    debussyCmdBackup.bitmap |= 0x80;
	_debussy_alive_check_proc(debussy);
#endif
    dev_info(debussy->dev, "%s: Done\n", __func__);

	debussyInfo.manu_load_ctr++;
}

static void _debussy_poweron_load_firmware(struct work_struct* work)
{
    struct debussy_priv* debussy = container_of(work, struct debussy_priv, poweron_update_fw_work.work);
    int force_update = 0;
    //unsigned int crc;
    _FW_UPDATE_RES_e fw_update_res = _FW_UPDATE_UNINIT;

    #ifdef DEBUSSY_TYPE_PSRAM
    force_update = 1;
    #endif

    dev_info(debussy->dev, "%s - %d\n", __func__, force_update);
	debussy->chip_pull_up(debussy->dev);  //"EXIT_SLEEP_MODE"
	//dev_info(debussy->dev, "%s:IG exit sleep mode!!!\n", __func__);
	msleep(20);

    fw_update_res = _debussy_fw_update(debussy, force_update);
    if (fw_update_res >= _FW_UPDATE_TRY_AGAIN) {
        if (fw_update_res == _FW_UPDATE_TRY_AGAIN)
            fw_update_res = _debussy_fw_update(debussy, 0);
    }

    if (fw_update_res == _FW_UPDATE_NEED_CALI) {
        mutex_lock(&debussy->igo_ch_lock);
        //if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_CRC_CHECK_ADDR, &crc)) {
        //    dev_info(debussy->dev, "CRC: %08X\n", crc);

            // Calibration
            while (1 == _debussy_hif_qck_cali(debussy)) {
                
                debussyInfo.cali_time ++;
                msleep(10);
                dev_info(debussy->dev, "cali_count =  %d\n", debussyInfo.cali_time);
            }
            fw_update_res = _FW_UPDATE_FW_READY;
        /*}
        else {
            dev_err(debussy->dev, "CRC check fail!!\n");
        }*/
        mutex_unlock(&debussy->igo_ch_lock);
    }

    if (fw_update_res == _FW_UPDATE_FW_READY) {        
        atomic_set(&debussy->reset_stage, RESET_STAGE_CALI_DONE);
        dev_info(debussy->dev, "%s: Done\n", __func__);
    }
    else {
        dev_info(debussy->dev, "%s: FW not ready !!!\n", __func__);
    }
    
	mutex_lock(&debussy->igo_ch_lock);
	debussy->chip_pull_down(debussy->dev);
	//dev_info(debussy->dev, "%s: enter sleep mode !!!!\n", __func__);
	mutex_unlock(&debussy->igo_ch_lock);
}

//prize huarui +
static void _debussy_power_load_firmware_nowait_load(
				const struct firmware *cont, void *context){
	struct debussy_priv* debussy = context;

	dev_info(debussy->dev,"%s+\n",__func__);
	if (!cont || !context) {
		dev_err(debussy->dev,"%s failed to read %s\n", __func__, DEBUSSY_FW_NAME);
		return;
	}
	
	debussy->fw = cont;
	
	INIT_DELAYED_WORK(&debussy->poweron_update_fw_work, _debussy_poweron_load_firmware);
    schedule_delayed_work(&debussy->poweron_update_fw_work, 1);
}
static void _debussy_power_load_firmware_nowait(struct debussy_priv *debussy){
	char fw_name[16] = {0};
	int ret = 0;
	
	snprintf(fw_name, sizeof(fw_name), DEBUSSY_FW_NAME);
	dev_info(debussy->dev, "%s fw name = %s",__func__, fw_name);
	
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				fw_name, debussy->dev, GFP_KERNEL,debussy, 
				_debussy_power_load_firmware_nowait_load);
	dev_info(debussy->dev,"%s ret %d\n",__func__);

}
//prize huarui -

static void _debussy_json_file_parser(struct debussy_priv* debussy, const unsigned char *wholeBuf, long fsize)
{
	int idx = 0, buf_idx = 0, msg_len = 0, input = 0;
	char buf[1024], tmp_buf[12];
	//FILE *pJson = NULL;
	long fidx = 0;
	char* p = NULL;

	dev_info(debussy->dev, "[FW_LOG][debussy] fsize=%ld\n%s\n", fsize, (char *)wholeBuf);

	while (fidx <= fsize)
	{
		buf_idx = 0;
		/* read line */
		while (wholeBuf[fidx] != 0x0a)
		{
			buf[buf_idx++] = wholeBuf[fidx++];
			if (fidx == fsize)
				break;
		}
		buf[buf_idx++] = '\0'; // eat the newline fgets() stores
		fidx++;

		if ((p = strstr(buf, "\"base\"")) != NULL)
		{
			msg_len = strlen(p) - 10;
			memcpy(tmp_buf, (char *)(p + 9), msg_len);
			tmp_buf[msg_len] = 0;
			sscanf(tmp_buf, "%x", &debussyDbgJson.base);

			dev_info(debussy->dev, "[FW_LOG][debussy] base=0x%x\n", debussyDbgJson.base);
		}
		else if ((p = strstr(buf, "\"size\"")) != NULL)
		{
			msg_len = strlen(p) - 10;
			memcpy(tmp_buf, (char *)(p + 9), msg_len);
			tmp_buf[msg_len] = 0;
			sscanf(tmp_buf, "%x", &input);
			debussyDbgJson.size = input&0xFFFF;
			dev_info(debussy->dev, "[FW_LOG][debussy] size=0x%x\n", debussyDbgJson.size);
		}
		else if ((p = strstr(buf, "\"vbf_id\"")) != NULL)
		{
			msg_len = strlen(p) - 11;
			memcpy(tmp_buf, (char *)(p + 10), msg_len);
			tmp_buf[msg_len] = 0;
			sscanf(tmp_buf, "%d", &input);
			debussyDbgJson.vbf_id = input &0xFFFF;
			dev_info(debussy->dev, "[FW_LOG][debussy] vbf_id=%d\n", debussyDbgJson.vbf_id);
		}
		else if ((p = strstr(buf, "\"msg_cnt\"")) != NULL)
		{
			msg_len = strlen(p) - 12;
			memcpy(tmp_buf, (char *)(p + 11), msg_len);
			tmp_buf[msg_len] = 0;
			sscanf(tmp_buf, "%d", &input);
			debussyDbgJson.msg_cnt = input&0xFFFF;
			dev_info(debussy->dev, "[FW_LOG][debussy] msg_cnt=%d\n", debussyDbgJson.msg_cnt);
		}
		else if ((p = strstr(buf, "msg_tbl")) != NULL)
		{
			continue;
		}
		else if ((p = strstr(buf, "\"id\"")) != NULL)
		{
			msg_len = strlen(p) - 7;
			memcpy(tmp_buf, (char *)(p + 6), msg_len);
			tmp_buf[msg_len] = 0;
			sscanf(tmp_buf, "%d", &idx);
			//printf("id=%ld\n", idx);
		}
		else if ((p = strstr(buf, "msg")) != NULL)
		{
			msg_len = strlen(p) - 9;
			memcpy(debussyDbgJson.msg_tbl[idx].msg, (char *)(p + 7), msg_len);
			debussyDbgJson.msg_tbl[idx].msg[msg_len] = 0;
			//printf("msg=%s\n", debussyDbgJson.msg_tbl[idx].msg);
		}
		else if ((p = strstr(buf, "\"param_num\"")) != NULL)
		{
			msg_len = strlen(p) - 13;
			memcpy(tmp_buf, (char *)(p + 13), msg_len);
			tmp_buf[msg_len] = 0;
			sscanf(tmp_buf, "%d", &input);
            debussyDbgJson.msg_tbl[idx].param_num = input&0xFFFF;
			//printf("param_num=%ld\n", debussyDbgJson.msg_tbl[idx].param_num);
		}
	}

}

static int _debussy_fw_log_print(struct debussy_priv* debussy, unsigned int log_ts, const char *format, ...)
{
	char vspBuffer[320];

	va_list aptr;
	int ret;

	va_start(aptr, format);
	ret = vsprintf(vspBuffer, format, aptr);
	va_end(aptr);

	dev_info(debussy->dev, "[FW_LOG][debussy] %10d : %s\n", log_ts, vspBuffer);
	return(ret);
}

static int _debussy_fw_log_raw_push(struct debussy_priv* debussy, unsigned int raw)
{
	static unsigned char push_idx = 0, param_cnt = 0;
	static unsigned short log_id = 0;
	static unsigned int log_ts = 0;
	static unsigned int params[DEBUSSY_FW_LOG_PARAM_MAX];

	if (push_idx == 0)		// first, include ts + id 
	{
		log_id = (raw & 0x03FF);
		log_ts = ((raw & 0xFFFFFC00) >> 10) * 8;
		//log_ts = ((raw & 0xFFFFFC00) >> 10);

		if (log_id < debussyDbgJson.msg_cnt)
		{
			param_cnt = debussyDbgJson.msg_tbl[log_id].param_num;
			//dev_info(debussy->dev, "%s - raw=0x%08x, <log_id=%d, param_num=%d>", __func__, raw, log_id, debussyDbgJson.msg_tbl[log_id].param_num);
		}
		else
		{
			dev_info(debussy->dev, "[FW_LOG][debussy] invalid log_id %d, exceed max %d", log_id, debussyDbgJson.msg_cnt);
			return -1;
		}
	}
	else // param
	{
		//dev_info(debussy->dev, "%s - params[%d]=0x%x\n", __func__, push_idx, raw);
		params[push_idx-1] = raw;
	}

	/* get complete log info, dump it */
	if (push_idx == param_cnt)
	{
		push_idx = 0;
		switch (param_cnt)
		{
		case 0:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg);
			break;
		case 1:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0]);
			break;
		case 2:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1]);
			break;
		case 3:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2]);
			break;
		case 4:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3]);
			break;
		case 5:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4]);
			break;
		case 6:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4], params[5]);
			break;
		case 7:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4], params[5], params[6]);
			break;
		case 8:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]);
			break;
		case 9:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8]);
			break;
		case 10:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]);
			break;
		case 11:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10]);
			break;
		case 12:
			_debussy_fw_log_print(debussy, log_ts, debussyDbgJson.msg_tbl[log_id].msg, params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10], params[11]);
			break;
		default:
			dev_info(debussy->dev, "[FW_LOG][debussy] param_cnt=%d, exceed max %d!!", param_cnt, DEBUSSY_FW_LOG_PARAM_MAX);
		}
		return 0;
	}
	else
	{
		push_idx++;
	}

	return push_idx;
}


static void _debussy_fw_log_dump(struct work_struct* work)
{
    const struct firmware *dbg_json = NULL;
    struct debussy_priv* debussy;
    struct i2c_client* client;
    unsigned int reg_base = debussyDbgJson.base, reg_offset = 0, prev_offset = 0, log_data = 0;
    unsigned int vbf_base = 0x0a0dff80, vbid = debussyDbgJson.vbf_id, log_addr = 0, rpt = 0, rpt_val = 0;
	unsigned int parsing_cnt = 0;
    int ret = 0, idx = 0;

    debussy = container_of(work, struct debussy_priv, fw_log_work);
    client = i2c_verify_client(debussy->dev);
    rpt = vbf_base + vbid * 4;

    //dev_info(debussy->dev, "%s - reg_base = 0x%x, rpt = 0x%x\n", __func__, reg_base, rpt);

    ret = request_firmware(&dbg_json, DEBUSSY_FW_DBG_TBL, debussy->dev);
    if (ret) {
        dev_err(debussy->dev, "%s: Failed to locate firmware %s errno = %d\n",
                __func__, DEBUSSY_FW_DBG_TBL, ret);
  	}
	else
	{
		//dev_info(debussy->dev, "%s - %s", __func__, fw->data);
		_debussy_json_file_parser(debussy, dbg_json->data, strlen(dbg_json->data));
	}

    if (dbg_json)
        release_firmware(dbg_json);

    while (debussyFwDumping)
    {
        mutex_lock(&debussy->igo_ch_lock);
        igo_i2c_read(client, rpt, &rpt_val);
        mutex_unlock(&debussy->igo_ch_lock);

		rpt_val >>= 16;
        if (rpt_val != prev_offset)
        {
            /*dev_info(debussy->dev, "%s: rpt_val = 0x%X, prev_offset = 0x%X\n", 
                __func__, rpt_val, prev_offset);*/

        	if ((rpt_val - prev_offset) <= debussyDbgJson.size)
    		{
    			parsing_cnt = (rpt_val - prev_offset) / 4;
            	reg_offset = (prev_offset%256);
	            for (idx=0; parsing_cnt>0; parsing_cnt--, idx++, reg_offset += 4)
	            {
	                log_addr = reg_base+(reg_offset%256);
	                mutex_lock(&debussy->igo_ch_lock);
	                igo_i2c_read(client, log_addr, &log_data);
	                mutex_unlock(&debussy->igo_ch_lock);

                /*dev_info(debussy->dev, "%s: 0x%08X = 0x%08X, idx=%d, reg_offset=0x%08x, prev_offset=0x%08x, parsing_cnt=%d", 
					__func__, log_addr, log_data, idx, reg_offset, prev_offset, parsing_cnt);

					if (idx == 0)
					{
						dev_info(debussy->dev, "%s: ID=%d", __func__,  log_data&0x3FF);
					}
					else
					{
						dev_info(debussy->dev, "%s: data=0x%x", __func__,  log_data);
					}*/

					if (_debussy_fw_log_raw_push(debussy, log_data) <= 0)
					{
						idx = 0;
					}					
	            }
            }

            prev_offset = rpt_val;
        }
        msleep(debussyFwDumpDelay);
		
    }
}

#ifdef ENABLE_SPI_INTF
extern ssize_t debussy_spidrv_buffer_read(uint32_t address, uint32_t *data, size_t word_len);
// #define VOICE_REC_READ_BUF_SIZE     128
// #define VOICE_REC_WRITE_BUF_SIZE    0x400
static void _debussy_voice_recording(struct work_struct* work)
{
    struct debussy_priv* debussy;
    struct i2c_client* client;
    unsigned int vbf_base = (0x2a0dff80 + debussyVoiceRecVbId * 4);
    //unsigned int read_addr = 0x2a0c29c0, read_offset = 0;
    unsigned int read_addr = 0, read_offset = 0;
    unsigned int parsing_cnt = 0, tmp_val = 0, sn = 0, parsing_cnt_part1 = 0;
    unsigned int rpt = 0, wpt = 0;//, read_low = 0, read_high = 0;
    int j=0;
    unsigned int rg_addr = 0, cfg = 0, base = 0, buf_size = 0;
    unsigned int VOICE_REC_WRITE_BUF_SIZE = 0;
    unsigned char is_first = 1;
    unsigned int read_buf[256];

    debussy = container_of(work, struct debussy_priv, voice_rec_work);
    client = i2c_verify_client(debussy->dev);

    rg_addr = 0x2a0dff40 + (debussyVoiceRecVbId / 2) * 4;
    mutex_lock(&debussy->igo_ch_lock);
    igo_i2c_read(client, rg_addr, &tmp_val);
    mutex_unlock(&debussy->igo_ch_lock);
    cfg = (tmp_val >> ((debussyVoiceRecVbId % 2) * 16)) & 0xFFFF;
    base = cfg & 0xFF;
    //buf_size = (cfg >> 8) & 0xF;
    buf_size = 1 << (6 + ((cfg >> 8) & 0xF));
    read_addr = 0x2a0c0000 + (base << 6);
    VOICE_REC_WRITE_BUF_SIZE = buf_size;

    dev_info(debussy->dev, "%s - vbid = 0x%x, cfg = 0x%x, rpt = 0x%x, VOICE_REC_WRITE_BUF_SIZE=0x%x\n", __func__,
                debussyVoiceRecVbId, cfg, rpt,
                VOICE_REC_WRITE_BUF_SIZE);

    while (debussyVoiceRecording)
    {
        mutex_lock(&debussy->igo_ch_lock);
        igo_i2c_read(client, vbf_base, &tmp_val);
        mutex_unlock(&debussy->igo_ch_lock);
        wpt = (tmp_val >> 16);

        if (is_first) {
            is_first = 0;
            rpt = wpt - ((wpt - rpt) % 4);
            continue;
        }

        if (wpt != rpt) {

            if (wpt > rpt) {
                parsing_cnt = (wpt - rpt) / 4;
            } else {
                parsing_cnt = (wpt + 65535 - rpt) / 4;
            }

            parsing_cnt = (parsing_cnt / 4) * 4;

            dev_info(debussy->dev, "%s: vbf_base=0x%x, tmp_val=0x%x, wpt=0x%08x(%d), rpt=0x%08x(%d), cnt=%d",
                            __func__, vbf_base, tmp_val,
                            wpt, (wpt%VOICE_REC_WRITE_BUF_SIZE),
                            rpt, (rpt%VOICE_REC_WRITE_BUF_SIZE),
                            parsing_cnt);

            if (parsing_cnt * 4 > VOICE_REC_WRITE_BUF_SIZE) {
                dev_info(debussy->dev, "VB overflow");
                rpt = wpt;
                continue;
            }

            if (parsing_cnt > 0) {
                read_offset = rpt % VOICE_REC_WRITE_BUF_SIZE;
                mutex_lock(&debussy->igo_ch_lock);

                if ((read_offset + parsing_cnt * 4) <= VOICE_REC_WRITE_BUF_SIZE) {
                    debussy_spidrv_buffer_read(read_addr + read_offset, read_buf, parsing_cnt);
                } else {
                    parsing_cnt_part1 = (VOICE_REC_WRITE_BUF_SIZE - read_offset) / 4;
                    debussy_spidrv_buffer_read(read_addr + read_offset, read_buf, parsing_cnt_part1);
                    debussy_spidrv_buffer_read(read_addr, &read_buf[parsing_cnt_part1], parsing_cnt - parsing_cnt_part1);
                }

                mutex_unlock(&debussy->igo_ch_lock);

                for (j = 0; j < parsing_cnt; j += 4)
                {
                #if 1
                    dev_info(debussy->dev, "[VoiceRec 0x%08x][StartAddr 0x%08x] 0x%08x 0x%08x 0x%08x 0x%08x",
                         sn, (read_addr + read_offset + j * 4), read_buf[j], read_buf[j + 1], read_buf[j + 2], read_buf[j + 3]);
                #else
                    read_low  = (read_buf[j]&0xFFFF);
                    read_high = (read_buf[j]>>16);
                    dev_info(debussy->dev, "%s: 0x%08X = 0x%08x,low [sn=0x%08x][VoiceRec] %d",
                        __func__, (read_addr+read_offset+j*4), read_buf[j], sn, read_low);
                    dev_info(debussy->dev, "%s: 0x%08X = 0x%08x,high [sn=0x%08x][VoiceRec] %d",
                        __func__, (read_addr+read_offset+j*4), read_buf[j], sn, read_high);
                #endif
                    sn += 4;
                }

                rpt = (rpt + parsing_cnt * 4) % 65535;
            }
        }
        // msleep(debussyVoiceRecordDelay);

    }
    queue_work(debussy->debussy_pull_down_delay_wq, &debussy->pull_down_delay_work);
}
#endif  /* end of ENABLE_SPI_INTF */

#if (DEBUSSY_ERR_RECOVER_LV == 2)
void _debussy_alive_check_proc(struct debussy_priv* debussy)
{
    struct i2c_client* client = i2c_verify_client(debussy->dev);
    unsigned int data = 0, prev_data = 0;
	unsigned char miss_cnt = 0;
	static char alive_checking = 0;
	int status = IGO_CH_STATUS_DONE, i = 0;

	if (alive_checking)
		return;

	if ((debussyCmdBackup.bitmap&0x07) != 0x07)
		return;
	
	alive_checking = 1;

    mutex_lock(&debussy->igo_ch_lock);
    igo_i2c_read(client, 0x2a000200, &data);
    prev_data = data;
    for (i=0; i<6; i++)
    {
        igo_i2c_read(client, 0x2a000200, &data);
        
        if (prev_data == data)
        {
        	dev_info(debussy->dev, "%s - data=0x%x, prev_data=0x%x\n", __func__, data, prev_data);
            miss_cnt++;
			msleep(10);
        }
		prev_data = data;
    }
    mutex_unlock(&debussy->igo_ch_lock);

    if (miss_cnt > 3)
    {
        dev_info(debussy->dev, "%s - data=0x%x, prev_data=0x%x, timeout !!\n", __func__, data, prev_data);
        debussyCmdBackup.bitmap |= 0x80;
    }  

    if (debussyCmdBackup.bitmap == 0x87)
    {
        dev_info(debussy->dev, "%s - re-send iGo commands !!\n", __func__);
        debussyCmdBackup.bitmap |= 0x08;
		debussyInfo.resend_ctr++;

        mutex_lock(&debussy->igo_ch_lock);
        atomic_set(&debussy->reset_stage, RESET_STAGE_EXEC_RESET);
        dev_info(debussy->dev, "%s - reset_stage = %d, active = %d", __func__, 
            atomic_read(&debussy->reset_stage), debussyCmdBackup.active);

        debussy->reset_chip(debussy->dev, 1);
        for (i=0; i<debussyCmdBackup.in_idx[debussyCmdBackup.re_send_idx]; i++)
        {
            dev_info(debussy->dev, "%s - re-send backup[%d] cmd=0x%x, val=0x%x !!\n", __func__, debussyCmdBackup.re_send_idx,
                        debussyCmdBackup.backup[debussyCmdBackup.re_send_idx][i].cmd, debussyCmdBackup.backup[debussyCmdBackup.re_send_idx][i].val);
            status = igo_ch_write_wait(debussy->dev, debussyCmdBackup.backup[debussyCmdBackup.re_send_idx][i].cmd, debussyCmdBackup.backup[debussyCmdBackup.re_send_idx][i].val, 0);
			if (status != IGO_CH_STATUS_DONE)
			{
				int data = 0;

				igo_ch_read(debussy->dev, IGO_CH_FW_SUB_VER_ADDR, &data);
				if (data != debussyInfo.fw_sub_ver)
					debussyCmdBackup.bitmap |= 0x10;
			
				dev_info(debussy->dev, "%s - re-send cmd not done!! break!!", __func__);
				atomic_set(&debussy->reset_stage, RESET_STAGE_EXEC_RESET);
				dev_info(debussy->dev, "%s - reset_stage = %d, active = %d", __func__, 
					RESET_STAGE_EXEC_RESET, debussyCmdBackup.active);

				break;
			}
		}
        atomic_set(&debussy->reset_stage, RESET_STAGE_EXEC_RESET);
        mutex_unlock(&debussy->igo_ch_lock);
        dev_info(debussy->dev, "%s - switch to working mode, reset_stage = %d", __func__, atomic_read(&debussy->reset_stage));
        
        
        debussyCmdBackup.bitmap &= ~0x08;
		if (i == debussyCmdBackup.in_idx[debussyCmdBackup.re_send_idx])
        	debussyCmdBackup.bitmap &= ~0x80;
			

#ifdef DEBUSSY_TYPE_PSRAM
		if ((debussyCmdBackup.bitmap&0x10) == 0x10)
		{
            dev_info(debussy->dev, "%s - re-send cmd timeout!! reload FW !!\n", __func__);
            debussyInfo.reload_ctr++;    
            _debussy_manual_load_firmware(&debussy->fw_work);
			debussyCmdBackup.bitmap &= ~0x10;
		}
#endif  /* end of DEBUSSY_TYPE_PSRAM */        
    }	

	alive_checking = 0;
}

// Usage: echo 1 > /d/debussy/alive_check
static ssize_t _debussy_alive_check(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_priv* debussy = file->private_data;
	unsigned char input[8] = {0};
	int input_val = 0;
	static int hb_ctr = 0 ;

	if (debussyCmdBackup.active == SYS_GUARD_ALIVE_HB)
	{
		if ((hb_ctr++ % 10) == 0)
		{
			dev_info(debussy->dev, "%s - bitmap=0x%02x\n", __func__, debussyCmdBackup.bitmap);
		}
	}

	if (copy_from_user(&input, user_buf, count)) {
        return -EFAULT;
    }

	input[count] = 0;
	sscanf(input, "%d", &input_val);

	if (input_val == 1)					//slient active
	{
		if (debussyCmdBackup.active != SYS_GUARD_DISABLE)
		{
		    _debussy_alive_check_proc(debussy);
		}

		if (debussyCmdBackup.active == SYS_GUARD_ENABLE)
		{
			debussyCmdBackup.active = SYS_GUARD_ALIVE;
		}
	}
	else if (input_val == 0)			//disable
	{
		debussyCmdBackup.active = SYS_GUARD_DISABLE;
	}
	else if (input_val == 2)			//enable
	{
		debussyCmdBackup.active = SYS_GUARD_ENABLE;
	}
	else								//enable with HB
	{
		debussyCmdBackup.active = SYS_GUARD_ALIVE_HB;
	}

   return count;

}
#endif  /* end of DEBUSSY_ERR_RECOVER_LV */

static int _debussy_codec_probe(struct snd_soc_codec* codec)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(codec->dev));
    struct i2c_client* client;
    unsigned char cmid[] = IGO_DRIVER_CMID;
    _FW_UPDATE_RES_e fw_update_res;

    memset(&debussyInfo, 0, sizeof (DEBUSSY_INFO_t));

    client = i2c_verify_client(debussy->dev);
    igo_i2c_read(client, 0x0a000ff4, &debussyInfo.ready_ptn);
    dev_info(codec->dev, "%s, debussy_ready=0x%08x, request_fw_delaytime = %d \n", __func__, debussyInfo.ready_ptn, debussy->request_fw_delaytime);
    debussy->codec = codec;

    if (debussy->request_fw_delaytime) {
		_debussy_power_load_firmware_nowait(debussy);//prize huarui
        //INIT_DELAYED_WORK(&debussy->poweron_update_fw_work, _debussy_poweron_load_firmware);
        //schedule_delayed_work(&debussy->poweron_update_fw_work, debussy->request_fw_delaytime * HZ);
    }
    else {
        int force_update = 0;

#ifdef DEBUSSY_TYPE_PSRAM
        force_update = 1;
#endif

        fw_update_res = _debussy_fw_update(debussy, force_update);
        if (fw_update_res >= _FW_UPDATE_TRY_AGAIN) {
            if (fw_update_res == _FW_UPDATE_TRY_AGAIN)
                fw_update_res = _debussy_fw_update(debussy, 0);
        }

        if (fw_update_res == _FW_UPDATE_NEED_CALI) {
            mutex_lock(&debussy->igo_ch_lock);

            if (IGO_CH_STATUS_DONE == igo_ch_read(debussy->dev, IGO_CH_FW_SUB_VER_ADDR, &debussyInfo.fw_sub_ver)) 
            {
                // Calibration
                dev_info(debussy->dev, "CHIP FW SUB VER: %08X\n", debussyInfo.fw_sub_ver);
                if (0 == _debussy_hif_qck_cali(debussy))
                    fw_update_res = _FW_UPDATE_FW_READY;
            }

            mutex_unlock(&debussy->igo_ch_lock);        
        }

        if (fw_update_res == _FW_UPDATE_FW_READY) {        
            atomic_set(&debussy->reset_stage, RESET_STAGE_CALI_DONE);
            dev_info(debussy->dev, "%s: Done\n", __func__);
        }
        else {
            dev_info(debussy->dev, "%s: FW not ready !!!\n", __func__);
        }        
    }

    /////////////////////////////////////////////////////////////////////////
#if DTS_SUPPORT
    if (of_property_read_u32(i2c_verify_client(debussy->dev)->dev.of_node, "ig,enable-kws", &debussy->enable_kws)) {
        dev_err(debussy->dev, "Unable to get \"ig,enable-kws\"\n");
        debussy->enable_kws = 0;
    }
#else
    debussy->enable_kws = debussyDtsReplace.enable_kws;
#endif  /* end of DTS_SUPPORT */
    dev_info(debussy->dev, "ig,enable-kws = %u\n", debussy->enable_kws);

    if (debussy->enable_kws) {
        debussy_kws_init(debussy);
    }

	
	/* 20190108: move sy_add_codec_controls here to prevent _debussy_hif_cali and y_add_codec_controls run simultaneously casue igo cmd access not atomic.
	   Prerequisite: setup request_fw_delaytime as 0 in dts. 
	*/
	debussy_add_codec_controls(codec);
#if DUMP_FW_LOG_WHEN_BOOTUP
    if (1)
#else
    if (0)
#endif
   	{
		debussyFwDumping = 1;
		debussyFwDumpDelay = DEBUSSY_FW_DUMP_DELAY_MIN;
	    queue_work(debussy->debussy_fw_log_wq, &debussy->fw_log_work);
		dev_info(codec->dev, "%s - start FW log dump\n", __func__);
	}
		
	memcpy(debussyInfo.drv_cmid, cmid, 8);
	debussyInfo.drv_cmid[8] = 0;

#if (DEBUSSY_ERR_RECOVER_LV == 2)
    memset(&debussyCmdBackup, 0, sizeof(debussyCmdBackup));
#endif  /* end of DEBUSSY_ERR_RECOVER_LV */
    
	pr_info("%s codec probe suscess.\n", __func__);

	debussyCmdBackup.active = debussyCmdBackup.prev_act = SYS_GUARD_ALIVE;
	dev_info(debussy->dev, "%s: active=%d, prev=%d\n", __func__, debussyCmdBackup.active, debussyCmdBackup.prev_act);

    return 0;
}

static int _debussy_codec_remove(struct snd_soc_codec* codec)
{
    dev_info(codec->dev, "%s\n", __func__);

    return 0;
}

static int _debussy_codec_suspend(struct snd_soc_codec* codec)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(codec->dev));
#ifdef ENABLE_CODEC_DAPM_CB
    if (0 != atomic_read(&debussy->referenceCount))
        return 0;

    dev_info(codec->dev, "%s\n", __func__);

    if (gpio_is_valid(debussy->reset_gpio)) {
        gpio_direction_output(debussy->reset_gpio, GPIO_LOW);
        gpio_set_value(debussy->reset_gpio, GPIO_LOW);
        usleep_range(IGO_RST_RELEASE_INIT_TIME - 1, IGO_RST_RELEASE_INIT_TIME);
    }
    else {
        dev_err(codec->dev, "%s: Reset GPIO-%d is invalid\n", __func__, debussy->reset_gpio);
    }
#endif
    pr_info("***%s  %d kws %u\n", __func__, __LINE__, debussy->enable_kws);
	if(debussy->enable_kws)
        disable_irq(debussy->kws_irq);//enable_irq_wake already set

    return 0;
}

static int _debussy_codec_resume(struct snd_soc_codec* codec)
{
    struct debussy_priv* debussy = i2c_get_clientdata(i2c_verify_client(codec->dev));

#ifdef ENABLE_CODEC_DAPM_CB
    uint32_t tmp_data = 0;

    dev_info(codec->dev, "%s\n", __func__);

    if (gpio_is_valid(debussy->reset_gpio)) {
        gpio_direction_output(debussy->reset_gpio, GPIO_HIGH);
        gpio_set_value(debussy->reset_gpio, GPIO_HIGH);
        usleep_range(IGO_RST_RELEASE_INIT_TIME - 1, IGO_RST_RELEASE_INIT_TIME);

        igo_i2c_write(i2c_verify_client(debussy->dev), 0x2A000018, tmp_data);
        msleep(debussy->reset_release_time);
        igo_spi_intf_enable(1);
    }
    else {
        dev_err(codec->dev, "%s: Reset GPIO-%d is invalid\n", __func__, debussy->reset_gpio);
    }
#endif
    pr_info("***%s  %d kws %u\n", __func__, __LINE__, debussy->enable_kws);
	if(debussy->enable_kws)
        enable_irq(debussy->kws_irq);

    return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_debussy = {
    .probe = _debussy_codec_probe,
    .remove = _debussy_codec_remove,
    .suspend = _debussy_codec_suspend,
    .resume = _debussy_codec_resume,

};
EXPORT_SYMBOL_GPL(soc_codec_dev_debussy);

static int _debussy_debufs_init(struct debussy_priv* debussy)
{
    int ret = 0;
    struct dentry* dir = NULL;

    debussy->reg_address = 0x2A000000;

    dir = debugfs_create_dir("debussy", NULL);
    if (IS_ERR_OR_NULL(dir)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node - %s\n",
            __func__, "debussy");
        dir = NULL;
        ret = -ENODEV;
        goto err_create_dir;
    }

    debussy->dir = dir;

#if (SEND_VAD_CMD_IN_DRIVER==1)
    if (!debugfs_create_file("vad_mode_switch", S_IWUSR,
            dir, debussy, &debussy_vad_mode_switch_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node - %s\n",
            __func__, "vad_mode_switch");
        ret = -ENODEV;
        goto err_create_entry;
    }
#endif 

    if (!debugfs_create_file("download_firmware", S_IWUSR,
            dir, debussy, &debussy_download_firmware_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node - %s\n",
            __func__, "download_firmware");
        ret = -ENODEV;
        goto err_create_entry;
    }

	if (!debugfs_create_file("download_firmware_status", S_IWUSR,
            dir, debussy, &download_firmware_status_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node - %s\n",
            __func__, "download_firmware_status");
        ret = -ENODEV;
        goto err_create_entry;
    }
	
    if (!debugfs_create_file("reset_chip", S_IWUSR,
            dir, debussy, &debussy_reset_chip_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node - %s\n",
            __func__, "reset_chip");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("reset_gpio_pull_down", S_IWUSR,
            dir, debussy, &debussy_reset_gpio_pull_down_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node - %s\n",
            __func__, "reset_gpio_pull_down");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("reset_gpio_pull_up", S_IWUSR,
            dir, debussy, &debussy_reset_gpio_pull_up_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node - %s\n",
            __func__, "reset_gpio_pull_up");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("mcu_hold", S_IWUSR,
            dir, debussy, &debussy_mcu_hold_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "mcu_hold");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("get_fw_version", S_IWUSR,
            dir, debussy, &debussy_get_fw_version_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "get_fw_version");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("debussy_reg", S_IWUSR,
            dir, debussy, &debussy_reg_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "debussy_reg");
        ret = -ENODEV;
        goto err_create_entry;
    }
    
    if (!debugfs_create_file("reg_get", S_IWUSR|S_IWGRP,		 
            dir, debussy, &debussy_reg_get_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "reg_get");
        ret = -ENODEV;
        goto err_create_entry;
    }
    
    if (!debugfs_create_file("reg_put", S_IWUSR|S_IWGRP,		
            dir, debussy, &debussy_reg_put_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "reg_put");
        ret = -ENODEV;
        goto err_create_entry;
    }
	
    if (!debugfs_create_file("fw_log", S_IWUSR,
            dir, debussy, &debussy_fw_log_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "fw_log");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("clk_rate", S_IWUSR,
            dir, debussy, &debussy_clk_rate_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "clk_rate");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("sys_info", S_IWUSR,
            dir, debussy, &debussy_sys_info_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "sys_info");
        ret = -ENODEV;
        goto err_create_entry;
    }

#ifdef ENABLE_FACTORY_CHECK
	if (!debugfs_create_file("factory_check", S_IWUSR,
            dir, debussy, &debussy_factory_check_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "factory_check");
        ret = -ENODEV;
        goto err_create_entry;
    }
#endif

	if (!debugfs_create_file("voice_rec", S_IWUSR,
            dir, debussy, &debussy_voice_rec_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "voice_rec");
        ret = -ENODEV;
        goto err_create_entry;
    }
		
#ifdef DEBUSSY_TYPE_PSRAM
    if (!debugfs_create_file("psram_dump", S_IWUSR,
            dir, debussy, &debussy_psram_dump_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "psram_dump");
        ret = -ENODEV;
        goto err_create_entry;
    }
#endif    

#if (DEBUSSY_ERR_RECOVER_LV == 2)
    if (!debugfs_create_file("alive_check", S_IWUSR,
            dir, debussy, &debussy_alive_check_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "alive_check");
        ret = -ENODEV;
        goto err_create_entry;
    }
#endif  /* end of DEBUSSY_ERR_RECOVER_LV */
 
     if (!debugfs_create_file("hif_cali", S_IWUSR|S_IWGRP,    
            dir, debussy, &debussy_hif_calibration_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "hif_cali");
        ret = -ENODEV;
        goto err_create_entry;
    }

#ifdef ENABLE_GENETLINK
    if (!debugfs_create_file("igal_put", S_IWUSR|S_IWGRP,        
            dir, debussy, &debussy_igal_put_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "igal_put");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("igal_get", S_IWUSR|S_IWGRP,        
            dir, debussy, &debussy_igal_get_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "igal_get");
        ret = -ENODEV;
        goto err_create_entry;
    }
#endif    

#ifdef ENABLE_SPI_INTF
    if (!debugfs_create_file("spi_en", S_IWUSR|S_IWGRP,        
            dir, debussy, &debussy_spi_en_fops)) {
        dev_err(debussy->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "spi_en");
        ret = -ENODEV;
        goto err_create_entry;
    }
#endif    

err_create_dir:
    debugfs_remove(dir);

err_create_entry:
    return ret;
}

static void parser_dts_table(struct debussy_priv* debussy, struct i2c_client* i2c) {

#if DTS_SUPPORT
    struct device_node *node = i2c->dev.of_node;

    /////////////////////////////////////////////////////////////////////////
    {
        char *dev_name = NULL;

        if (0 == of_property_read_string(node, "ig,device_name", (const char **) &dev_name)) {
            dev_info(debussy->dev, "ig,device_name = %s\n", dev_name);
        }
    }

    /////////////////////////////////////////////////////////////////////////
    debussy->mcu_hold_gpio = of_get_named_gpio(node, "ig,mcu-hold-gpio", 0);
    if (debussy->mcu_hold_gpio < 0) {
        dev_err(debussy->dev, "Unable to get \"ig,mcu-hold-gpio\"\n");
    }
    else {
        dev_info(debussy->dev, "debussy->mcu_hold_gpio = %d\n", debussy->mcu_hold_gpio);

        if (gpio_is_valid(debussy->mcu_hold_gpio)) {
            if (0 == gpio_request(debussy->mcu_hold_gpio, "IGO_MCU_HOLD")) {
#ifdef DEBUSSY_TYPE_PSRAM
                gpio_direction_output(debussy->mcu_hold_gpio, GPIO_HIGH);
#else
                gpio_direction_output(debussy->mcu_hold_gpio, GPIO_LOW);
#endif
            }
            else {
                dev_err(debussy->dev, "IGO_MCU_HOLD: gpio_request fail\n");
            }
        }
        else {
            dev_err(debussy->dev, "debussy->mcu_hold_gpio is invalid\n");
        }
    }

    /////////////////////////////////////////////////////////////////////////
    if (of_property_read_u32(node, "ig,reset-hold-time", &debussy->reset_hold_time)) {
        dev_err(debussy->dev, "Unable to get \"ig,reset-hold-time\"\n");
        debussy->reset_hold_time = IGO_RST_HOLD_TIME_MIN;
    }
    else if (debussy->reset_hold_time < IGO_RST_HOLD_TIME_MIN) {
        debussy->reset_hold_time = IGO_RST_HOLD_TIME_MIN;
    }

    dev_info(debussy->dev, "debussy->reset_hold_time = %dms\n", debussy->reset_hold_time);

    if (of_property_read_u32(node, "ig,reset-release-time", &debussy->reset_release_time)) {
        dev_err(debussy->dev, "Unable to get \"ig,reset-release-time\"\n");
        debussy->reset_release_time = IGO_RST_RELEASE_TIME_MIN;
    }
    else if (debussy->reset_release_time < IGO_RST_RELEASE_TIME_MIN) {
        debussy->reset_release_time = IGO_RST_RELEASE_TIME_MIN;
    }

    dev_info(debussy->dev, "debussy->reset_release_time = %dms\n", debussy->reset_release_time);

    /////////////////////////////////////////////////////////////////////////
    debussy->reset_gpio = of_get_named_gpio(node, "ig,reset-gpio", 0);
    if (debussy->reset_gpio < 0) {
        dev_err(debussy->dev, "Unable to get \"ig,reset-gpio\"\n");
        debussy->reset_gpio = IGO_RST_GPIO;
    }
    else {
        dev_info(debussy->dev, "debussy->reset_gpio = %d\n", debussy->reset_gpio);
    }

    if (gpio_is_valid(debussy->reset_gpio)) {
        if (0 == gpio_request(debussy->reset_gpio, "IGO_RESET")) {
            gpio_direction_output(debussy->reset_gpio, GPIO_LOW);
            gpio_set_value(debussy->reset_gpio, GPIO_LOW);

            #ifdef DEBUSSY_TYPE_PSRAM
            debussy->reset_chip(debussy->dev, 0);
            #else
            debussy->reset_chip(debussy->dev, 1);
            #endif
        }
        else {
            dev_err(debussy->dev, "IGO_RESET: gpio_request fail\n");
        }
    }
    else {
        dev_err(debussy->dev, "debussy->reset_gpio is invalid: %d\n", debussy->reset_gpio);
    }

    /////////////////////////////////////////////////////////////////////////
    if (of_property_read_u32(node, "ig,request_fw_delaytime", &debussy->request_fw_delaytime)) {
        dev_err(debussy->dev, "Unable to get \"ig,request_fw_delaytime\"\n");
        debussy->request_fw_delaytime = 0;      // default: Disable delay work method
    }

    /////////////////////////////////////////////////////////////////////////
    if (of_property_read_u32(node, "ig,max_data_length_i2c", &debussy->max_data_length_i2c)) {
        dev_err(debussy->dev, "Unable to get \"ig,max_data_length_i2c\"\n");
        debussy->max_data_length_i2c = 0;
    }
    else {
        dev_info(debussy->dev, "debussy->max_data_length_i2c = %d\n", debussy->max_data_length_i2c);
    }

    if (of_property_read_u32(node, "ig,max_data_length_spi", &debussy->max_data_length_spi)) {
        dev_err(debussy->dev, "Unable to get \"ig,max_data_length_spi\"\n");
        debussy->max_data_length_spi = 0;
    }
    else {
        dev_info(debussy->dev, "debussy->max_data_length_spi = %d\n", debussy->max_data_length_spi);
    }

    debussy->dma_mode_i2c = 0;
    debussy->dma_mode_spi = 0;

    debussy_dts_table_cus(debussy, node);
#else
    dev_info(debussy->dev, "ig,device_name = %s\n", debussyDtsReplace.device_name);

    /* mcu_hold_gpio */
    debussy->mcu_hold_gpio = debussyDtsReplace.mcu_hold_gpio;
    dev_info(debussy->dev, "debussy->mcu_hold_gpio = %d\n", debussy->mcu_hold_gpio);
    {
        if (gpio_is_valid(debussy->mcu_hold_gpio)) {
            if (0 == gpio_request(debussy->mcu_hold_gpio, "IGO_MCU_HOLD")) {
#ifdef DEBUSSY_TYPE_PSRAM
                gpio_direction_output(debussy->mcu_hold_gpio, GPIO_HIGH);
#else
                gpio_direction_output(debussy->mcu_hold_gpio, GPIO_LOW);
#endif
            }
            else {
                dev_err(debussy->dev, "IGO_MCU_HOLD: gpio_request fail\n");
            }
        }
        else {
            dev_err(debussy->dev, "debussy->mcu_hold_gpio is invalid\n");
        }
    }

    /* reset_hold_time */
    if (debussyDtsReplace.reset_hold_time < IGO_RST_HOLD_TIME_MIN) {
        dev_err(debussy->dev, "reset_hold_time < min, change to min value\n");
        debussy->reset_hold_time = IGO_RST_HOLD_TIME_MIN;
    }
    else {
        debussy->reset_hold_time = debussyDtsReplace.reset_hold_time;
    }
    dev_info(debussy->dev, "debussy->reset_hold_time = %dms\n", debussy->reset_hold_time);    

    /* reset_release_time */
    if (debussyDtsReplace.reset_release_time < IGO_RST_RELEASE_TIME_MIN) {
        dev_err(debussy->dev, "reset_release_time < min, change to min value\n");
        debussy->reset_release_time = IGO_RST_RELEASE_TIME_MIN;
    }
    else {
        debussy->reset_release_time = debussyDtsReplace.reset_release_time;
    }
    dev_info(debussy->dev, "debussy->reset_release_time = %dms\n", debussy->reset_release_time);

    /* reset_gpio */
    debussy->reset_gpio = debussyDtsReplace.reset_gpio;
    dev_info(debussy->dev, "debussy->reset_gpio = %d\n", debussy->reset_gpio);
    {
        if (gpio_is_valid(debussy->reset_gpio)) {
            if (0 == gpio_request(debussy->reset_gpio, "IGO_RESET")) {
                gpio_direction_output(debussy->reset_gpio, GPIO_LOW);
                gpio_set_value(debussy->reset_gpio, GPIO_LOW);

                #ifdef DEBUSSY_TYPE_PSRAM
                debussy->reset_chip(debussy->dev, 0);
                #else
                debussy->reset_chip(debussy->dev, 1);
                #endif
            }
            else {
                dev_err(debussy->dev, "IGO_RESET: gpio_request fail\n");
            }
        }
        else {
            dev_err(debussy->dev, "debussy->reset_gpio is invalid: %d\n", debussy->reset_gpio);
        }
    }

    /* request_fw_delaytime */
    debussy->request_fw_delaytime = debussyDtsReplace.request_fw_delaytime;
    dev_info(debussy->dev, "debussy->request_fw_delaytime = %d\n", debussy->request_fw_delaytime);

    /* max_data_length_i2c */
    debussy->max_data_length_i2c = debussyDtsReplace.max_data_length_i2c;
    dev_info(debussy->dev, "debussy->max_data_length_i2c = %d\n", debussy->max_data_length_i2c);

    /* max_data_length_spi */
    debussy->max_data_length_spi = debussyDtsReplace.max_data_length_spi;
    dev_info(debussy->dev, "debussy->max_data_length_spi = %d\n", debussy->max_data_length_spi);

    debussy->dma_mode_i2c = 0;
    debussy->dma_mode_spi = 0;

    dev_set_name(debussy->dev, "%s", "debussy.5d");

#endif  /* end of DTS_SUPPORT */    
}

static int igo_i2c_probe(struct i2c_client *i2c,
    const struct i2c_device_id *id)
{
    struct debussy_priv *debussy;
    int ret;

    pr_info("debussy: %s - cmid: %s\n", __func__, IGO_DRIVER_CMID);
    dev_info(&i2c->dev, "Linux Version = %d.%d.%d\n",
        (LINUX_VERSION_CODE >> 16) & 0xFF,
        (LINUX_VERSION_CODE >> 8) & 0xFF,
        (LINUX_VERSION_CODE) & 0xFF);

    debussy = devm_kzalloc(&i2c->dev, sizeof(struct debussy_priv), GFP_KERNEL);
    if (!debussy) {
        dev_err(&i2c->dev, "Failed to allocate memory\n");
        return -ENOMEM;
    }

    memset((void *) debussy, 0, sizeof(struct debussy_priv));

    {
        uint32_t t = 0x12345678;
        uint8_t *p = (uint8_t *) &t;

        if (*p == 0x78) {
            debussy->isLittleEndian = 1;
            dev_info(&i2c->dev, "System is little endian\n");
        }
        else {
            debussy->isLittleEndian = 0;
            dev_info(&i2c->dev, "System is big endian\n");
        }

        dev_info(&i2c->dev, "[%s %d] %02X %02X %02X %02X\n", __func__,__LINE__, *p, *(p+1), *(p+2), *(p+3));
    }

    #ifdef ENABLE_DEBUSSY_I2C_REGMAP
    dev_info(&i2c->dev, "Enable I2C regmap\n");
    debussy->i2c_regmap = devm_regmap_init_i2c(i2c, &debussy_i2c_regmap);
    if (IS_ERR(debussy->i2c_regmap)) {
        dev_err(&i2c->dev, "Failed to allocate I2C regmap!\n");
        debussy->i2c_regmap = NULL;
    }
    else {
        regcache_cache_bypass(debussy->i2c_regmap, 1);
    }
    #endif

    p_debussy_priv = debussy;
    debussy->dev = &i2c->dev;
    atomic_set(&debussy->maskConfigCmd, 1);
    atomic_set(&debussy->referenceCount, 0);
    atomic_set(&debussy->kws_triggered, 0);
    atomic_set(&debussy->kws_count, 0);
    atomic_set(&debussy->vad_count, 0);
    atomic_set(&debussy->reset_stage, RESET_STAGE_NONE);
#if (SEND_VAD_CMD_IN_DRIVER==1)
    atomic_set(&debussy->vad_switch_flag, 0);
#endif 
    debussy->reset_chip = _debussy_reset_chip_ctrl;
    debussy->chip_pull_up = _debussy_chip_pull_up_ctrl;
    debussy->chip_pull_down = _debussy_chip_pull_down_ctrl;
    debussy->mcu_hold = _debussy_mcu_hold_ctrl;
#if (SEND_VAD_CMD_IN_DRIVER==1)
    debussy->set_vad_cmd = _debussy_set_vad_cmd;
#endif
    i2c_set_clientdata(i2c, debussy);

    /////////////////////////////////////////////////////////////////////////
    parser_dts_table(debussy, i2c);
    debussy_power_enable(1);

    /////////////////////////////////////////////////////////////////////////
    debussy->debussy_wq = create_singlethread_workqueue("debussy");
    if (debussy->debussy_wq == NULL) {
        dev_err(&i2c->dev, "create singlethread workqueue fail\n");
        devm_kfree(debussy->dev, debussy);
        ret = -ENOMEM;
        goto out;
    }
    debussy->debussy_fw_log_wq = create_singlethread_workqueue("debussy_fw_log");
    if (debussy->debussy_fw_log_wq == NULL) {
        dev_err(&i2c->dev, "create singlethread workqueue fail\n");
        devm_kfree(debussy->dev, debussy);
        ret = -ENOMEM;
        goto out;
    }
    debussy->debussy_voice_rec_wq = create_singlethread_workqueue("debussy_voice_rec");
    if (debussy->debussy_voice_rec_wq == NULL) {
        dev_err(&i2c->dev, "create singlethread workqueue fail\n");
        devm_kfree(debussy->dev, debussy);
        ret = -ENOMEM;
        goto out;
    }

    /////////////////////////////////////////////////////////////////////////
    debussy->debussy_pull_down_delay_wq = create_singlethread_workqueue("debussy_pull_down_delay");
    if (debussy->debussy_pull_down_delay_wq == NULL) {
        dev_err(&i2c->dev, "create singlethread workqueue fail\n");
        devm_kfree(debussy->dev, debussy);
        ret = -ENOMEM;
        goto out;
    }

    INIT_WORK(&debussy->pull_down_delay_work, _debussy_pull_down_delay);
	atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_PULL_DOWN);


    INIT_WORK(&debussy->fw_work, _debussy_manual_load_firmware);
    INIT_WORK(&debussy->fw_log_work, _debussy_fw_log_dump);
#ifdef ENABLE_SPI_INTF
    INIT_WORK(&debussy->voice_rec_work, _debussy_voice_recording);
#endif
    _debussy_debufs_init(debussy);

    mutex_init(&debussy->igo_ch_lock);

    dev_info(&i2c->dev, "Register Codec\n");
    ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_debussy, debussy_dai, ARRAY_SIZE(debussy_dai));

    {
        // Test only
        uint32_t data;

        dev_info(&i2c->dev, "I2C Test ...\n");
        debussy->reset_chip(debussy->dev, 0);

        igo_i2c_read(i2c, 0x2A000000, &data);
        dev_info(&i2c->dev, "CHIP ID: 0x%08X\n", data);
    }

    dev_info(&i2c->dev, "End of igo_i2c_probe\n");
    igo_spi_intf_init();

    #ifdef ENABLE_GENETLINK
    debussy_genetlink_init((void *) debussy);
    #endif
	debussy->vad_buf_loaded = false;
	
out:
    return ret;
}

static int igo_i2c_remove(struct i2c_client* i2c)
{
    #ifdef ENABLE_DEBUSSY_I2C_REGMAP
    if (p_debussy_priv->i2c_regmap) {
        regmap_exit(p_debussy_priv->i2c_regmap);
    }
    #endif

    if (gpio_is_valid(p_debussy_priv->reset_gpio)) {
        gpio_free(p_debussy_priv->reset_gpio);
    }

    if (gpio_is_valid(p_debussy_priv->mcu_hold_gpio)) {
        gpio_free(p_debussy_priv->mcu_hold_gpio);
    }

    devm_kfree(&i2c->dev, p_debussy_priv);
    p_debussy_priv = NULL;

    igo_spi_intf_exit();

    #ifdef ENABLE_GENETLINK
    debussy_genetlink_exit();
    #endif
    #ifdef ENABLE_CDEV
    debussy_cdev_exit();
    #endif
    snd_soc_unregister_codec(&i2c->dev);

    return 0;
}

#if (SEND_VAD_CMD_IN_DRIVER==1)
#if 0
int igo_i2c_suspend(void)
{
    struct debussy_priv *debussy = p_debussy_priv;
    unsigned int vadModeStage = atomic_read(&debussy->vad_mode_stage);
    unsigned int pullDownState = atomic_read(&debussy->pull_down_state);
    unsigned int resetStage = atomic_read(&debussy->reset_stage);
    unsigned int currOpMode = atomic_read(&debussy->curr_op_mode);
    //unsigned int vadEnable = atomic_read(&debussy->vad_enable);

    dev_info(debussy->dev, "%s:reset_stage=%d, vadModeStage=%d, pullDownState=%d, vadEnable=%d\n", __func__, 
                    resetStage, vadModeStage, pullDownState, vadEnable);
    
    if(vadEnable == 1 || 
        debussy->voice_mode == InPhoneCall ||
        debussy->voice_mode == InRecord ||
        debussy->voice_mode == InVoip)
        return 0;

    dev_info(debussy->dev, "%s(+)================>\n", __func__);

    if (1 == atomic_read(&debussy->vad_switch_flag))
    {
        if (VAD_MODE_ST_DISABLE == vadModeStage ||
            VAD_MODE_ST_ENABLE == vadModeStage ||
            VAD_MODE_ST_BYPASS == vadModeStage)
        {
            debussy->set_vad_cmd(debussy->dev);
        }
    }
    else
    {
        if (VAD_MODE_ST_DISABLE == vadModeStage)
        {
            atomic_set(&debussy->reset_stage, RESET_STAGE_CALI_DONE);   
            dev_info(debussy->dev, "%s: currOpMode=%d, resetStage=>%d\n", __func__, currOpMode, RESET_STAGE_CALI_DONE);

            if (pullDownState != PULL_DOWN_ST_PULL_DOWN && currOpMode == OP_MODE_CONFIG)
            {
                atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_CNT_DOWN);       
                dev_info(debussy->dev, "%s:pullDownState=>%d\n", __func__, PULL_DOWN_ST_CNT_DOWN);
            }
        }
    }
    dev_info(debussy->dev, "%s(-)================>\n", __func__);
    return 0;
}
EXPORT_SYMBOL(igo_i2c_suspend);
MODULE_LICENSE("GPL");
#endif    
#endif
static struct i2c_driver igo_i2c_driver = {
    .driver = {
        .name = "debussy",
        .owner = THIS_MODULE,
#if (DTS_SUPPORT == 1)
        .of_match_table = of_match_ptr(debussy_of_match),
#endif
    },
    .probe = igo_i2c_probe,
    .remove = igo_i2c_remove,
    .id_table = igo_i2c_id,
};

#if (DTS_SUPPORT == 1)
module_i2c_driver(igo_i2c_driver);
#else
static struct i2c_board_info igo_device = {
    I2C_BOARD_INFO("debussy", 0x5D),
};


#define SENSOR_BUS_NUM 1
static int __init igo_driver_init(void)
{
    printk("igo driver init\n");

    struct i2c_adapter *adap;
    struct i2c_client *client;

    adap = i2c_get_adapter(SENSOR_BUS_NUM);
 
    if (!adap) { 
	    printk("i2c adapter %d\n",SENSOR_BUS_NUM); 
	    return -ENODEV; 
    } else { 
	    printk("get ii2 adapter %d ok\n", SENSOR_BUS_NUM); client = i2c_new_device(adap, &igo_device); 
    } 

    if (!client) {
        printk("get i2c client %s @ 0x%02x fail!\n", igo_device.type, igo_device.addr); 
        return -ENODEV; 
    } 
    else { 
        printk("get i2c client ok!\n"); 
    } 
    
    i2c_put_adapter(adap);

    return i2c_add_driver(&igo_i2c_driver);
}
module_init(igo_driver_init);

static void __exit igo_driver_exit(void)
{
    return i2c_del_driver(&igo_i2c_driver);
}
module_exit(igo_driver_exit);
#endif /* end of DTS_SUPPORT */

MODULE_DESCRIPTION("Debussy driver");
MODULE_LICENSE("GPL v2");
