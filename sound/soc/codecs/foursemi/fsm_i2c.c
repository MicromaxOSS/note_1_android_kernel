/*
 * Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
 */
#include "fsm_public.h"
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/i2c.h>
//#include <linux/slab.h>
//#include <linux/uaccess.h>
//#include <linux/ioctl.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#if defined(CONFIG_REGULATOR)
#include <linux/regulator/consumer.h>
static struct regulator *g_fsm_vdd = NULL;
#endif

#define FSDEV_I2C_NAME        "fs16xx"

#define FS_KERNEL_VERSION        "kusai_V1.1_20200401"

static fsm_dev_t *fsm_device[MAX_DEV_COUNT] = {NULL, NULL};

static DEFINE_MUTEX(fsm_mutex);
static LIST_HEAD(fsm_device_list);
static int g_dev_count = 0;

void fsm_mutex_lock(void)
{
    mutex_lock(&fsm_mutex);
}

void fsm_mutex_unlock(void)
{
    mutex_unlock(&fsm_mutex);
}

int fsm_dump_info(char *buf)
{
    fsm_pdata_t *pdata = NULL;
    fsm_dev_t *fsm_dev = NULL;
    fsm_config_t *cfg = NULL;
    int len = 0;

    list_for_each_entry(pdata, &fsm_device_list, list)
    {
        fsm_dev = &pdata->fsm_dev;
        cfg = fsm_dev->config;
        len += sprintf(buf + len, "[%s]\n", fsm_dev->dev_name);
        len += sprintf(buf + len, "addr\t: 0x%02x\n", fsm_dev->i2c_addr);
        len += sprintf(buf + len, "version\t: %02x\n", fsm_dev->version);
        len += sprintf(buf + len, "rate\t: %d\n", cfg->srate);
        len += sprintf(buf + len, "bclk\t: %d\n", cfg->bclk);
        len += sprintf(buf + len, "volume\t: %d\n", cfg->volume);
        len += sprintf(buf + len, "scene\t: %d\n", cfg->next_scene);
        len += sprintf(buf + len, "state\t: %04x\n", fsm_dev->dev_state);
    }
    pr_info("%s length: %d\n", __func__, len);

    return len;
}

fsm_dev_t *fsm_get_device(void)
{
    return (fsm_dev_t *)fsm_device;
}


int fsm_check_device(void)
{
    return g_dev_count;
}
EXPORT_SYMBOL(fsm_check_device);

int fsm_i2c_cmd(int cmd, int argv)
{
    fsm_pdata_t *data_ptr = NULL;
    fsm_dev_t *fsm_dev = NULL;
    int ret = 0, flag = 0;

    pr_info("%s cmd: %d\n", __func__, cmd);

    if(g_dev_count <= 0)
        return -EINVAL;

    list_for_each_entry(data_ptr, &fsm_device_list, list)
    {
        mutex_lock(&data_ptr->i2c_lock);
        fsm_dev = &data_ptr->fsm_dev;
        if(((cmd == FSM_CMD_CALIB_STEP1) || (cmd == FSM_CMD_CALIB_STEP2) || (cmd == FSM_CMD_CALIB_STEP3)) &&
            ((fsm_dev->dev_state & STATE(OTP_STORED)) != 0) && !argv){
                pr_info("%s calibrate already done.\n", __func__);
                mutex_unlock(&data_ptr->i2c_lock);
               continue;
        }
        if(cmd == FSM_CMD_LOAD_FW)
        {
            ret |= fsm_init_firmware_sync(data_ptr, argv);
        } else {
            ret |= fsm_set_cmd(fsm_dev, cmd, argv);
            if(cmd == FSM_CMD_CALIB_STEP1)
                flag |= fsm_dev->dev_state;
        }
        mutex_unlock(&data_ptr->i2c_lock);
    }
    if((cmd == FSM_CMD_CALIB_STEP1) && (fsm_dev->dev_state & STATE(OTP_STORED)) == 0)
        fsm_delay_ms(2500); //need delay when first-time calibrate

    return ret;
}

struct i2c_client *fsm_get_i2c(uint8_t addr)
{
    fsm_pdata_t *data_ptr = NULL;
    fsm_dev_t *fsm_dev = NULL;

    if(fsm_check_device() <= 0)
        return NULL;

    list_for_each_entry(data_ptr, &fsm_device_list, list)
    {
        fsm_dev = &data_ptr->fsm_dev;
        if(fsm_dev->i2c->addr == addr)
            return fsm_dev->i2c;
    }

    return NULL;
}

int fsm_speaker_on(void)
{
    int ret = 0;

    //ret |= fsm_i2c_cmd(FSM_CMD_SWH_PRESET, 0);
    ret |= fsm_i2c_cmd(FSM_CMD_START_UP, 0);
    fsm_i2c_cmd(FSM_CMD_WAIT_STABLE, FSM_WAIT_AMP_ON);
    ret |= fsm_i2c_cmd(FSM_CMD_SET_MUTE, FSM_UNMUTE);

    return ret;
}

int fsm_speaker_off(void)
{
    int ret = 0;

    pr_info("enter\n");
    ret = fsm_i2c_cmd(FSM_CMD_SET_MUTE, FSM_MUTE);
    //ret |= fsm_i2c_cmd(FSM_CMD_WAIT_STABLE, FSM_WAIT_TSIGNAL_OFF);
    ret |= fsm_i2c_cmd(FSM_CMD_SHUT_DOWN, 0);
    fsm_i2c_cmd(FSM_CMD_WAIT_STABLE, FSM_WAIT_AMP_ADC_PLL_OFF);

    pr_info("exit\n");

    return ret;
}

int fsm_calibrate(int force)
{
    int ret = 0;

    pr_info("%s fsm_calibrate enter.\n", __func__);
    ret = fsm_i2c_cmd(FSM_CMD_CALIB_STEP1, force);
    ret |= fsm_i2c_cmd(FSM_CMD_CALIB_STEP2, force);
    ret |= fsm_i2c_cmd(FSM_CMD_CALIB_STEP3, force);
    //if(!ret) {
    //    ret |= fsm_i2c_cmd(FSM_CMD_SET_MUTE, FSM_UNMUTE);
    //}

    pr_info("%s fsm_calibrate exit, ret = %d.\n", __func__, ret);
    return ret;
}

int fsm_get_zmdata_str(char *buf, int max_size)
{
    fsm_pdata_t  *data_ptr = NULL;
    fsm_dev_t *fsm_dev = NULL;
    uint16_t zmdata = 0;
    int str_len = 0;
    int len = 0;

    pr_debug("%s : enter",__func__);
    if (fsm_check_device() <= 0) {
        pr_err("no found device");
        return 0;
    }
    fsm_mutex_lock();
    list_for_each_entry(data_ptr, &fsm_device_list, list) {
        mutex_lock(&data_ptr->i2c_lock);
        fsm_dev = &data_ptr->fsm_dev;
        zmdata = fsm_get_zmdata(fsm_dev);
        len = snprintf(buf + str_len, max_size, "%02x:%04x %d",
                fsm_dev->i2c->addr, zmdata & 0xffff, fsm_dev->errcode);
        str_len += len;
        mutex_unlock(&data_ptr->i2c_lock);
    }
    if (str_len > max_size) {
        pr_err("string lengh error: %d", str_len);
    }
    fsm_mutex_unlock();

    pr_info("zmdata: %d ,fsm_dev->errcode: %d", zmdata, fsm_dev->errcode);
    return str_len;
}

int fsm_get_r25_str(char *buf, int max_size)
{
    fsm_pdata_t  *data_ptr = NULL;
    fsm_dev_t *fsm_dev = NULL;
    int str_len = 0;
    int len = 0;
    int ohms = 0;

    if (fsm_check_device() <= 0) {
        pr_err("no found device");
        return 0;
    }
    fsm_mutex_lock();
    list_for_each_entry(data_ptr, &fsm_device_list, list) {
        mutex_lock(&data_ptr->i2c_lock);
        fsm_dev = &data_ptr->fsm_dev;
        ohms = fsm_dev->calib_data.calib_re25;
        len = snprintf(buf + str_len, max_size, "%02x %d %d %d",
                fsm_dev->i2c->addr, ohms,fsm_dev->spkr,fsm_dev->errcode);
        str_len += len;
        mutex_unlock(&data_ptr->i2c_lock);
    }
    if (str_len > max_size) {
        pr_err("string lengh error: %d", str_len);
    }
    fsm_mutex_unlock();

    //pr_info("fsm_dev->errcode: %d", fsm_dev->errcode);
    return str_len;
}

int fsm_i2c_event(int event, int argv)
{
    int ret = 0, dev_count = 0;
    fsm_config_t *config = fsm_get_config();

    dev_count = fsm_check_device();
    if(dev_count < 1)
        return -EINVAL;

    pr_info("%s event:%d devices:%d\n", __func__, event, dev_count);

    switch(event)
    {
        case FSM_EVENT_GET_DEVICE:
            return dev_count;
        case FSM_EVENT_GET_SCENE:
            return config->next_scene;
        default:
            break;
    }

    fsm_mutex_lock();
    switch(event)
    {
        case FSM_EVENT_SET_SRATE:
            config->srate = (unsigned int)argv;
            break;
        case FSM_EVENT_SET_BCLK:
            config->bclk = (unsigned int)argv;
            break;
        case FSM_EVENT_SET_SCENE:
            //config->next_scene = argv;
            //if(config->state == FSM_UNMUTE)
                ret |= fsm_i2c_cmd(FSM_CMD_SWH_PRESET, 1);
            break;
        case FSM_EVENT_SET_VOLUME:
            config->volume = argv;
            break;
        case FSM_EVENT_SPEAKER_ON:
            ret = fsm_speaker_on();
            config->state = FSM_UNMUTE;
            break;
        case FSM_EVENT_SPEAKER_OFF:
            ret = fsm_speaker_off();
            config->state = FSM_MUTE;
            break;
        case FSM_EVENT_LOAD_FW:
            ret = fsm_i2c_cmd(FSM_CMD_LOAD_FW, argv);
            break;
        case FSM_EVENT_INIT:
            ret = fsm_i2c_cmd(FSM_CMD_INIT, argv);
            break;
        case FSM_EVENT_CALIBRATE:
            ret = fsm_calibrate(argv);
            break;
        case FSM_EVENT_GET_RE25:
			
            break;//TODO
        default:
            break;
    }
    fsm_mutex_unlock();

    return ret;
}

#if defined(CONFIG_FSM_MISC)
int fsm_dev_read(uint8_t addr, uint8_t reg, uint16_t *val)
{
    int ret = 0;
    uint16_t value = 0;
    fsm_pdata_t  *data_ptr = NULL;

    if(fsm_check_device() <= 0)
        return -EINVAL;

    fsm_mutex_lock();
    list_for_each_entry(data_ptr, &fsm_device_list, list)
    {
        if(addr == data_ptr->fsm_dev.i2c_addr)
        {
            ret |= fsm_reg_read(&data_ptr->fsm_dev, reg, &value);
            *val = value;
        }
    }
    fsm_mutex_unlock();

    return ret;
}

int fsm_dev_write(uint8_t addr, uint8_t reg, uint16_t val)
{
    int ret = 0;
    fsm_pdata_t  *data_ptr = NULL;

    if(fsm_check_device() <= 0)
        return -EINVAL;

    fsm_mutex_lock();
    list_for_each_entry(data_ptr, &fsm_device_list, list)
    {
        if(addr == data_ptr->fsm_dev.i2c_addr)
        {
            ret |= fsm_reg_write(&data_ptr->fsm_dev, reg, val);
        }
    }
    fsm_mutex_unlock();

    return ret;
}
#endif

int fsm_power_on(struct i2c_client *i2c)
{
    int ret = 0;

#if defined(CONFIG_REGULATOR)
    g_fsm_vdd = regulator_get(&i2c->dev, "fsm_vdd");
    if(IS_ERR(g_fsm_vdd) != 0)
    {
        pr_err("%s error getting fsm_vdd regulator, ret: %d.", __func__, ret);
        ret = PTR_ERR(g_fsm_vdd);
        g_fsm_vdd = NULL;
        return ret;
    }
    regulator_set_voltage(g_fsm_vdd, 1800000, 1800000);
    ret = regulator_enable(g_fsm_vdd);
    if (ret < 0) {
        pr_err("%s enabling vdd regulator failed, ret: %d.", __func__, ret);
    }
#endif
    fsm_delay_ms(20);

    return ret;
}

void fsm_power_off(void)
{
    if(g_dev_count > 0)
        return;

#if defined(CONFIG_REGULATOR)
    if(g_fsm_vdd)
    {
        regulator_disable(g_fsm_vdd);
        regulator_put(g_fsm_vdd);
        g_fsm_vdd = NULL;
    }
#endif
}

#if defined(CONFIG_FSM_I2C)
int fsm_i2c_reg_read(struct i2c_client *i2c, uint8_t reg, uint16_t *pVal)
{
    int ret = 0;
    uint8_t retries = 20;
    uint8_t buffer[2] = {0};
    struct i2c_msg msgs[2];

    if(i2c == NULL || pVal == NULL)
        return -EINVAL;

    // write register address.
    msgs[0].addr = i2c->addr;
    msgs[0].flags = 0;
    msgs[0].len = 1;
    msgs[0].buf = &reg;
    // read register buffer.
    msgs[1].addr = i2c->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = 2;
    msgs[1].buf = &buffer[0];

    do
    {
        ret = i2c_transfer(i2c->adapter, &msgs[0], ARRAY_SIZE(msgs));
        if (ret != ARRAY_SIZE(msgs))
            fsm_delay_ms(5);
    }
    while ((ret != ARRAY_SIZE(msgs)) && (--retries > 0));

    if (ret != ARRAY_SIZE(msgs))
    {
        pr_err("%s read transfer error, ret: %d.", __func__, ret);
        return -EIO;
    }

    *pVal = ((buffer[0] << 8) | buffer[1]);

    return 0;
}

static int fsm_i2c_msg_write(struct i2c_client *i2c, uint8_t *buff, uint8_t len)
{
    int ret = 0;
    uint8_t retries = 20;

    struct i2c_msg msgs[] = {
        {
        .addr = i2c->addr,
        .flags = 0,
        .len = len + 1,
        .buf = buff,
        },
    };

    if (!i2c || !buff)
        return -EINVAL;

    do
    {
        ret = i2c_transfer(i2c->adapter, msgs, ARRAY_SIZE(msgs));
        if (ret != ARRAY_SIZE(msgs))
            fsm_delay_ms(5);
    }
    while ((ret != ARRAY_SIZE(msgs)) && (--retries > 0));

    if (ret != ARRAY_SIZE(msgs))
    {
        pr_err("%s write transfer error, ret: %d.", __func__, ret);
        return -EIO;
    }

    return ret;
}
int fsm_i2c_reg_write(struct i2c_client *i2c, uint8_t reg, uint16_t val)
{
    int ret = 0;
    uint8_t buffer[3] = {0};

    buffer[0] = reg;
    buffer[1] = (val >> 8) & 0x00ff;
    buffer[2] = val & 0x00ff;

    ret = fsm_i2c_msg_write(i2c, &buffer[0], 2);
    if (ret < 0)
        pr_err("%s write error, ret: %d.", __func__, ret);

    return (ret < 0) ? ret : 0;
}

int fsm_i2c_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *data, uint32_t len)
{
    int ret = 0, count = 0;
    uint8_t *buffer = NULL;

    buffer = fsm_alloc_mem(len + 1);
    if (!buffer) {
        pr_err("alloc memery failed");
        return -EINVAL;
    }

    buffer[count++] = reg;
    memcpy(&buffer[count], data, len);

    ret = fsm_i2c_msg_write(fsm_dev->i2c, &buffer[0], len);
    fsm_free_mem(buffer);
    if (ret < 0)
        pr_err("%s write error, ret: %d.", __func__, ret);

    return (ret < 0) ? ret : 0;
}

#endif

static void *fsm_devm_kstrdup(struct device *dev, char *buf)
{
    char *str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);
    if (!str)
        return str;
    memcpy(str, buf, strlen(buf));
    return str;
}

#ifdef CONFIG_OF
static int fsm_parse_dts(struct i2c_client *i2c, fsm_pdata_t *pdata)
{
    struct device_node *np = i2c->dev.of_node;
    fsm_dev_t *fsm_dev = NULL;
    char i2c_addr[16];
    int ret = 0;

    pr_info("%s enter\n", __func__);
    if(pdata == NULL || np == NULL)
        return -EINVAL;

    fsm_dev = &pdata->fsm_dev;
    if (of_property_read_string(np, "fsm,dev-name", &fsm_dev->dev_name))
    {
        sprintf(i2c_addr, "0x%02x", i2c->addr);
        fsm_dev->dev_name = fsm_devm_kstrdup(&i2c->dev, i2c_addr);
    }

    pr_info("%s exit\n", __func__);

    return ret;
}
#endif

int fsm_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
    fsm_pdata_t *pdata = NULL;
    fsm_dev_t *fsm_dev = NULL;
    int ret = 0;

    pr_info("%s enter version : %s.\n", __func__,FS_KERNEL_VERSION);

    if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C))
    {
        dev_err(&i2c->dev, "check_functionality failed\n");
        return -EIO;
    }

    fsm_power_on(i2c);

    pdata = devm_kzalloc(&i2c->dev, sizeof(fsm_pdata_t), GFP_KERNEL);
    if (pdata == NULL)
        return -ENOMEM;

    fsm_dev = &pdata->fsm_dev;

#if defined(CONFIG_FSM_REGMAP)
    fsm_dev->regmap = fsm_regmap_i2c_init(i2c);
    if (fsm_dev->regmap == NULL)
    {
        devm_kfree(&i2c->dev, pdata);
        return -EINVAL;
    }
#endif
#ifdef CONFIG_OF
    ret |= fsm_parse_dts(i2c, pdata);
    if(ret)
        pr_err("%s failed to parse DTS node\n", __func__);

#endif
    // check device id.
    fsm_dev->i2c = i2c;
    fsm_dev->i2c_addr = i2c->addr;
    ret = fsm_probe(&pdata->fsm_dev);
    if(ret)
    {
        pr_err("%s detect device failed\n", __func__);
        fsm_power_off();
        devm_kfree(&i2c->dev, pdata);
        return ret;
    }

    pdata->dev = &i2c->dev;
    i2c_set_clientdata(i2c, pdata);
    mutex_init(&pdata->i2c_lock);
#if defined(CONFIG_FSM_CODEC)
    // register codec driver
    ret |= fsm_codec_register(i2c);
    if(ret < 0)
        return ret;
#endif

#if defined(CONFIG_FSM_MISC)
    // register misc device
    fsm_misc_init();
#endif
#if defined(CONFIG_FSM_PROC)
    // register proc filesystem
    fsm_proc_init();
#endif

    ret |= fsm_init_firmware(pdata,0);

    //register sys node
    ret |= fsm_sysfs_init(i2c);

    INIT_LIST_HEAD(&pdata->list);

    fsm_mutex_lock();
    list_add(&pdata->list, &fsm_device_list);
    fsm_mutex_unlock();
    fsm_device[g_dev_count] = fsm_dev;
    g_dev_count++;
    pr_info("%s i2c probe completed! preset name: %s, ret = %d\n", __func__, fsm_dev->preset_name, ret);

    return ret;
}

int fsm_i2c_remove(struct i2c_client *i2c)
{
    fsm_pdata_t *pdata = i2c_get_clientdata(i2c);
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;

#if defined(CONFIG_FSM_REGMAP)
    fsm_regmap_i2c_deinit(fsm_dev->regmap);
#endif
    mutex_lock(&fsm_mutex);
    list_del(&pdata->list);
    g_dev_count--;
    mutex_unlock(&fsm_mutex);
#if defined(CONFIG_FSM_CODEC)
    fsm_codec_unregister(i2c);
#endif   
    fsm_power_off();

#if defined(CONFIG_FSM_MISC)
    fsm_misc_deinit();
#endif

#if defined(CONFIG_FSM_PROC)
    fsm_proc_deinit();
#endif

    fsm_remove(fsm_dev);
    if(pdata)
        devm_kfree(&i2c->dev, pdata);

    return 0;
}

static const struct i2c_device_id fsm_i2c_id[] =
{
    { FSDEV_I2C_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, fsm_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id fsm_match_tbl[] =
{
    { .compatible = "foursemi,fs16xx" },
    { },
};
MODULE_DEVICE_TABLE(of, fsm_match_tbl);
#endif

static struct i2c_driver fsm_i2c_driver =
{
    .driver =
    {
        .name = FSDEV_I2C_NAME,
        .owner = THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = of_match_ptr(fsm_match_tbl),
#endif
    },
    .probe =    fsm_i2c_probe,
    .remove =   fsm_i2c_remove,
    .id_table = fsm_i2c_id,
};

int fsm_i2c_init(void)
{
    int ret = 0;
    ret = i2c_add_driver(&fsm_i2c_driver);
    return ret;
}

void fsm_i2c_exit(void)
{
    i2c_del_driver(&fsm_i2c_driver);
}

#if !defined(module_i2c_driver)
static int __init fsm_mod_init(void)
{
    int ret = 0;
    ret = i2c_add_driver(&fsm_i2c_driver);
    return ret;
}

static void __exit fsm_mod_exit(void)
{
    i2c_del_driver(&fsm_i2c_driver);
}

module_init(fsm_mod_init);
module_exit(fsm_mod_exit);
#else
module_i2c_driver(fsm_i2c_driver);
#endif

MODULE_AUTHOR("FourSemi SW <support@foursemi.com>");
MODULE_DESCRIPTION("FourSemi Smart PA i2c driver");
MODULE_LICENSE("GPL");
