/**
 * Copyright (c)Foursemi Co., Ltd. 2018-2019. All rights reserved.
 * Description:  Fs1894 Devices Ops.
 * uthor: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */
//#include <math.h>
//#include <stdlib.h>
//#include <stdint.h>
//#include <string.h>
//#include <unistd.h>
//#include <errno.h>
//#include <fcntl.h>
//#include <sys/ioctl.h>
#include "fsm_public.h"
#include "fs1603.h"
#include "fsm_dev.h"
//#include "fsm_hal.h"

#define FS1603_OTP_BST_CFG    0xB9DE
enum fsm1603_info_type {
    FS1603_INFO_SPK_TMAX = 0,
    FS1603_INFO_SPK_TEMPR_COEF,
    FS1603_INFO_SPK_TEMPR_SEL,
    FS1603_INFO_SPK_RES,
    FS1603_INFO_SPK_RAPP,
    FS1603_INFO_SPK_TRE,
    FS1603_INFO_SPK_TM6,
    FS1603_INFO_SPK_TM24,
    FS1603_INFO_SPK_TERR,
    FS1603_INFO_SPK_MAX,
};

/**
 * registers of reg table as below:
 * common(0xff):
 * SPKMDB1,SPKMDB2,SPKMDB3,
 * scene(scene bit):
 * BFLCTRL,BFLSET,SQC,AGC,DRPARA,ACSDRC,ACSDRCS,ACSDRCSE,ADPBST,BSTCTRL,I2SCTRL
 */

#define REG(addr)               FS1603_##addr
#define INFO(tag)               FS1603_INFO_##tag
#define FS1603_PRESET_RAM2_ADDR 0x0090
#define FS1603_RS2RL_RATIO      (2700)
#ifdef FSM_FACTORY_MODE
#define FS1603_OTP_COUNT_MAX    (4)
#else
#define FS1603_OTP_COUNT_MAX    (6)
#endif

const static fsm_pll_config_t g_fs1603_pll_tbl[] = {
    /* bclk,    0xC1,   0xC2,   0xC3 */
    {  256000, 0x0260, 0x0540, 0x0001 }, //  8000*16*2
    {  512000, 0x0260, 0x0540, 0x0002 }, // 16000*16*2 &  8000*32*2
    { 1024000, 0x0260, 0x0540, 0x0004 }, //         & 16000*32*2
    // 32kHz+16bit: same as 48000*16*2, need register config
    { 1024032, 0x0260, 0x0540, 0x0006 }, // 32000*16*2+32
    { 1411200, 0x0260, 0x0460, 0x0005 }, // 44100*16*2
    { 1536000, 0x0260, 0x0540, 0x0006 }, // 48000*16*2
    // 32kHz+32bit: same as 48000*32*2, need register config
    { 2048032, 0x0260, 0x0540, 0x000c }, //         & 32000*32*2+32
    { 2822400, 0x0260, 0x0460, 0x000a }, //         & 44100*32*2
    { 3072000, 0x0260, 0x0540, 0x000c }, //         & 48000*32*2
};

int fs1603_get_scenes(fsm_dev_t *fsm_dev);
int fs1603_wait_stable(fsm_dev_t *fsm_dev, int type);
int fs1603_switch_preset(fsm_dev_t *fsm_dev, int force);
void convert_data_to_bytes(uint32_t val, uint8_t *buf)
{
    uint16_t temp = 0;

    temp = val & 0xFFFF;
    buf[0] = ((temp) >> 8) & 0xFF;
    buf[1] = temp & 0xFF;
    temp = (val >> 16) & 0xFF;
    buf[2] = (temp >> 8) & 0xFF;
    buf[3] = temp & 0xFF;
}

static void *fs1603_get_list_by_idx(fsm_dev_t *fsm_dev, int idx)
{
    dev_list_t *dev_list = fsm_dev->dev_list;
    fsm_index_t *index = &dev_list->index[0];
    uint8_t *pdata = (uint8_t *)index;

    return (void *)(&pdata[index[idx].offset]);
}

static void *fs1603_get_list(fsm_dev_t *fsm_dev, int type)
{
    dev_list_t *dev_list = fsm_dev->dev_list;
    fsm_index_t *index = NULL;
    int i = 0;

    for (i = 0; i < dev_list->len; i++) {
        index = &dev_list->index[i];
        if (index->type == type) {
            break;
        }
    }
    if (i >= dev_list->len)
        return NULL;

    return fs1603_get_list_by_idx(fsm_dev, i);
}

static int fs1603_get_info(fsm_dev_t *fsm_dev, uint16_t info_type)
{
    info_list_t *info = NULL;

    info = (info_list_t *)fs1603_get_list(fsm_dev, FSM_DSC_SPK_INFO);
    if (!info)
        return -EINVAL;

    if (info_type > info->len)
        return -EINVAL;

    return info->data[info_type];
}

static int fs1603_set_spkset(fsm_dev_t *fsm_dev)
{
    union REG_SPKSET spkset;
    uint16_t spkr = 0;
    int ret = 0;

    spkr = fs1603_get_info(fsm_dev, INFO(SPK_RES));
    if(spkr <= 0 || spkr > 32)
    {
        pr_err("%s wrong spkr: %d\n", __func__, spkr);
        return -EINVAL;
    }
    fsm_dev->spkr = spkr;

    ret = fsm_reg_read(fsm_dev, REG(SPKSET), &spkset.value);
    switch (spkr) {
        case 4:
            spkset.bit.SPKR = 0;
            break;
        case 6:
            spkset.bit.SPKR = 1;
            break;
        case 7:
            spkset.bit.SPKR = 2;
            break;
        case 8:
        default:
            spkset.bit.SPKR = 3;
            break;
    }
    ret |= fsm_reg_write(fsm_dev, REG(SPKSET), spkset.value);

    return ret;
}

static int fs1603_write_stereo_ceof(fsm_dev_t *fsm_dev)
{
    stereo_coef_t *stereo_coef = NULL;
    int i = 0, len = 0;
    int ret = 0;

    stereo_coef = (stereo_coef_t *)fs1603_get_list(fsm_dev, FSM_DSC_STEREO_COEF);
    if (!stereo_coef)
        return -EINVAL;

    len = stereo_coef->len;
    pr_info("%s stereo coef len: %d\n", __func__, len);
    for (i = 0; i < len - 2; i++) {
        ret |= fsm_reg_write(fsm_dev, REG(STERC1) + i, stereo_coef->data[i]);
    }
    ret |= fsm_reg_write(fsm_dev, REG(STERCTRL), stereo_coef->data[i++]);
    ret |= fsm_reg_write(fsm_dev, REG(STERGAIN), stereo_coef->data[i++]);

    return ret;
}

static int fs1603_write_excer_ram(fsm_dev_t *fsm_dev)
{
    ram_data_t *excer_ram = NULL;
    uint8_t write_buf[4] = {0};
    int i = 0, len = 0, ret = 0;

    excer_ram = fs1603_get_list(fsm_dev, FSM_DSC_EXCER_RAM);
    if (!excer_ram)
        return -EINVAL;

    len = excer_ram->len;
    pr_info("%s excer ram len: %d\n", __func__, len);
    ret = fsm_reg_write(fsm_dev, REG(DACEQA), 0);
    for (i = 0; i < len; i++) {
        convert_data_to_bytes(excer_ram->data[i], write_buf);
        ret |= fsm_burst_write(fsm_dev, REG(DACEQWL), write_buf, 4);
    }

    return ret;
}

static int fs1603_write_reg_tbl(fsm_dev_t *fsm_dev, uint16_t scene)
{
    reg_scene_t *reg_scene = NULL;
    reg_comm_t *reg_comm = NULL;
    reg_unit_t *reg = NULL;
    regs_unit_t *regs = NULL;
    int i = 0, len = 0;
    int ret = 0;

    if (scene == FSM_SCENE_COMMON) {
        reg_comm = fs1603_get_list(fsm_dev, FSM_DSC_REG_COMMON);
        if (!reg_comm)
            return 0;
        len = reg_comm->len;
        reg = reg_comm->reg;
        pr_info("%s reg comm len: %d\n", __func__, len);
        for (i = 0; i < len; i++) {
            ret |= fsm_reg_wmask(fsm_dev, &reg[i]);
        }
    } else if (scene != 0) {
        reg_scene = fs1603_get_list(fsm_dev, FSM_DSC_REG_SCENES);
        if (!reg_scene)
            return 0;

        len = reg_scene->len;
        regs = reg_scene->regs;
        pr_info("%s reg scene len: %d\n", __func__, len);
        for (i = 0; i < len; i++) {
            if ((regs[i].scene & scene) == 0) {
                continue;
            }
            ret |= fsm_reg_wmask(fsm_dev, &regs[i].reg);
        }
    }

    return ret;
}

static int fs1603_write_preset_eq(fsm_dev_t *fsm_dev, int ram_id, uint16_t scene)
{
    dev_list_t *dev_list = fsm_dev->dev_list;
    preset_list_t *preset_list = NULL;
    fsm_index_t *index = NULL;
    uint8_t write_buf[4] = {0};
    uint16_t ram_scene = fsm_dev->ram_scene[ram_id];
    int i = 0, ret = 0, len = 0;

    if (ram_scene != 0xff && (ram_scene & scene) != 0) {
        pr_info("%s already wroten scene eq: %04x\n", __func__, scene);
        return 0;
    }

    index = dev_list->index;
    for (i = 0; i < dev_list->len; i++) {
        if (index[i].type != FSM_DSC_PRESET_EQ) {
            continue;
        }
        preset_list = (preset_list_t *)fs1603_get_list_by_idx(fsm_dev, i);
        if (preset_list->scene & scene) {
            break;
        }
    }

    //for npreset
 
    if (!preset_list || i >= dev_list->len) {
        if(scene == FSM_SCENE_VOICE){
            pr_info("found no voice EQ, use music EQ");
            scene = FSM_SCENE_MUSIC;
            for (i = 0; i < dev_list->len; i++) {
                if (index[i].type != FSM_DSC_PRESET_EQ) {
                    continue;
                }
                preset_list = (preset_list_t *)fs1603_get_list_by_idx(fsm_dev, i);
                if (preset_list->scene & scene) {
                    preset_list->scene |= FSM_SCENE_VOICE;
                    break;
                }
            }
        } 
        else {
            pr_info("found no scene %d", scene);
            return -EINVAL;
        }
    }

    len = preset_list->len;
    pr_info("%s preset eq len: %d\n", __func__, len);
    ret = fsm_reg_write(fsm_dev, REG(ACSEQA), (ram_id == 0) ? 0x0 : FS1603_PRESET_RAM2_ADDR);
    for (i = 0; i < len; i++) {
        convert_data_to_bytes(preset_list->data[i], write_buf);
        ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), write_buf, 4); // write 4 bytes each time
    }
    if (!ret) {
        fsm_dev->ram_scene[ram_id] = preset_list->scene;
        pr_info("%s update ram_scene[%d]: %04x\n", __func__, ram_id, preset_list->scene);
    }

    return ret;

}

static int fs1603_write_preset(fsm_dev_t *fsm_dev)
{
    union REG_SYSCTRL sysctrl;
    uint16_t temp = 0;
    int ret = 0, ready = 0;

    pr_info("%s enter\n", __func__);

    if (fsm_dev->own_scene == 0) {
        pr_err("%s not found any scene!\n", __func__);
        return -EINVAL;
    }

    ret = fsm_reg_read(fsm_dev, REG(SYSCTRL), &sysctrl.value);
    temp = sysctrl.value;
    sysctrl.bit.AMPE = 0;
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);
    sysctrl.value = temp;

    ret |= fs1603_set_spkset(fsm_dev);
    ret |= fsm_reg_write(fsm_dev, 0x08, (fs1603_get_info(fsm_dev, INFO(SPK_TEMPR_COEF)) << 1));
    ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
    if (!ready) {
        pr_err("%s wait timeout!\n", __func__);
        return -1;
    }

    ret |= fsm_access_key(fsm_dev, 1);
    ret |= fs1603_write_stereo_ceof(fsm_dev);
    ret |= fs1603_write_excer_ram(fsm_dev);
    // use music and voice scene config as default
    ret |= fs1603_write_preset_eq(fsm_dev, FSM_EQ_RAM0, FSM_SCENE_MUSIC);
    ret |= fs1603_write_preset_eq(fsm_dev, FSM_EQ_RAM1, FSM_SCENE_VOICE);
    ret |= fsm_reg_write(fsm_dev, REG(ACSCTRL), 0x9880);
    ret |= fs1603_write_reg_tbl(fsm_dev, FSM_SCENE_COMMON);
    ret |= fsm_access_key(fsm_dev, 0);
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), 0);

    ret |= fs1603_switch_preset(fsm_dev, 1);

    pr_info("%s exit, ret = %d.\n", __func__, ret);

    return ret;
}

static int fs1603_config_pll(fsm_dev_t *fsm_dev, int on)
{
    fsm_config_t *cfg = fsm_dev->config;
    union REG_PLLCTRL4 pllc4;
    int idx = 0, ret = 0, size = 0;

    // config pll need disable pll first
    ret = fsm_reg_write(fsm_dev, REG(PLLCTRL4), 0);
    ret |= fsm_access_key(fsm_dev, 1);
    if (on == FSM_PLL_OFF){
        if (cfg->srate == 32000) {
            pr_info("MTK voice mode pll off\n");
            ret |= fsm_reg_write(fsm_dev, REG(ANACTRL), 0x0100);
        }
        ret |= fsm_reg_write(fsm_dev, 0xD3, 0x0168);
        ret |= fsm_access_key(fsm_dev, 0);
        return 0;
    }

    size = sizeof(g_fs1603_pll_tbl)/ sizeof(fsm_pll_config_t);
    for (idx = 0; idx < size; idx++) {
        if (g_fs1603_pll_tbl[idx].bclk == cfg->bclk) {
            break;
        }
    }
    pr_info("idx: %d, bclk: %d\n", idx, cfg->bclk);
    if (idx >= size) {
        pr_info("Not found bclk: %d, rate: %d\n", cfg->bclk, cfg->srate);
        return -EINVAL;
    }

    if((fsm_dev->cur_scene & FSM_SCENE_VOICE) || (fsm_dev->cur_scene & FSM_SCENE_EARPIECE)) {
        pr_err("in voice mode\n");
        ret |= fsm_reg_write(fsm_dev, 0xD3, 0x0068);
        if (cfg->srate == 32000) {
            pr_info("MTK voice mode pll on\n");
            ret |= fsm_reg_write(fsm_dev, REG(ANACTRL), 0x0101);
        }
    }
    ret |= fsm_access_key(fsm_dev, 0);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL1), g_fs1603_pll_tbl[idx].c1);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL2), g_fs1603_pll_tbl[idx].c2);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL3), g_fs1603_pll_tbl[idx].c3);

    pllc4.bit.PLLEN = 1;
    pllc4.bit.OSCEN = 1;
    pllc4.bit.ZMEN = 1;
    pllc4.bit.VBGEN = 1;
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), pllc4.value);

    return ret;
}

static int fs1603_config_i2s(fsm_dev_t *fsm_dev)
{
    union REG_I2SCTRL i2sctrl;
    fsm_config_t *cfg = fsm_dev->config;
    int ret = 0;

    ret = fsm_reg_read(fsm_dev, REG(I2SCTRL), &i2sctrl.value);
    switch (cfg->srate) {
        case 8000: // sample rate 8000
            i2sctrl.bit.I2SSR = FSM_RATE_08000;
            break;
        case 16000: // sample rate 16000
            i2sctrl.bit.I2SSR = FSM_RATE_16000;
            break;
        case 44100: // sample rate 44100
            i2sctrl.bit.I2SSR = FSM_RATE_44100;
            break;
        case 32000: // sample rate 32000
            i2sctrl.bit.I2SSR = FSM_RATE_48000;
            break;
        case 48000: // sample rate 48000
            i2sctrl.bit.I2SSR = FSM_RATE_48000;
            break;
        default:
            i2sctrl.bit.I2SSR = FSM_RATE_48000;
            break;
    }
    i2sctrl.bit.I2SF = FSM_FMT_I2S;
    pr_info("%s I2SSR: %d, val: 0x%04x\n", __func__, i2sctrl.bit.I2SSR, i2sctrl.value);
    ret |= fsm_reg_write(fsm_dev, REG(I2SCTRL), i2sctrl.value);

    return ret;
}

static int fs1603_set_tsctrl(fsm_dev_t *fsm_dev, int enable, int auto_off)
{
    union REG_TSCTRL tsctrl;
    int ret = 0;

    ret = fsm_reg_read(fsm_dev, REG(TSCTRL), &tsctrl.value);
    tsctrl.bit.EN = enable;
    tsctrl.bit.OFF_AUTOEN = auto_off;
    ret |= fsm_reg_write(fsm_dev, REG(TSCTRL), tsctrl.value);

    return ret;
}

static int fs1603_power_on(fsm_dev_t *fsm_dev, bool enable)
{
    union REG_SYSCTRL sysctrl;
    int ret = 0;

    ret = fsm_reg_read(fsm_dev, REG(SYSCTRL), &sysctrl.value);
    if (ret)
        return ret;

    sysctrl.bit.PWDN = 0;
    if (!enable) {
        sysctrl.bit.AMPE = 0;
        sysctrl.bit.PWDN = 1;
    }
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);

    return ret;
}

static void fs1603_parse_otp(fsm_dev_t *fsm_dev, uint16_t value, int *count, int *re25)
{
    uint8_t byte = 0;
    uint8_t byte7bit = 0;
    int step_unit = 0;
    int offset = 0;
    int cal_count = 0;

    // parse re25
    if (re25 != NULL) {
        byte = (uint8_t)(value >> 8) & 0xff;
        byte7bit = byte & 0x7f;
        step_unit = ((fsm_dev->spkr * FSM_SPKR_ALLOWANCE) << 8) / 3175; // calc step
        offset = byte7bit * step_unit;
        if ((byte & 0x80) != 0)
            *re25 = (fsm_dev->spkr << 10) - offset;
        else
            *re25 = (fsm_dev->spkr << 10) + offset;

        pr_info("%s offset: %d, step: %d, re25: %d mohm\n", __func__, offset, step_unit, *re25);
    }

    // parse calibration count
    if (count != NULL) {
        byte = value & 0x00ff;
        cal_count = count_bit0_number(byte);
        pr_info("%s count: %d\n", __func__, cal_count);
        *count = cal_count;
        if (cal_count > 0)
            fsm_dev->dev_state |= STATE(OTP_STORED);
        else
            fsm_dev->dev_state &= ~(STATE(OTP_STORED));
    }
    pr_info("%s state\t: %04x, cal_count %d\n",__func__, fsm_dev->dev_state,cal_count);
}

uint8_t fs1603_re25_to_byte(fsm_dev_t *fsm_dev, int re25)
{
    int diff_val = 0, step_unit = 0;
    uint8_t re25_byte = 0, offset = 0;

    if (re25 == 0)
        return 0xff;

    diff_val = re25 - (fsm_dev->spkr << 10);
    step_unit = ((fsm_dev->spkr * FSM_SPKR_ALLOWANCE) << 8) / 3175; // calc step
    if (diff_val < 0) {
        re25_byte = 0x80;
        diff_val *= -1;
    }
    offset = (uint8_t)((diff_val / step_unit) & 0xff);
    if (offset > 0x7f)
        offset = 0x7f;

    re25_byte |= offset;
    pr_info("%s re25: %d mohm, step: %d, offset: %d\n", __func__, re25, step_unit, offset);

    return re25_byte;
}

static int fs1603_write_otp(fsm_dev_t *fsm_dev, uint8_t re25_byte)
{
    int ret = 0, count_new = 0, ready = 0, re25_new = 0;
    uint16_t otprdata = 0;

    ret = fsm_reg_write(fsm_dev, REG(OTPADDR), 0x0010);
    ret |= fsm_reg_write(fsm_dev, REG(OTPWDATA), (uint16_t)re25_byte);
    ret |= fsm_reg_write(fsm_dev, REG(OTPCMD), 0x0002);
    ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_OTP_READY);
    if (!ready) {
        pr_err("%s wait OTP ready failed!\n", __func__);
        return -EINVAL;
    }
    ret |= fsm_reg_write(fsm_dev, REG(OTPADDR), 0x0010);
    ret |= fsm_reg_write(fsm_dev, REG(OTPCMD), 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(OTPCMD), 0x0001);
    ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_OTP_READY);
    if (!ready) {
        pr_err("%s wait OTP ready failed!\n", __func__);
        return -EINVAL;
    }
    ret |= fsm_reg_read(fsm_dev, REG(OTPRDATA), &otprdata);
    if (((otprdata >> 8) & 0xff) != re25_byte) {
        pr_err("%s read back failed: 0x%04x(expect: 0x%04x)\n", __func__, otprdata, re25_byte);
        return -EINVAL;
    }
    fs1603_parse_otp(fsm_dev, otprdata, &count_new, &re25_new);
    pr_info("%s read back count: %d\n", __func__, count_new);

    return ret;
}

static int fs1603_store_otp(fsm_dev_t *fsm_dev, uint8_t re25_byte, int store)
{
    int ready = 0, count = 0, delta = 0, re25_old = 0, re25_new = 0, ret = 0;
    uint16_t pllctrl4 = 0, bstctrl = 0, otprdata = 0;

    ret = fsm_access_key(fsm_dev, 1);

    ret |= fsm_reg_read(fsm_dev, REG(BSTCTRL), &bstctrl);
    ret |= fsm_reg_write(fsm_dev, REG(BSTCTRL), FS1603_OTP_BST_CFG);
    ret |= fsm_reg_read(fsm_dev, REG(PLLCTRL4), &pllctrl4);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), 0x000f);

    ret |= fsm_reg_write(fsm_dev, REG(OTPADDR), 0x0010);
    ret |= fsm_reg_write(fsm_dev, REG(OTPCMD), 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(OTPCMD), 0x0001);

    do {
        ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_OTP_READY);
        if (!ready) {
            pr_err("%s wait OTP ready failed!\n", __func__);
            ret = -EINVAL;
            break;
        }
        ret |= fsm_reg_read(fsm_dev, REG(OTPRDATA), &otprdata);
        fs1603_parse_otp(fsm_dev, otprdata, &count, &re25_old);
        re25_new = fsm_dev->calib_data.calib_re25;
        pr_info("%s re25 old: %d, new: %d\n", __func__, re25_old, re25_new);
        delta = abs(re25_old - re25_new);
        if (count > 0 && (delta < (fsm_dev->spkr << 10) / IMPEDANCE_JITTER_RATIO)) { // 10bit shift
            pr_info("%s no need to update otp, delta: %d\n", __func__, delta);
            break;
        }
        if (count >= FS1603_OTP_COUNT_MAX || !store) {
            pr_err("%s not store(%d) otp or count exceeds max: %d\n", __func__, store, count);
            break;
        }
        ret |= fs1603_write_otp(fsm_dev, re25_byte);
    }
    while (0);

    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), pllctrl4);
    ret |= fsm_reg_write(fsm_dev, REG(BSTCTRL), bstctrl);
    ret |= fsm_access_key(fsm_dev, 0);

    return ret;
}

static uint16_t fs1603_cal_threshold(fsm_dev_t *fsm_dev, int mt_type, uint16_t zmdata)
{
    uint16_t threshold = 0, mt_tempr = 0;
    int tmax = 0, tcoef = 0, tsel = 0;

    mt_tempr = fs1603_get_info(fsm_dev, mt_type);
    pr_info("%s MT[%d]: %d\n", __func__, mt_type, mt_tempr);
    if (mt_tempr <= 0)
        return 0;

    tmax = fs1603_get_info(fsm_dev, INFO(SPK_TMAX));
    tcoef = fs1603_get_info(fsm_dev, INFO(SPK_TEMPR_COEF));
    tsel = fs1603_get_info(fsm_dev, INFO(SPK_TEMPR_SEL));
    if (tsel <= 0 || tcoef < 0 || tmax < 0) {
        pr_err("%s get speaker info failed!\n", __func__);
        return 0;
    }
    threshold = (uint16_t)((uint32_t)zmdata * FSM_MAGNIF_TEMPR_COEF /
        (FSM_MAGNIF_TEMPR_COEF + tcoef * (mt_tempr * tmax / 100 - (tsel >> 1))));

    return threshold;
}

static int fs1603_set_threshold(fsm_dev_t *fsm_dev, uint16_t data, int data_type)
{
    int calib_min = 0, calib_max = 0, re25 = 0, ret = 0;
    uint16_t zmdata = 0, value = 0;
    uint16_t threshold = 0;
    int zmdata_defalut = 0xffff;
    int ratio_min = 0x0b33;
    int ratio_max = 0x14cc;

    ret = fsm_reg_read(fsm_dev, REG(OTPPG1W2), &value);
    if (value == 0) {
        pr_warning("OTPPG1W2 unexpected!, use default value\n");
        value = FSM_RS_TRIM_DEFAULT;
    }
    value &= 0xff;

    if (data_type == FSM_DATA_TYPE_ZMDATA) {
        re25 = (data == 0) ? zmdata_defalut : ((int)value * FS1603_RS2RL_RATIO << 10) / data; // 10 bit shift
        zmdata = data;
        fsm_dev->calib_data.calib_re25 = re25;
    } else {
        zmdata = (data == 0) ? zmdata_defalut : ((int)value * FS1603_RS2RL_RATIO << 10) / data & zmdata_defalut; // 10 bit shift
        re25 = data;
    }
    pr_info("%s zm: %04x, re25: %d\n", __func__, zmdata, re25);

    // check re25
    calib_min = (ratio_min * fsm_dev->spkr) >> 2; // 2 bit shift
    calib_max = (ratio_max * fsm_dev->spkr) >> 2; // 2 bit shift
    pr_info("%s spkr: %d, min: %d, max: %d\n", __func__,
        fsm_dev->spkr, calib_min, calib_max);
    if (re25 < calib_min || re25 > calib_max) {
        pr_err("%s invalid re25: %d\n", __func__, re25);
        ret = -EINVAL;
    } else {
        threshold = fs1603_cal_threshold(fsm_dev, INFO(SPK_TRE), zmdata);
        ret |= fsm_reg_write(fsm_dev, REG(SPKRE), threshold);
        threshold = fs1603_cal_threshold(fsm_dev, INFO(SPK_TM6), zmdata);
        ret |= fsm_reg_write(fsm_dev, REG(SPKM6), threshold);
        threshold = fs1603_cal_threshold(fsm_dev, INFO(SPK_TM24), zmdata);
        ret |= fsm_reg_write(fsm_dev, REG(SPKM24), threshold);
        threshold = fs1603_cal_threshold(fsm_dev, INFO(SPK_TERR), zmdata);
        ret |= fsm_reg_write(fsm_dev, REG(SPKERR), threshold);
    }

    ret |= fsm_reg_write(fsm_dev, REG(ADCTIME), 0x0031);
    ret |= fs1603_set_tsctrl(fsm_dev, FSM_ENABLE, FSM_ENABLE);

    return ret;
}

int fs1603_write_default_r25(fsm_dev_t *fsm_dev, int store)
{
    uint8_t re25_byte = 0;
    int re25 = 0, ret = 0;

    ret = fsm_access_key(fsm_dev, 1);
    re25 = (fsm_dev->spkr << 10); // 10 bit shift
    pr_info("%s defaut re25: %d\n", __func__, re25);
    fsm_dev->calib_data.calib_re25 = re25;
    re25_byte = fs1603_re25_to_byte(fsm_dev, re25);
    if (re25_byte != 0xff && store) {
        ret |= fs1603_store_otp(fsm_dev, re25_byte, store);
        fsm_dev->calib_data.store_otp = 0;
    }

    ret |= fs1603_set_threshold(fsm_dev, (uint16_t)re25, FSM_DATA_TYPE_RE25);
    ret |= fsm_access_key(fsm_dev, 0);

    return ret;
}

int fs1603_check_otp(fsm_dev_t *fsm_dev)
{
    uint16_t otppg2_val = 0;
    uint16_t pllc4_val = 0;
    int count = 0, re25 = 0, ret = 0;

    ret = fsm_reg_read(fsm_dev, REG(PLLCTRL4), &pllc4_val);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), 0x000f);
    ret |= fsm_access_key(fsm_dev, 1);
    ret |= fsm_reg_read(fsm_dev, REG(OTPPG2), &otppg2_val); // 0xE8
    pr_info("%s OTPPG2: 0x%04x\n", __func__, otppg2_val);
    fs1603_parse_otp(fsm_dev, otppg2_val, &count, &re25);
    fsm_dev->calib_data.calib_count = count;
    fsm_dev->calib_data.calib_re25 = re25;

    if (count > 0 && count <= MAX_CALIB_COUNT) {
        pr_info("%s already calibrated\n", __func__);
        ret |= fs1603_set_threshold(fsm_dev, re25, FSM_DATA_TYPE_RE25);
    } else if (count == 0) {
        pr_info("%s not calibrate yet\n", __func__);
        ret = -EINVAL;
    }
    ret |= fsm_access_key(fsm_dev, 0);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), pllc4_val);
    pr_info("%s ret = %d\n", __func__, ret);

    return ret;
}

int fs1603_reg_init(fsm_dev_t *fsm_dev)
{
    int ret = 0;
    int i = 0;
    uint16_t val = 0;

    pr_info("%s enter.\n", __func__);

    while (i++ < 15) {
        ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), 0x0002);
        fsm_reg_read(fsm_dev, REG(SYSCTRL), NULL);//dummy read, ignore error
        ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), 0x0001);
        fsm_delay_ms(15); // delay 15 ms
        ret |= fsm_reg_read(fsm_dev, REG(CHIPINI), &val);
        if ((val == 0x03) || (val == 0x0300))
            break;
    }
    ret |= fsm_reg_write(fsm_dev, 0xc4, 0x000a);
    fsm_delay_ms(5); // delay 5 ms
    ret |= fsm_reg_write(fsm_dev, 0x06, 0xff00);
    ret |= fsm_access_key(fsm_dev, 1);
    ret |= fsm_reg_write(fsm_dev, 0xc0, 0x5b80);
    ret |= fsm_reg_write(fsm_dev, 0xc4, 0x000f);
    ret |= fsm_reg_write(fsm_dev, 0xae, 0x0210);
    ret |= fsm_reg_write(fsm_dev, 0xb9, 0xffff);
    ret |= fsm_reg_write(fsm_dev, 0x09, 0x0009);
    ret |= fsm_reg_write(fsm_dev, 0xcd, 0x2004);
    ret |= fsm_reg_write(fsm_dev, 0xa1, 0x1c92);
    ret |= fsm_reg_write(fsm_dev, 0xc7, 0x4b46);
    ret |= fsm_access_key(fsm_dev, 0);

    ret |= fs1603_write_preset(fsm_dev);
    ret |= fs1603_power_on(fsm_dev, FSM_DISABLE);
    pr_info("%s exit, ret = %d.\n", __func__, ret);
    return ret;
}

static int fs1603_update_preset(fsm_dev_t *fsm_dev, uint16_t scene)
{
    union REG_DSPCTRL dspctrl;
    int ret = 0;

    pr_info("enter, scene to be set %d.\n", scene);

    // load reg table
    ret = fs1603_write_reg_tbl(fsm_dev, scene);
    ret |= fsm_reg_read(fsm_dev, REG(DSPCTRL), &dspctrl.value);
    if (dspctrl.bit.DSPEN == 0) {
        pr_info("%s bypass mode!!!\n", __func__);
        fsm_dev->flag |= FSM_FLAGS_BYPASS;
        return 0;
    }
    fsm_dev->flag &= ~FSM_FLAGS_BYPASS;

    ret |= fsm_access_key(fsm_dev, 1);
    if (scene & fsm_dev->ram_scene[FSM_EQ_RAM0]) {
        ret |= fsm_reg_write(fsm_dev, REG(ACSCTRL), 0x9880);
    } else if (scene & fsm_dev->ram_scene[FSM_EQ_RAM1]) {
        ret |= fsm_reg_write(fsm_dev, REG(ACSCTRL), 0x9890);
    } else {
        // use ram1 to switch preset
        ret |= fs1603_write_preset_eq(fsm_dev, FSM_EQ_RAM1, scene);
        if (!ret) {
            ret |= fsm_reg_write(fsm_dev, REG(ACSCTRL), 0x9890);
        }
    }
    ret |= fsm_access_key(fsm_dev, 0);

    pr_info("%s exit, ret = %d.\n", __func__, ret);

    return ret;
}

int fs1603_switch_preset(fsm_dev_t *fsm_dev, int force)
{
    union REG_SYSCTRL sysctrl;
    union REG_PLLCTRL4 pllc4;
    uint16_t temp = 0;
    uint16_t scene = fsm_dev->config->next_scene;
    int ret = 0, ready = 0;

    pr_info("force = %d!\n", force);
    ret = fsm_reg_read(fsm_dev, REG(STATUS), NULL);//TODO
    // amplifier off
    ret |= fsm_reg_read(fsm_dev, REG(SYSCTRL), &sysctrl.value);
    temp = sysctrl.value;
    sysctrl.bit.AMPE = 0;
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);
    sysctrl.value = temp;
    // wait stable
    ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
    if (!ready) {
        pr_err("wait timeout!\n");
        return -EINVAL;
    }
    // enable pll osc
    ret |= fsm_reg_read(fsm_dev, REG(PLLCTRL4), &pllc4.value);
    temp = pllc4.value;
    pllc4.bit.OSCEN = 1;
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), pllc4.value);
    pllc4.value = temp;
    ret |= fs1603_update_preset(fsm_dev, scene);
    if (!ret) {
        fsm_dev->cur_scene = scene;
        pr_info("switched scene: %04x\n", scene);
    }

    // recover pll and sysctrl
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), pllc4.value);
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);

    return ret;
}

int fs1603_start_up(fsm_dev_t *fsm_dev)
{
    int ret = 0;

    ret = fs1603_config_i2s(fsm_dev);
    ret |= fs1603_config_pll(fsm_dev, FSM_PLL_ON);
    ret |= fs1603_power_on(fsm_dev, FSM_ENABLE);// power on

    return ret;
}

int fs1603_shut_down(fsm_dev_t *fsm_dev)
{
    int ret = 0;

    ret = fs1603_wait_stable(fsm_dev, FSM_WAIT_TSIGNAL_OFF);//TODO
    ret |= fs1603_power_on(fsm_dev, FSM_DISABLE);// power down

    return ret;
}

int fs1603_set_mute(fsm_dev_t *fsm_dev, int mute, uint8_t volume)
{
    union REG_SYSCTRL sysctrl;
    union REG_AUDIOCTRL audioctrl;
    uint16_t status = 0;
    int ret = 0;

    audioctrl.value = 0;
    ret = fsm_reg_read(fsm_dev, REG(SYSCTRL), &sysctrl.value);
    if (mute == FSM_UNMUTE) {
        if ((fsm_dev->flag & FSM_FLAGS_BYPASS) && (fsm_dev->cur_scene == FSM_SCENE_EARPIECE)) {
            pr_info("receiver mode\n");
            volume = 0xdf;// 0xf8 = -2.625dB, 0xdf = -12db  (0xff - volume) * -0.375db = db
        }
        audioctrl.bit.VOL = volume;
        ret |= fsm_reg_write(fsm_dev, REG(AUDIOCTRL), audioctrl.value);
        sysctrl.bit.AMPE = 1;
        ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);
        fsm_delay_ms(15);
        if (fsm_dev->flag & FSM_FLAGS_BYPASS)
            ret |= fs1603_set_tsctrl(fsm_dev, FSM_DISABLE, FSM_ENABLE);
        else
            ret |= fs1603_set_tsctrl(fsm_dev, FSM_ENABLE, FSM_ENABLE);
        ret |= fsm_reg_read(fsm_dev, REG(STATUS), &status);
        pr_info("%s status: 0x%04X\n", __func__, status);
    } else {
        ret |= fsm_reg_read(fsm_dev, REG(STATUS), &status);
        pr_info("%s status: 0x%04X\n", __func__, status);
        ret |= fs1603_set_tsctrl(fsm_dev, FSM_DISABLE, FSM_ENABLE);
    }

    return ret;
}

static int fs1603_wait_type(fsm_dev_t *fsm_dev, int type)
{
    union REG_STATUS status;
    union REG_DIGSTAT digstat;
    union REG_TSCTRL tsctrl;
    union REG_BSTCTRL bstctrl;
    union REG_OTPCMD otpcmd;
    int ret = 0;
    int ready = 0;

    switch (type) {
        case FSM_WAIT_AMP_ON:
            ret = fsm_reg_read(fsm_dev, REG(STATUS), &status.value);
            ret |= fsm_reg_read(fsm_dev, REG(BSTCTRL), &bstctrl.value);
            ready = status.bit.PLLS & bstctrl.bit.SSEND;
            break;
        case FSM_WAIT_AMP_OFF:
            ret = fsm_reg_read(fsm_dev, REG(DIGSTAT), &digstat.value);
            ready = !digstat.bit.DACRUN;
            break;
        case FSM_WAIT_AMP_ADC_OFF:
        case FSM_WAIT_AMP_ADC_PLL_OFF:
            ret = fsm_reg_read(fsm_dev, REG(DIGSTAT), &digstat.value);
            ready = !(digstat.bit.DACRUN | digstat.bit.ADCRUN);
            break;
        case FSM_WAIT_TSIGNAL_OFF:
            ret = fsm_reg_read(fsm_dev, REG(TSCTRL), &tsctrl.value);
            ready = tsctrl.bit.OFFSTA;
            break;
        case FSM_WAIT_OTP_READY:
            ret = fsm_reg_read(fsm_dev, REG(OTPCMD), &otpcmd.value);
            ready = !otpcmd.bit.BUSY;
            break;
        default:
            break;
    }
    return (!ret && ready);
}

int fs1603_wait_stable(fsm_dev_t *fsm_dev, int type)
{
    int retries = FSM_WAIT_STABLE_RETRY;
    int ready = 0;

    fsm_delay_ms(5); // delay 5 ms
    if (type == FSM_WAIT_TSIGNAL_OFF || type == FSM_WAIT_AMP_ADC_PLL_OFF)
        retries = 30; // retry 30 times

    while (retries-- > 0) {
        ready = fs1603_wait_type(fsm_dev, type);
        if (ready)
            break;
        fsm_delay_ms(1); // delay 1 ms
    }
    if (retries <= 0)
        pr_err("%s wait stable type: %d timeout!\n", __func__, type);

    if (type == FSM_WAIT_AMP_ADC_PLL_OFF)
        fs1603_config_pll(fsm_dev, FSM_PLL_OFF); // disable pll

    return ready;
}

/**
 * switch specified scene, refer to fs1603_switch_preset
 */
static int switch_calib_preset(fsm_dev_t *fsm_dev, int scene)
{
    union REG_SYSCTRL sysctrl;
    union REG_PLLCTRL4 pllc4;
    uint16_t temp = 0;
    int ret = 0, ready = 0;

    // amplifier off
    ret = fsm_reg_read(fsm_dev, REG(SYSCTRL), &sysctrl.value);
    temp = sysctrl.value;
    sysctrl.bit.AMPE = 0;
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);
    sysctrl.value = temp;
    // wait stable
    ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
    if (!ready) {
        pr_err("wait timeout!\n");
        return -EINVAL;
    }
    // enable pll osc
    ret |= fsm_reg_read(fsm_dev, REG(PLLCTRL4), &pllc4.value);
    temp = pllc4.value;
    pllc4.bit.OSCEN = 1;
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), pllc4.value);
    pllc4.value = temp;
    ret |= fs1603_update_preset(fsm_dev, scene);
    if (!ret) {
        pr_info("switched scene: %04x\n", scene);
    }

    // recover pll and sysctrl
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), pllc4.value);
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), sysctrl.value);

    return ret;
}

int fs1603_calib_step1(fsm_dev_t *fsm_dev, int force)
{
    uint16_t status = 0;
    int ret = 0;

    // add log for compile error
    pr_info("%s force = %d", __func__, force);

    ret = fs1603_status_check(fsm_dev);
    if(ret)
        return 0;
    if ((fsm_dev->cur_scene & FSM_SCENE_MUSIC) == 0) {
        // use music scene config when do calibration
        ret = switch_calib_preset(fsm_dev, FSM_SCENE_MUSIC);
    }

    ret = fsm_access_key(fsm_dev, 1);
    ret |= fsm_reg_write(fsm_dev, REG(ZMCONFIG), 0x0010);
    ret |= fs1603_set_tsctrl(fsm_dev, FSM_ENABLE, FSM_DISABLE);
    ret |= fsm_reg_write(fsm_dev, REG(AUDIOCTRL), 0x8f00);
    ret |= fsm_reg_write(fsm_dev, REG(SPKERR), 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(SPKRE), 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(SPKM6), 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(SPKM24), 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(ADCENV), 0xffff);
    ret |= fsm_reg_write(fsm_dev, REG(ADCTIME), 0x0031);
    ret |= fsm_access_key(fsm_dev, 0);
    ret |= fsm_reg_read(fsm_dev, REG(STATUS), &status);
    pr_info("%s status: 0x%04x\n", __func__, status);

    return ret;
}
/*
int fs1603_calib_step2(fsm_dev_t *fsm_dev)
{
    uint16_t zmdata;
    int ret;

    ret = fsm_reg_read(fsm_dev, REG(ZMDATA), &zmdata);
    fsm_dev->calib_data.zmdata = zmdata;

    return ret;
}*/

int fs1603_calib_step2(fsm_dev_t *fsm_dev)
{
    int count = 0;
    int retries = 0;
    int retry_max = 80;

    while(retries++ < retry_max) {
        count = fsm_calib_zmdata(fsm_dev);
        if (count >= 10) {
            break;
        }

        fsm_delay_ms(100);//delay 100 ms
    }
    if (retries >= retry_max) {
        pr_err("%s calibrate failed\n", __func__);
        return -EINVAL;
    }
    pr_info("%s retry times: %d\n", __func__, retries);

    return 0;
}

int fs1603_calib_step3(fsm_dev_t *fsm_dev, int store)
{
    fsm_calib_data_t *calib_data = &fsm_dev->calib_data;
    uint16_t minval = 0;
    uint8_t re25_byte = 0;
    int re25 = 0, ret = 0;

    minval = calib_data->minval;
    ret = fsm_access_key(fsm_dev, 1);
    ret |= fs1603_set_threshold(fsm_dev, minval, FSM_DATA_TYPE_ZMDATA);
    ret |= fsm_reg_write(fsm_dev, REG(ZMCONFIG), 0x0000);
    ret |= fsm_access_key(fsm_dev, 0);
    if (ret)
        return ret;

    re25 = calib_data->calib_re25;
    re25_byte = fs1603_re25_to_byte(fsm_dev, re25);

    if (re25_byte != 0xff && store) {
        ret |= fs1603_store_otp(fsm_dev, re25_byte, store);
        calib_data->store_otp = 0;
    }
    ret |= fs1603_set_tsctrl(fsm_dev, FSM_ENABLE, FSM_ENABLE);

    if (!ret) {
	    if((fsm_dev->cur_scene & FSM_SCENE_MUSIC) == 0) {
            // restore current scene config
            ret = switch_calib_preset(fsm_dev, fsm_dev->cur_scene);
		}
        if (fsm_dev->cur_scene == FSM_SCENE_EARPIECE) {
            // restore earpiece volume config
            ret |= fsm_reg_write(fsm_dev, REG(AUDIOCTRL), 0xdf);
        }
		else {
            // restore other volume config
            ret |= fsm_reg_write(fsm_dev, REG(AUDIOCTRL), 0xff);
		}
    }

    return ret;
}


static int fs1603_f0_config_regs(fsm_dev_t *fsm_dev)
{
    uint16_t value = 0, i2sctrl = 0;
    int ret = 0;

    // ValD0 Bypass OT
    ret = fsm_reg_write(fsm_dev, REG(ANACTRL), 0x0120);
    //DCR Bypass
    ret |= fsm_reg_write(fsm_dev, 0xD7, 0x1020);
    // Bypass OT by reseting speaker OT thresholds
    ret |= fsm_reg_write(fsm_dev, 0xC9, 0x0000);
    ret |= fsm_reg_write(fsm_dev, 0xCA, 0x0000);
    ret |= fsm_reg_write(fsm_dev, 0xCB, 0x0000);
    ret |= fsm_reg_write(fsm_dev, 0xCC, 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(AUDIOCTRL), 0xFF00);
    ret |= fsm_reg_write(fsm_dev, REG(DSPCTRL), 0x1012);
    ret |= fsm_reg_write(fsm_dev, REG(STERCTRL), 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(ACSCTRL), 0x9800);
    ret |= fsm_reg_write(fsm_dev, 0x8B, 0x0000);
    ret |= fsm_reg_write(fsm_dev, 0x8C, 0x0000);
    ret |= fsm_reg_write(fsm_dev, REG(BFLCTRL), 0x0006);
    ret |= fsm_reg_write(fsm_dev, REG(BFLSET), 0x0093);
    ret |= fsm_reg_write(fsm_dev, 0xA9, 0x02E0);
    ret |= fsm_reg_write(fsm_dev, REG(AGC), 0x00B5);
    ret |= fsm_reg_write(fsm_dev, 0xAB, 0x0010);
    ret |= fsm_reg_write(fsm_dev, 0xAD, 0x0001);
    ret |= fsm_reg_read(fsm_dev, REG(I2SCTRL), &i2sctrl);
    ret |= fsm_reg_write(fsm_dev, REG(ADCENV), 0x9FFF);
    ret |= fsm_reg_write(fsm_dev, REG(ADCTIME), 0x0030);

    // disable I2SDOE
    ret |= fsm_reg_write(fsm_dev, REG(I2SCTRL), i2sctrl & (~0x0800));
    ret |= fsm_reg_read(fsm_dev, REG(BSTCTRL), &value);
    ret |= fsm_reg_write(fsm_dev, REG(BSTCTRL), value | 0x000c);
    ret |= fsm_reg_write(fsm_dev, REG(ADCCTRL), 0x0300);
    ret |= fsm_reg_write(fsm_dev, REG(TSCTRL), 0x162F);
    ret |= fsm_reg_write(fsm_dev, REG(I2SCTRL), i2sctrl);

    return ret;
}

#if 0
static int fs1603_f0_update_ram(fsm_dev_t *fsm_dev)
{
    int32_t coef_acs_dflt[COEF_LEN] = { 0x000603ac, 0x001603ac, 0x001603ac, 0x001603ac, 0x001603ac };
    int32_t coef_acs_gain[COEF_LEN] = { 0x003b1bb5, 0x001603ac, 0x001603ac, 0x001603ac, 0x001603ac };
    int32_t coef_ad[COEF_LEN] = { 0x0069b1cd, 0x001603ad, 0x001603ad, 0x001603ad, 0x001603ad };
    int32_t coef_prescale_dflt = 0x000603ac;
    uint8_t buf[4] = {0}; // data len 4
    int i, count, ret;

    // Set ADC coef(B0, B1)
    ret = fsm_reg_write(fsm_dev, REG(ADCEQA), 0x0000);
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coef_ad[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ADCEQWL), buf, 4); // write 4 bytes each time
    }
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coef_ad[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ADCEQWL), buf, 4); // write 4 bytes each time
    }
    // Set acs to default coef
    ret |= fsm_reg_write(fsm_dev, REG(ACSEQA), 0x0000);
    for (count = 0; count < ACS_COEF_COUNT; count ++) {
        for (i = 0; i < COEF_LEN; i++) {
            convert_data_to_bytes(coef_acs_dflt[i], buf);
            ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4); // write 4 bytes each time
        }
    }
    // Prescale
    convert_data_to_bytes(coef_prescale_dflt, buf);
    ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4); // write 4 bytes each time

    // Set acs b2, b3
    ret |= fsm_reg_write(fsm_dev, REG(ACSEQA), 0x000A);
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coef_acs_gain[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4); // write 4 bytes each time
    }
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coef_acs_gain[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4); // write 4 bytes each time
    }
    // ADC on
    ret |= fsm_reg_write(fsm_dev, REG(ADCCTRL), 0x1300);

    return ret;
}
#endif

int fs1603_get_f0_step1(fsm_dev_t *fsm_dev)
{
    int32_t coefACSDflt[COEF_LEN] = {0x000603ac, 0x001603ac, 0x001603ac, 0x001603ac, 0x001603ac};
    int32_t coefACSGain[COEF_LEN] = {0x003b1bb5, 0x001603ac, 0x001603ac, 0x001603ac, 0x001603ac};
    int32_t coefAD[COEF_LEN] = {0x0069b1cd, 0x001603ad, 0x001603ad, 0x001603ad, 0x001603ad};
    int32_t coefPrescaleDflt = 0x000603ac;
    uint16_t value = 0;
    uint8_t buf[4];
    int count = 0;
    int ready = 0;
    int ret = 0;
    int i = 0;

    if (fsm_dev == NULL) {
        return -EINVAL;
    }

    ret = fs1603_status_check(fsm_dev);
    if(ret)
        return 0;

    ret = fsm_reg_write(fsm_dev, REG(OTPACC), 0xca91);
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), 0x0000);
    ret |= fsm_reg_read(fsm_dev, REG(PLLCTRL4), &value);
    ret |= fsm_reg_write(fsm_dev, REG(PLLCTRL4), value | 0x000f);
    // wait stable
    ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
    if (!ready) {
        pr_err("wait timeout!");
        return -EINVAL;
    }
    ret |= fs1603_f0_config_regs(fsm_dev);

    // Make sure amp is already off
    ret |= fsm_reg_read(fsm_dev, REG(SYSCTRL), NULL);

    // Set ADC coef(B0, B1)
    ret |= fsm_reg_write(fsm_dev, REG(ADCEQA), 0x0000);
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coefAD[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ADCEQWL), buf, 4);
    }
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coefAD[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ADCEQWL), buf, 4);
    }
    // Set acs to default coef
    ret |= fsm_reg_write(fsm_dev, REG(ACSEQA), 0x0000);
    for (count = 0; count < ACS_COEF_COUNT; count ++) {
        for (i = 0; i < COEF_LEN; i++) {
            convert_data_to_bytes(coefACSDflt[i], buf);
            ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4);
        }
    }
    // Prescale
    convert_data_to_bytes(coefPrescaleDflt, buf);
    ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4);

    // Set acs b2, b3
    ret |= fsm_reg_write(fsm_dev, REG(ACSEQA), 0x000A);
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coefACSGain[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4);
    }
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coefACSGain[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4);
    }

    // ADC on
    ret |= fsm_reg_write(fsm_dev, REG(ADCCTRL), 0x1300);

    return ret;
}

const static struct fsm_bpcoef g_fs1603_bpcoef_table[] = {
    {  200, { 0x00160EF6, 0x001603AD, 0xFFE9F108, 0x0009E1D6, 0xFFE61915 } },
    {  250, { 0x00160EF6, 0x001603AD, 0xFFE9F108, 0x0009E344, 0xFFE61915 } },
    {  300, { 0x00160EF6, 0x001603AD, 0xFFE9F108, 0x0009DD50, 0xFFE6191A } },
    {  350, { 0x00160EF6, 0x001603AD, 0xFFE9F108, 0x0009DF15, 0xFFE6191A } },
    {  400, { 0x00160EF7, 0x001603AD, 0xFFE9F10B, 0x0009D9B4, 0xFFE6191B } },
    {  450, { 0x00160EF7, 0x001603AD, 0xFFE9F10B, 0x0009D48C, 0xFFE61918 } },
    {  500, { 0x00160EF4, 0x001603AD, 0xFFE9F10A, 0x0009D07D, 0xFFE61919 } },
    {  550, { 0x00160EF4, 0x001603AD, 0xFFE9F10A, 0x0009D38B, 0xFFE6191E } },
    {  600, { 0x00160EF5, 0x001603AD, 0xFFE9F105, 0x0009CF8F, 0xFFE6191F } },
    {  650, { 0x00160EF5, 0x001603AD, 0xFFE9F105, 0x0009C468, 0xFFE6191C } },
    {  700, { 0x00160EFA, 0x001603AD, 0xFFE9F104, 0x0009C0A2, 0xFFE61902 } },
    {  750, { 0x00160EFB, 0x001603AD, 0xFFE9F107, 0x0009BDAD, 0xFFE61903 } },
    {  800, { 0x00160EFB, 0x001603AD, 0xFFE9F107, 0x0009BB35, 0xFFE61900 } },
    {  850, { 0x00160EF8, 0x001603AD, 0xFFE9F106, 0x0009B17A, 0xFFE61906 } },
    {  900, { 0x00160EF9, 0x001603AD, 0xFFE9F101, 0x0009AF13, 0xFFE61904 } },
    {  950, { 0x00160EFE, 0x001603AD, 0xFFE9F100, 0x0009A5E6, 0xFFE6190A } },
    { 1000, { 0x00160EFF, 0x001603AD, 0xFFE9F103, 0x00099CD2, 0xFFE6190B } },
    { 1050, { 0x00160EFC, 0x001603AD, 0xFFE9F102, 0x00099BF6, 0xFFE61909 } },
    { 1100, { 0x00160EFD, 0x001603AD, 0xFFE9F11D, 0x00099373, 0xFFE6190C } },
    { 1150, { 0x00160EE2, 0x001603AD, 0xFFE9F11C, 0x00098AA4, 0xFFE61932 } },
    { 1200, { 0x00160EE3, 0x001603AD, 0xFFE9F11F, 0x00098376, 0xFFE61930 } },
    { 1250, { 0x00160EE0, 0x001603AD, 0xFFE9F11E, 0x00097BF8, 0xFFE61936 } },
    { 1300, { 0x00160EE6, 0x001603AD, 0xFFE9F118, 0x00096CDA, 0xFFE61935 } },
    { 1350, { 0x00160EE7, 0x001603AD, 0xFFE9F11B, 0x000965EC, 0xFFE6193B } },
    { 1400, { 0x00160EE4, 0x001603AD, 0xFFE9F11A, 0x00095F1F, 0xFFE6193E } },
    { 1450, { 0x00160EEA, 0x001603AD, 0xFFE9F114, 0x00095166, 0xFFE6193D } },
    { 1500, { 0x00160EEB, 0x001603AD, 0xFFE9F117, 0x00094B20, 0xFFE61920 } },
    { 1550, { 0x00160EE9, 0x001603AD, 0xFFE9F111, 0x00093E5A, 0xFFE61927 } },
    { 1600, { 0x00160EEE, 0x001603AD, 0xFFE9F110, 0x000930A4, 0xFFE6192A } },
    { 1650, { 0x00160EEC, 0x001603AD, 0xFFE9F112, 0x00092469, 0xFFE61929 } },
    { 1700, { 0x00160EED, 0x001603AD, 0xFFE9F16D, 0x00091F8A, 0xFFE6192C } },
    { 1750, { 0x00160E93, 0x001603AD, 0xFFE9F16F, 0x00091399, 0xFFE619D3 } },
    { 1800, { 0x00160E91, 0x001603AD, 0xFFE9F169, 0x00090044, 0xFFE619D7 } },
    { 1850, { 0x00160E96, 0x001603AD, 0xFFE9F168, 0x0008F4EA, 0xFFE619DA } },
    { 1900, { 0x00160E94, 0x001603AD, 0xFFE9F16A, 0x0008E9E3, 0xFFE619DE } },
    { 1950, { 0x00160E9A, 0x001603AD, 0xFFE9F164, 0x0008DF53, 0xFFE619C2 } },
    { 2000, { 0x00160E98, 0x001603AD, 0xFFE9F166, 0x0008CCF5, 0xFFE619C6 } },
};

int fs1603_get_f0_step2(fsm_dev_t *fsm_dev)
{
    fsm_config_t *cfg = fsm_get_config();
    int freq = cfg->test_freq;
    const uint32_t *coefACSBP;
    uint8_t buf[4];
    int ready = 0;
    int size = 0;
    int ret = 0;
    int i = 0;

    if (fsm_dev == NULL || !cfg) {
        return -EINVAL;
    }

    ret = fs1603_status_check(fsm_dev);
    if(ret)
        return 0;

    // CalculateBPCoef
    size = sizeof(g_fs1603_bpcoef_table) / sizeof(struct fsm_bpcoef);
    for (i = 0; i < size; i++) {
        if (freq == g_fs1603_bpcoef_table[i].freq) {
            coefACSBP = g_fs1603_bpcoef_table[i].coef;
            break;
        }
    }
    if (i == size) {
        pr_err("freq no matched: %d", freq);
        return -EINVAL;
    }

    freq = cfg->test_freq;
    // Keep amp off here.
    ret = fsm_reg_write(fsm_dev, REG(SYSCTRL), 0x0000);
    // wait stable
    ready = fs1603_wait_stable(fsm_dev, FSM_WAIT_AMP_OFF);
    if (!ready) {
        return -EINVAL;
    }

    // ACS EQ band 0 and band 1
    ret |= fsm_reg_write(fsm_dev, REG(ACSEQA), 0x0000);
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coefACSBP[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4);
    }
    for (i = 0; i < COEF_LEN; i++) {
        convert_data_to_bytes(coefACSBP[i], buf);
        ret |= fsm_burst_write(fsm_dev, REG(ACSEQWL), buf, 4);
    }
    // Amp on
    ret |= fsm_reg_write(fsm_dev, REG(SYSCTRL), 0x0008);
    // now need sleep 350ms, first time need 700ms

    return ret;
}

int fs1603_set_bypass(fsm_dev_t *fsm_dev, int bypass)
{
    int ret = 0;
    union REG_DSPCTRL dspval;

    ret = fsm_reg_read(fsm_dev, REG(DSPCTRL), &dspval.value);
    if (bypass) {
        pr_info("do dsp bypass\n");
        dspval.bit.DSPEN = 0;
        ret |= fsm_reg_write(fsm_dev, REG(DSPCTRL), dspval.value);
        ret |= fsm_reg_write(fsm_dev, REG(AUDIOCTRL), 0xE600);
    } else {
        pr_info("dsp work mode\n");
        dspval.bit.DSPEN = 1;
        ret |= fsm_reg_write(fsm_dev, REG(DSPCTRL), dspval.value);
        ret |= fsm_reg_write(fsm_dev, REG(AUDIOCTRL), 0xFF00);
    }

    return ret;
}

int fs1603_status_check(fsm_dev_t *fsm_dev)
{
    union REG_STATUS status;

    fsm_reg_read(fsm_dev, 0x00, &status.value);
    if(!status.bit.CLKS) {
        pr_info("no i2s clock, status-reg: 0x%04X", status.value);
        fsm_dev->errcode = FSM_ERROR_NO_CLK;
        return -EINVAL;
    } else if(!status.bit.UVDS) {
        pr_info("under voltage detected, status-reg: 0x%04X", status.value);
        fsm_dev->errcode = FSM_ERROR_UV_DETECTED;
        return -EINVAL;
    } else if (status.bit.OCDS) {
        pr_info("over current detected, status-reg: 0x%04X", status.value);
        fsm_dev->errcode = FSM_ERROR_OC_DETECTED;
        return -EINVAL;
    } else  if (!status.bit.OVDS) {
        pr_info("over voltage detected, status-reg: 0x%04X", status.value);
        fsm_dev->errcode = FSM_ERROR_OV_DETECTED;
        return -EINVAL;
    } else  if (!status.bit.OTDS) {
        pr_info("over temperature detected, status-reg: 0x%04X", status.value);
        fsm_dev->errcode = FSM_ERROR_OT_DETECTED;
        return -EINVAL;
    }

    return 0;
}

void fs1603_ops(fsm_dev_t *fsm_dev)
{
    fsm_dev->dev_ops.init_register = fs1603_reg_init;
    fsm_dev->dev_ops.switch_preset = fs1603_switch_preset;
    fsm_dev->dev_ops.check_otp = fs1603_check_otp;
    fsm_dev->dev_ops.start_up = fs1603_start_up;
    fsm_dev->dev_ops.shut_down = fs1603_shut_down;
    fsm_dev->dev_ops.set_mute = fs1603_set_mute;
    fsm_dev->dev_ops.wait_stable = fs1603_wait_stable;
    fsm_dev->dev_ops.calib_step1 = fs1603_calib_step1;
    fsm_dev->dev_ops.calib_step2 = fs1603_calib_step2;
    fsm_dev->dev_ops.calib_step3 = fs1603_calib_step3;
    fsm_dev->dev_ops.f0_step1 = fs1603_get_f0_step1;
    fsm_dev->dev_ops.f0_step2 = fs1603_get_f0_step2;
    fsm_dev->dev_ops.dsp_bypass = fs1603_set_bypass;
    fsm_dev->dev_ops.write_default_r25 = fs1603_write_default_r25;
}

