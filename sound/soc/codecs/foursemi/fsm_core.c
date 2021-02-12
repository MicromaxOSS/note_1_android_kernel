/**
 * Copyright (c) Foursemi Co., Ltd. 2018-2019. All rights reserved.
 *Description:  Core Code For Foursemi Devices.
 *Author: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */
#include "fsm_public.h"
//#include <unistd.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define REG(field) FSM_##field
#define HIGH8(val) (((val) >> 8) & 0xff)
#define LOW8(val)  ((val) & 0xff)

#define CRC16_TABLE_SIZE  256
#define CRC16_POLY_NOMIAL 0xA001

static uint16_t g_crc16table[CRC16_TABLE_SIZE] = {
    0x0000, 0xc0c1, 0xc181, 0x0140, 0xc301, 0x03c0, 0x0280, 0xc241, 0xc601, 0x06c0, 0x0780, 0xc741, 0x0500, 0xc5c1, 0xc481, 0x0440,
    0xcc01, 0x0cc0, 0x0d80, 0xcd41, 0x0f00, 0xcfc1, 0xce81, 0x0e40, 0x0a00, 0xcac1, 0xcb81, 0x0b40, 0xc901, 0x09c0, 0x0880, 0xc841,
    0xd801, 0x18c0, 0x1980, 0xd941, 0x1b00, 0xdbc1, 0xda81, 0x1a40, 0x1e00, 0xdec1, 0xdf81, 0x1f40, 0xdd01, 0x1dc0, 0x1c80, 0xdc41,
    0x1400, 0xd4c1, 0xd581, 0x1540, 0xd701, 0x17c0, 0x1680, 0xd641, 0xd201, 0x12c0, 0x1380, 0xd341, 0x1100, 0xd1c1, 0xd081, 0x1040,
    0xf001, 0x30c0, 0x3180, 0xf141, 0x3300, 0xf3c1, 0xf281, 0x3240, 0x3600, 0xf6c1, 0xf781, 0x3740, 0xf501, 0x35c0, 0x3480, 0xf441,
    0x3c00, 0xfcc1, 0xfd81, 0x3d40, 0xff01, 0x3fc0, 0x3e80, 0xfe41, 0xfa01, 0x3ac0, 0x3b80, 0xfb41, 0x3900, 0xf9c1, 0xf881, 0x3840,
    0x2800, 0xe8c1, 0xe981, 0x2940, 0xeb01, 0x2bc0, 0x2a80, 0xea41, 0xee01, 0x2ec0, 0x2f80, 0xef41, 0x2d00, 0xedc1, 0xec81, 0x2c40,
    0xe401, 0x24c0, 0x2580, 0xe541, 0x2700, 0xe7c1, 0xe681, 0x2640, 0x2200, 0xe2c1, 0xe381, 0x2340, 0xe101, 0x21c0, 0x2080, 0xe041,
    0xa001, 0x60c0, 0x6180, 0xa141, 0x6300, 0xa3c1, 0xa281, 0x6240, 0x6600, 0xa6c1, 0xa781, 0x6740, 0xa501, 0x65c0, 0x6480, 0xa441,
    0x6c00, 0xacc1, 0xad81, 0x6d40, 0xaf01, 0x6fc0, 0x6e80, 0xae41, 0xaa01, 0x6ac0, 0x6b80, 0xab41, 0x6900, 0xa9c1, 0xa881, 0x6840,
    0x7800, 0xb8c1, 0xb981, 0x7940, 0xbb01, 0x7bc0, 0x7a80, 0xba41, 0xbe01, 0x7ec0, 0x7f80, 0xbf41, 0x7d00, 0xbdc1, 0xbc81, 0x7c40,
    0xb401, 0x74c0, 0x7580, 0xb541, 0x7700, 0xb7c1, 0xb681, 0x7640, 0x7200, 0xb2c1, 0xb381, 0x7340, 0xb101, 0x71c0, 0x7080, 0xb041,
    0x5000, 0x90c1, 0x9181, 0x5140, 0x9301, 0x53c0, 0x5280, 0x9241, 0x9601, 0x56c0, 0x5780, 0x9741, 0x5500, 0x95c1, 0x9481, 0x5440,
    0x9c01, 0x5cc0, 0x5d80, 0x9d41, 0x5f00, 0x9fc1, 0x9e81, 0x5e40, 0x5a00, 0x9ac1, 0x9b81, 0x5b40, 0x9901, 0x59c0, 0x5880, 0x9841,
    0x8801, 0x48c0, 0x4980, 0x8941, 0x4b00, 0x8bc1, 0x8a81, 0x4a40, 0x4e00, 0x8ec1, 0x8f81, 0x4f40, 0x8d01, 0x4dc0, 0x4c80, 0x8c41,
    0x4400, 0x84c1, 0x8581, 0x4540, 0x8701, 0x47c0, 0x4680, 0x8641, 0x8201, 0x42c0, 0x4380, 0x8341, 0x4100, 0x81c1, 0x8081, 0x4040,
 };

static struct fsm_config g_fsm_config = {
    .event = FSM_CMD_UNKNOWN,
    .volume = 0xff,
    .channel = FSM_CHN_MONO,
    .next_scene = FSM_SCENE_MUSIC,
    .bclk = 5644800, // bck ratio 3072000
    .srate = 48000, // sample rate 48000
    .flags = 0,
    .f0cal_mode = 0,
    .dev_sel = 0xff,
};
struct preset_file *g_presets_file = NULL;

fsm_config_t *fsm_get_config(void)
{
    return &g_fsm_config;
}
void *fsm_alloc_mem(int size)
{
#if defined(__KERNEL__)
    return kzalloc(size, GFP_KERNEL);
#elif defined(FSM_HAL_V2_SUPPORT)
    return malloc(size);
#else
    return NULL;
#endif
}

void fsm_free_mem(void *buf)
{
#if defined(__KERNEL__)
    if(buf)
        kfree(buf);
#elif defined(FSM_HAL_V2_SUPPORT)
    if(buf)
        free(buf);
#endif
    buf = NULL;
}

void fsm_delay_ms(uint32_t delay_ms)
{
    if(delay_ms == 0) return;
#if defined(__KERNEL__)
    usleep_range(delay_ms * 1000, delay_ms * 1000 + 1);
#elif defined(FSM_HAL_V2_SUPPORT)
    usleep(delay_ms * 1000);
#endif
}

int fsm_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val)
{
    int ret = 0;

#if defined(FSM_DEBUG)
    pr_info("%s %02x-%02X<-%04X\n", __func__, fsm_dev->i2c_addr, reg, val);
#endif
#if defined(CONFIG_FSM_REGMAP)
    ret = fsm_regmap_write(fsm_dev->regmap, reg, val);
#elif defined(CONFIG_FSM_I2C)
    ret = fsm_i2c_reg_write(fsm_dev->i2c, reg, val);
#elif defined(FSM_HAL_V2_SUPPORT)
    ret = fsm_hal_reg_write(fsm_dev, reg, val);
#endif
    if(ret != 0) {
        pr_err("%s [%04X->%02X] write failed, ret: %d.\n", __func__, val, reg, ret);
    }

    return ret;
}

int fsm_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pval)
{
    int ret = 0;
    uint16_t value = 0;

#if defined(CONFIG_FSM_REGMAP)
    ret = fsm_regmap_read(fsm_dev->regmap, reg, &value);
#elif defined(CONFIG_FSM_I2C)
    ret = fsm_i2c_reg_read(fsm_dev->i2c, reg, &value);
#elif defined(FSM_HAL_V2_SUPPORT)
    ret = fsm_hal_reg_read(fsm_dev, reg, &value);
#endif
#if defined(FSM_DEBUG)
    pr_info("%s %02x-%02X->%04X\n", __func__, fsm_dev->i2c_addr, reg, value);
#endif
    if(ret != 0) {
        pr_err("%s reg[%02X] read failed, ret: %d.\n", __func__, reg, ret);
    }
    if(pval)
        *pval = value;

    return ret;
}

int fsm_reg_wmask(fsm_dev_t *fsm_dev, struct reg_unit *reg)
{
    uint16_t temp = 0;
    uint16_t mask = 0;
    uint16_t val = 0;
    uint8_t addr = 0;
    int ret = 0;

    addr = reg->addr;
    if (reg->pos == 0 && reg->len == 15) {
        ret = fsm_reg_write(fsm_dev, addr, reg->value);
    } else {
        mask = ((1 << (reg->len + 1)) - 1) << reg->pos;
        val = (reg->value << reg->pos);
        ret = fsm_reg_read(fsm_dev, addr, &temp);
        temp = ((~mask & temp) | (val & mask));
        ret |= fsm_reg_write(fsm_dev, addr, temp);
    }

    return ret;
}

#if defined(FSM_DEBUG)
void fsm_byte_to_str(char *str, const uint8_t *buf, int len)
{
    int i = 0;
    int step = 3;

    for (i = 0; i < len; i++) {
        //sprintf_s((str + i * step), sizeof(str), "%02X ", buf[i]);
        sprintf((str + i * step), "%02X ", buf[i]);
    }
    *(str + i * step) = '\0';
}
#endif

int fsm_burst_write(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *val, int len)
{
    int ret = 0;

#if defined(FSM_DEBUG)
    int start_point = 7;
    char pr_buf[512];

    //sprintf_s(pr_buf, sizeof(pr_buf), "%02x-%02X<-", fsm_dev->i2c_addr, reg);
    sprintf(pr_buf, "%02x-%02X<-", fsm_dev->i2c_addr, reg);
    fsm_byte_to_str(pr_buf + start_point, val, len);
    pr_info("%s %s\n", __func__, pr_buf);
#endif

#if defined(CONFIG_FSM_REGMAP)
    ret = fsm_regmap_bulkwrite(fsm_dev->regmap, reg, val, len);
#elif defined(CONFIG_FSM_I2C)
    ret = fsm_i2c_bulkwrite(fsm_dev, reg, val, len);
#elif defined(FSM_HAL_V2_SUPPORT)
    ret = fsm_hal_bulkwrite(fsm_dev, reg, val, len);
#endif
    return ret;
}

uint16_t fsm_calc_checksum(uint16_t *data, int len)
{
    uint16_t crc = 0;
    uint8_t b = 0, index = 0;
    int bit_shift = 8;
    int i;

    if (len <= 0)
        return 0;

    for (i = 0; i < len; i++) {
        b = (uint8_t)(data[i] & 0xFF);
        index = (uint8_t)(crc ^ b);
        crc = (uint16_t)((crc >> bit_shift) ^ g_crc16table[index]);

        b = (uint8_t)((data[i] >> bit_shift) & 0xFF);
        index = (uint8_t)(crc ^ b);
        crc = (uint16_t)((crc >> bit_shift) ^ g_crc16table[index]);
    }
    return crc;
}
int fsm_access_key(fsm_dev_t *fsm_dev, int access)
{
    return fsm_reg_write(fsm_dev, 0x0b, access ? FSM_OTP_ACC_KEY2 : 0);
}

int fsm_reg_dump(fsm_dev_t *fsm_dev)
{
    int addr;
    int ret = 0;
    uint16_t value = 0;

    for (addr = 0; addr < 0xff; addr++) {
        if (addr == 0x0c) {
            addr = 0x89;
        } else if (addr == 0x8a) {
            addr = 0xa1;
        } else if (addr == 0xa2) {
            addr = 0xaf;
        } else if (addr == 0xb0) {
            addr = 0xbb;
        } else if (addr == 0xbc) {
            addr = 0xc0;
        } else if (addr == 0xc5) {
            addr = 0xc9;
        } else if (addr == 0xd0) {
            addr = 0xe0;
        }

        if (addr == 0xe0) {
            ret |= fsm_access_key(fsm_dev, 1);
        } else if (addr == 0xe9) {
            ret |= fsm_access_key(fsm_dev, 0);
            break;
        }
        ret |= fsm_reg_read(fsm_dev, addr, &value);
        pr_info("%s [%02x] %02X: %04X\n", __func__, fsm_dev->i2c_addr, addr, value);
    }

    return ret;
}

int fsm_init_dev_list(fsm_dev_t *fsm_dev, struct preset_file *pfile)
{
    dev_list_t *dev_list = NULL;
    uint8_t *preset = NULL;
    uint16_t offset = 0;
    uint16_t type = 0;
    int i;

    pr_info("%s: enter\n", __func__);
    if (!pfile)
        return -EINVAL;

    preset = (uint8_t *)pfile;
    pr_info("%s ndev: %d\n", __func__, pfile->hdr.ndev);
    for (i = 0; i < pfile->hdr.ndev; i++) {
        type = pfile->index[i].type;
        if (type == FSM_DSC_DEV_INFO) {
            offset = pfile->index[i].offset;
            pr_info("%s dev: %d, offset: %d\n", __func__, i, offset);
            dev_list = (struct dev_list *)&preset[offset];
            if (fsm_dev->i2c_addr == dev_list->addr) {
                break;
            }
        }
    }
    if (HIGH8(fsm_dev->version) != HIGH8(dev_list->dev_type) || i == pfile->hdr.ndev) {
        pr_err("%s device type(%04x) not matched version(%04x)\n", __func__, dev_list->dev_type, fsm_dev->version);
        fsm_dev->dev_list = NULL;
        fsm_dev->dev_state &= ~(STATE(FW_INITED));
        return -EINVAL;
    }
    fsm_dev->dev_list = dev_list;
    fsm_dev->dev_state |= STATE(FW_INITED);
    pr_info("%s addr: 0x%02x\n", __func__, dev_list->addr);
    pr_info("%s bus: %d\n", __func__, dev_list->bus);
    pr_info("%s dev_type: %02x\n", __func__, dev_list->dev_type);
    pr_info("%s eq_scenes: %04x\n", __func__, dev_list->eq_scenes);
    pr_info("%s reg_scenes: %04x\n", __func__, dev_list->reg_scenes);
    pr_info("%s len: %d\n", __func__, dev_list->len);
    pr_info("%s npreset: %d\n", __func__, dev_list->npreset);
    fsm_dev->own_scene = dev_list->reg_scenes | dev_list->eq_scenes;
    pr_info("%s: exit\n", __func__);

    return 0;
}

int fsm_parse_firmware(fsm_dev_t *fsm_dev, uint8_t *pdata, uint32_t size)
{
    struct preset_file *pfile = NULL;
    struct preset_header *hdr = NULL;
    uint16_t checksum = 0;
    int ret = 0;

    pfile = (struct preset_file *)fsm_alloc_mem(size);
    if (!pfile) {
        pr_err("%s failed to allocate %d bytes\n", __func__, size);
        return -ENOMEM;
    }
    if (!pdata) {
        pr_err("pdata == NULL\n");
        return -ENOMEM;
    }
    //memcpy_s(pfile, size, pdata, size);
    memcpy(pfile, pdata, size);
    hdr = &pfile->hdr;

    pr_info("%s version : 0x%04x\n", __func__, hdr->version);
    pr_info("%s customer: %s\n", __func__, hdr->customer);
    pr_info("%s project : %s\n", __func__, hdr->project);
    pr_info("%s date    : %4d-%02d-%02d-%02d-%02d\n", __func__, hdr->date.year, hdr->date.month,
        hdr->date.day, hdr->date.hour, hdr->date.min);
    pr_info("%s size    : %d\n", __func__, hdr->size);
    pr_info("%s crc16   : 0x%04x\n", __func__, hdr->crc16);

    if (hdr->size <= 0 && hdr->size != size) {
        pr_err("%s wrong file size: %d\n", __func__, hdr->size);
        return -EINVAL;
    }

    checksum = fsm_calc_checksum((uint16_t *)(&(pfile->hdr.ndev)),
            (size - sizeof(struct preset_header) + 2)/2);
    if (checksum != hdr->crc16) {
        pr_err("%s checksum(0x%04x) not match(0x%04x)\n", __func__, checksum, hdr->crc16);
        fsm_free_mem(pfile);
        return -EINVAL;
    }
    fsm_dev->preset = pfile;
    ret = fsm_init_dev_list(fsm_dev, pfile);
    if (ret)
        return ret;
    g_presets_file = pfile;

    pr_info("%s exit, ret: %d\n", __func__, ret);

    return ret;
}

uint8_t count_bit0_number(uint8_t byte)
{
    uint8_t count = 0;

    byte = ~byte;
    while (byte) {
        byte &= byte - 1;
        ++count;
    }
    return count;
}

int fsm_init_ops(fsm_dev_t *fsm_dev, uint8_t dev_id)
{
    if (fsm_dev == NULL) {
        return -EINVAL;
    }
    switch(dev_id) {
        case FS1601S_DEV_ID:
            #ifdef CONFIG_FSM_FS1601S
            fs1601s_ops(fsm_dev);
            fsm_dev->preset_name = "fs1601s_fw.fsm";
            pr_info("%s fs1601s detected!", __func__);
            #endif
            break;
        case FS1603_DEV_ID:
            #ifdef CONFIG_FSM_FS1603
            fs1603_ops(fsm_dev);
            fsm_dev->preset_name = "fs1603_fw.fsm";
            pr_info("%s fs1603 detected!", __func__);
            #endif
            break;
        case FS1818_DEV_ID:
            #ifdef CONFIG_FSM_FS1818
            fs1818_ops(fsm_dev);
            fsm_dev->preset_name = "fs1818_fw.fsm";
            pr_info("%s fs1818 detected!", __func__);
            #endif
            break;
        case FS1860_DEV_ID:
            #ifdef CONFIG_FSM_FS1860
            fs1860_ops(fsm_dev);
            fsm_dev->preset_name = "fs1860_fw.fsm";
            pr_info("%s fs1860 detected!", __func__);
            #endif
            break;
        default:
            pr_err("Unsupported Device: 0x%04x", dev_id);
            return -EINVAL;
    }

    return 0;
}

int fsm_probe(fsm_dev_t *fsm_dev)
{
    uint16_t id = 0;
    int ret = 0;

    if (fsm_dev == NULL)
        return -EINVAL;

    pr_info("%s enter!\n", __func__);
    ret = fsm_reg_read(fsm_dev, 0x03, &id);
    pr_info("%s device id: %04x", __func__, id);

    ret |= fsm_init_ops(fsm_dev, ((id >> 8) & 0xff));
    if (ret)
        return ret;

    fsm_dev->config = fsm_get_config();
    fsm_dev->ram_scene[FSM_EQ_RAM0] = FSM_SCENE_UNKNOW;
    fsm_dev->ram_scene[FSM_EQ_RAM1] = FSM_SCENE_UNKNOW;
    fsm_dev->own_scene = FSM_SCENE_UNKNOW;
    fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
    fsm_dev->version = id;
    fsm_dev->calib_data.store_otp = 1;
    fsm_dev->calib_data.preval = 0;
    fsm_dev->calib_data.minval = 0;
    fsm_dev->calib_data.count = 0;
    fsm_dev->calib_data.zmdata = 0;
    fsm_dev->zmdata_done = 0;
    fsm_dev->dev_state = FSM_STATE_UNKNOWN;
    fsm_dev->dev_mask = (uint8_t)(BIT(fsm_dev->i2c_addr - 0x34) & 0xff);

    return ret;
}

void fsm_remove(fsm_dev_t *fsm_dev)
{
    if (fsm_dev) {
        fsm_free_mem(fsm_dev->preset);
        fsm_dev->preset = NULL;
    }
}

int fsm_get_state(fsm_dev_t *fsm_dev, int type)
{
    return ((fsm_dev->dev_state & type) != 0);
}

int fsm_init(fsm_dev_t *fsm_dev, int force)
{
    int ret = 0;

    pr_info("%s force: %d, state: %X +\n", __func__, force, fsm_dev->dev_state);
    if ((fsm_dev->dev_state & STATE(FW_INITED)) == 0)
        return -EINVAL;

    if (!force && (fsm_dev->dev_state & STATE(DEV_INITED)))
        return 0;

    fsm_dev->dev_state &= ~(STATE(DEV_INITED));
    fsm_dev->ram_scene[FSM_EQ_RAM0] = FSM_SCENE_UNKNOW;
    fsm_dev->ram_scene[FSM_EQ_RAM1] = FSM_SCENE_UNKNOW;
    fsm_dev->cur_scene = FSM_SCENE_UNKNOW;
    if (fsm_dev->dev_ops.init_register) {
        ret = fsm_dev->dev_ops.init_register(fsm_dev);
        if (!ret) {
            fsm_dev->dev_state |= STATE(DEV_INITED);
        }
    }
    if (fsm_dev->dev_ops.check_otp) {
        ret = fsm_dev->dev_ops.check_otp(fsm_dev);
        if (!ret) {
            fsm_dev->dev_state |= STATE(CALIB_DONE);
        }
    }
    pr_info("%s updated state: %X\n", __func__, fsm_dev->dev_state);

    return ret;
}

int fsm_switch_preset(fsm_dev_t *fsm_dev, int force)
{
    fsm_config_t *config = fsm_get_config();
    int next_scene = config->next_scene;
    //uint8_t flags = config->flags;
    int ret = 0;

    pr_info("scene cur: %04x, next: %04x\n", fsm_dev->cur_scene, next_scene);
	 if(fsm_dev->cur_scene == next_scene) {
		pr_info("no need to switch same scene\n");
		return 0;
	 }

    if (!force && fsm_get_state(fsm_dev, STATE(AMP_ON)))
        return 0;

    if (fsm_dev->dev_ops.switch_preset)
        ret = fsm_dev->dev_ops.switch_preset(fsm_dev, force);

    return ret;
}

uint16_t fsm_get_zmdata(fsm_dev_t *fsm_dev)
{
    uint16_t zmdata = 0;
    int ret = 0;

    ret = fs1603_status_check(fsm_dev);
    if(ret)
        return 0;

    ret = fsm_reg_read(fsm_dev, 0xbb, &zmdata);
    fsm_dev->calib_data.zmdata = zmdata;

    return ret ? 0xffff : zmdata;
}

int fsm_calib_zmdata(fsm_dev_t *fsm_dev)
{
    struct fsm_calib_data *calib_data = &fsm_dev->calib_data;
    uint16_t preval = calib_data->preval;
    uint16_t count = calib_data->count;
    uint16_t zmdata = 0;
    int ret = 0;

    if (count >= 10)
        return (int)count;

    ret = fsm_reg_read(fsm_dev, 0xBB, &zmdata);
    if (ret)
        return ret;

    if (zmdata == 0 || zmdata == 0xffff)
        return 0;

    if (!preval || abs(preval - zmdata) > FSM_ZMDELTA_MAX) {
        calib_data->preval = zmdata;
        calib_data->count = 1;
        calib_data->minval = zmdata;
    } else {
        calib_data->count++;
        if (zmdata < calib_data->minval) {
            calib_data->minval = zmdata;
        }
    }

    if (calib_data->count >= 10)
        pr_info("%s [0x%02x] get zmdata(0x%04x) done!\n", __func__, fsm_dev->i2c_addr, calib_data->minval);

    return (int)calib_data->count;
}
int fsm_normal_cmd_parse(fsm_dev_t *fsm_dev, int cmd, int argv, int calib_done)
{
    fsm_config_t *config = fsm_dev->config;
    int ret = -EINVAL;
    int volume = 0xff;

    pr_info("%s enter.", __func__);
    switch (cmd) {
        case FSM_CMD_SWH_PRESET:
            ret = fsm_switch_preset(fsm_dev, argv);
            return ret;
        case FSM_CMD_START_UP:
            if (fsm_dev->dev_ops.start_up)
                ret = fsm_dev->dev_ops.start_up(fsm_dev);
            return ret;
        case FSM_CMD_SHUT_DOWN:
            if (fsm_dev->dev_ops.shut_down)
                ret = fsm_dev->dev_ops.shut_down(fsm_dev);
            return ret;
        case FSM_CMD_SET_MUTE:
            if (calib_done)
                volume = config->volume;

            if (fsm_dev->dev_ops.set_mute) {
                ret = fsm_dev->dev_ops.set_mute(fsm_dev, argv, volume);

                if (!ret && argv == FSM_UNMUTE)
                    fsm_dev->dev_state |= STATE(AMP_ON);
                else
                    fsm_dev->dev_state &= ~(STATE(AMP_ON));
            }
            return ret;
        case FSM_CMD_PARSE_FIRMW:
            return fsm_parse_firmware(fsm_dev, fw_cont, fw_size);
        case FSM_CMD_INIT:
            return fsm_init(fsm_dev, argv);
        case FSM_CMD_WAIT_STABLE:
            if (fsm_dev->dev_ops.wait_stable)
                ret = fsm_dev->dev_ops.wait_stable(fsm_dev, argv);
            return ret;
        case FSM_CMD_F0_STEP1:
            fsm_dev->f0_data.f0 = 0;
            fsm_dev->f0_data.freq = 0;
            fsm_dev->f0_data.gain = 0;
            fsm_dev->errcode = 0;
            if (fsm_dev->dev_ops.f0_step1)
                ret = fsm_dev->dev_ops.f0_step1(fsm_dev);
            return ret;
        case FSM_CMD_F0_STEP2:
            fsm_dev->errcode = 0;
            if (fsm_dev->dev_ops.f0_step2)
                ret = fsm_dev->dev_ops.f0_step2(fsm_dev);
            return ret;
        case FSM_CMD_GET_ZMDATA:
            return fsm_get_zmdata(fsm_dev);
        case FSM_CMD_SET_BYPASS:
            if (fsm_dev->dev_ops.dsp_bypass)
                ret = fsm_dev->dev_ops.dsp_bypass(fsm_dev, argv);
            return ret;
        case FSM_CMD_DUMP_REG:
            return fsm_reg_dump(fsm_dev);
        default:
            break;
    }
    pr_info("exit.");

    return ret;

}

int fsm_calib_cmd_parse(fsm_dev_t *fsm_dev, int cmd, int argv, int calib_done)
{
    int ret = -EINVAL;

    if (!argv /* force */ && calib_done) {
        pr_info("%s already calibrated\n", __func__);
        return 0;
    }
    switch (cmd) {
        case FSM_CMD_CALIB_STEP1:
            fsm_dev->errcode = 0;
            fsm_hal_clear_calib_data(fsm_dev);
            fsm_dev->dev_state &= ~(STATE(CALIB_DONE));
            if (fsm_dev->dev_ops.calib_step1) {
                ret = fsm_dev->dev_ops.calib_step1(fsm_dev, argv);
            }
            break;
        case FSM_CMD_CALIB_STEP2:
            if (fsm_dev->dev_ops.calib_step2) {
                ret = fsm_dev->dev_ops.calib_step2(fsm_dev);
            }
            break;
        case FSM_CMD_CALIB_STEP3:
            if (fsm_dev->dev_ops.calib_step3) {
                ret = fsm_dev->dev_ops.calib_step3(fsm_dev,
                        fsm_dev->calib_data.store_otp);
                if (!ret) {
                    pr_info("%s [0x%02x] calibrate success!\n", __func__, fsm_dev->i2c_addr);
                    fsm_dev->dev_state |= STATE(CALIB_DONE);
                    fsm_dev->calib_data.store_otp = 0;
                    if(fsm_dev->dev_ops.set_mute) {
                        ret = fsm_dev->dev_ops.set_mute(fsm_dev, FSM_UNMUTE, 0xff);
                    }
                }
            }
            break;
        default:
            break;
    }

    return ret;
}

int fsm_set_cmd(fsm_dev_t *fsm_dev, int cmd, int argv)
{
    fsm_config_t *config = fsm_dev->config;
    int ret = -EINVAL;
    int calib_done = 0;

    pr_info("%s enter.", __func__);

    if(cmd == FSM_CMD_PARSE_FIRMW && fsm_dev != NULL) {
	pr_info("parse firmware.");
        ret = fsm_normal_cmd_parse(fsm_dev, cmd, argv, calib_done);
        return ret;
    }

    if (fsm_dev == NULL || fsm_dev->dev_list == NULL || (cmd < 0 || cmd >= FSM_CMD_MAX))
        return -EINVAL;

    if (((fsm_dev->own_scene & config->next_scene) == 0)) {
        pr_info("%s [0x%02x] no found scene: %04X\n", __func__,
                fsm_dev->i2c_addr, config->next_scene);
        fsm_dev->cur_scene = config->next_scene;
        return 0;
    }

    pr_info("%s [0x%02x] cmd: %d, state: %X, argv = %d\n", __func__,
            fsm_dev->i2c_addr, cmd, fsm_dev->dev_state, argv);

    if ((cmd > FSM_CMD_INIT) && (!fsm_get_state(fsm_dev, STATE(DEV_INITED)))){
        return -EINVAL;
        }

    if (cmd > FSM_CMD_INIT && !(fsm_dev->dev_mask & config->dev_sel)) {
        pr_info("%s not select device [0x%02x]\n", __func__, fsm_dev->i2c_addr);
        return 0;
    }

    calib_done = fsm_get_state(fsm_dev, STATE(CALIB_DONE));

    if (cmd != FSM_CMD_CALIB_STEP1 && cmd != FSM_CMD_CALIB_STEP2
       && cmd != FSM_CMD_CALIB_STEP3) {
        return fsm_normal_cmd_parse(fsm_dev, cmd, argv, calib_done);
    }

    ret = fsm_calib_cmd_parse(fsm_dev, cmd, argv, calib_done);
    pr_info("exit.");

    return ret;
}
