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
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include "debussy.h"
#include "debussy_intf.h"
#include "debussy_snd_ctrl.h"
#include "debussy_customer.h"
#define IGO_CH_NO_INVERT (0)
#define IGO_CH_MAX (0xFFFFFFFF)
#define IGO_CH_SHIFT (0)


static const char* const enum_op_mode[] = {
    "OP_MODE_CONFIG",
    "OP_MODE_XNR",
    "OP_MODE_NR",
    "OP_MODE_LPNR",
    "OP_MODE_BYPASS",
    "OP_MODE_VAD",
    "OP_MODE_LPVAD",
    "OP_MODE_KWS",
    "OP_MODE_BARGEIN",
    "OP_MODE_DBG",
    "OP_MODE_RSV",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_op_mode, enum_op_mode);


static const char* const enum_nr_ul[] = {
    "NR_UL_DISABLE",
    "NR_UL_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_ul, enum_nr_ul);


static const char* const enum_nr_ul_sec[] = {
    "NR_UL_SEC_DISABLE",
    "NR_UL_SEC_MODE0",
    "NR_UL_SEC_MODE1",
    "NR_UL_SEC_MODE2",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_ul_sec, enum_nr_ul_sec);


static const char* const enum_nr_adaptive_en[] = {
    "NR_ADAPTIVE_EN_DISABLE",
    "NR_ADAPTIVE_EN_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_adaptive_en, enum_nr_adaptive_en);


static const char* const enum_nr_voice_str[] = {
    "NR_VOICE_STR_LV_0",
    "NR_VOICE_STR_LV_1",
    "NR_VOICE_STR_LV_2",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_voice_str, enum_nr_voice_str);


static const char* const enum_nr_noisy_level[] = {
    "NR_NOISY_LEVEL_LV_0",
    "NR_NOISY_LEVEL_LV_1",
    "NR_NOISY_LEVEL_LV_2",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_noisy_level, enum_nr_noisy_level);


static const char* const enum_nr_level[] = {
    "NR_LEVEL_LV_0",
    "NR_LEVEL_LV_1",
    "NR_LEVEL_LV_2",
    "NR_LEVEL_LV_3",
    "NR_LEVEL_LV_4",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_level, enum_nr_level);


static const char* const enum_aec_en[] = {
    "AEC_EN_DISABLE",
    "AEC_EN_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_aec_en, enum_aec_en);


static const char* const enum_aes_en[] = {
    "AES_EN_DISABLE",
    "AES_EN_LV_0",
    "AES_EN_LV_1",
    "AES_EN_LV_2",
    "AES_EN_LV_3",
    "AES_EN_LV_4",
    "AES_EN_LV_5",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_aes_en, enum_aes_en);


static const char* const enum_vad_status[] = {
    "VAD_STATUS_STANDBY",
    "VAD_STATUS_TRIGGERED",
    "VAD_STATUS_HW_VAD_TRIGGERED",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_status, enum_vad_status);


static const char* const enum_vad_clear[] = {
    "VAD_CLEAR_NOP",
    "VAD_CLEAR_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_clear, enum_vad_clear);


static const char* const enum_vad_int_mod[] = {
    "VAD_INT_MOD_DISABLE",
    "VAD_INT_MOD_EDGE",
    "VAD_INT_MOD_LEVEL",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_int_mod, enum_vad_int_mod);


static const char* const enum_vad_int_pin[] = {
    "VAD_INT_PIN_DAI0_BCLK",
    "VAD_INT_PIN_DAI0_LRCLK",
    "VAD_INT_PIN_DAI0_RXDAT",
    "VAD_INT_PIN_DAI0_TXDAT",
    "VAD_INT_PIN_DAI1_BCLK",
    "VAD_INT_PIN_DAI1_LRCLK",
    "VAD_INT_PIN_DAI1_RXDAT",
    "VAD_INT_PIN_DAI1_TXDAT",
    "VAD_INT_PIN_DAI2_BCLK",
    "VAD_INT_PIN_DAI2_LRCLK",
    "VAD_INT_PIN_DAI2_RXDAT",
    "VAD_INT_PIN_DAI2_TXDAT",
    "VAD_INT_PIN_DAI3_BCLK",
    "VAD_INT_PIN_DAI3_LRCLK",
    "VAD_INT_PIN_DAI3_RXDAT",
    "VAD_INT_PIN_DAI3_TXDAT",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_int_pin, enum_vad_int_pin);


static const char* const enum_vad_key_group_sel[] = {
    "VAD_KEY_GROUP_SEL_GROUP_0",
    "VAD_KEY_GROUP_SEL_GROUP_1",
    "VAD_KEY_GROUP_SEL_GROUP_2",
    "VAD_KEY_GROUP_SEL_GROUP_3",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_key_group_sel, enum_vad_key_group_sel);


static const char* const enum_vad_voice_enhance[] = {
    "VAD_VOICE_ENHANCE_DISABLE",
    "VAD_VOICE_ENHANCE_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_voice_enhance, enum_vad_voice_enhance);


static const char* const enum_vad_voice_enroll[] = {
    "VAD_VOICE_ENROLL_DISABLE",
    "VAD_VOICE_ENROLL_ENROLL",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_voice_enroll, enum_vad_voice_enroll);


static const char* const enum_vad_enroll_cnt[] = {
    "VAD_ENROLL_CNT_CNT_0",
    "VAD_ENROLL_CNT_CNT_1",
    "VAD_ENROLL_CNT_CNT_2",
    "VAD_ENROLL_CNT_CNT_3",
    "VAD_ENROLL_CNT_CNT_4",
    "VAD_ENROLL_CNT_ENROLL_DONE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_enroll_cnt, enum_vad_enroll_cnt);


static const char* const enum_vad_enroll_apply[] = {
    "VAD_ENROLL_APPLY_DISABLE",
    "VAD_ENROLL_APPLY_APLLY",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_enroll_apply, enum_vad_enroll_apply);


static const char* const enum_vad_enroll_rst[] = {
    "VAD_ENROLL_RST_DISABLE",
    "VAD_ENROLL_RST_ENROLL_RST",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_enroll_rst, enum_vad_enroll_rst);


static const char* const enum_vad_keyword_hit_clear[] = {
    "VAD_KEYWORD_HIT_CLEAR_NOP",
    "VAD_KEYWORD_HIT_CLEAR_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_keyword_hit_clear, enum_vad_keyword_hit_clear);


static const char* const enum_vad_bg_model_en[] = {
    "VAD_BG_MODEL_EN_ENABLE",
    "VAD_BG_MODEL_EN_DISABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_bg_model_en, enum_vad_bg_model_en);


static const char* const enum_vad_keyword_thr[] = {
    "VAD_KEYWORD_THR_THR_P_0",
    "VAD_KEYWORD_THR_THR_P_2",
    "VAD_KEYWORD_THR_THR_P_4",
    "VAD_KEYWORD_THR_THR_P_6",
    "VAD_KEYWORD_THR_THR_P_8",
    "VAD_KEYWORD_THR_THR_P_10",
    "VAD_KEYWORD_THR_THR_P_12",
    "VAD_KEYWORD_THR_THR_N_2",
    "VAD_KEYWORD_THR_THR_N_4",
    "VAD_KEYWORD_THR_THR_N_6",
    "VAD_KEYWORD_THR_THR_N_8",
    "VAD_KEYWORD_THR_THR_N_10",
    "VAD_KEYWORD_THR_THR_N_12",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_keyword_thr, enum_vad_keyword_thr);


static const char* const enum_vad_buf_rst[] = {
    "VAD_BUF_RST_DISABLE",
    "VAD_BUF_RST_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_buf_rst, enum_vad_buf_rst);


static const char* const enum_agc_mode[] = {
    "AGC_MODE_DISABLE",
    "AGC_MODE_ONE_STAGE_AGC",
    "AGC_MODE_TWO_STAGE_AGC",
    "AGC_MODE_ASR_GAIN",
    "AGC_MODE_FFPU",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_agc_mode, enum_agc_mode);


static const char* const enum_agc_gain[] = {
    "AGC_GAIN_0_DB",
    "AGC_GAIN_6_DB",
    "AGC_GAIN_9P5_DB",
    "AGC_GAIN_12_DB",
    "AGC_GAIN_15P5_DB",
    "AGC_GAIN_18_DB",
    "AGC_GAIN_21P5_DB",
    "AGC_GAIN_24_DB",
    "AGC_GAIN_27P5_DB",
    "AGC_GAIN_30_DB",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_agc_gain, enum_agc_gain);


static const char* const enum_asr_gain_max_vol[] = {
    "ASR_GAIN_MAX_VOL_0_DB",
    "ASR_GAIN_MAX_VOL_N_2_DB",
    "ASR_GAIN_MAX_VOL_N_6_DB",
    "ASR_GAIN_MAX_VOL_N_12_DB",
    "ASR_GAIN_MAX_VOL_N_18_DB",
    "ASR_GAIN_MAX_VOL_N_24_DB",
    "ASR_GAIN_MAX_VOL_N_30_DB",
    "ASR_GAIN_MAX_VOL_N_36_DB",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_asr_gain_max_vol, enum_asr_gain_max_vol);


static const char* const enum_ul_tx_gain[] = {
    "UL_TX_GAIN_0_DB",
    "UL_TX_GAIN_P_1_DB",
    "UL_TX_GAIN_P_2_DB",
    "UL_TX_GAIN_P_3_DB",
    "UL_TX_GAIN_P_4_DB",
    "UL_TX_GAIN_P_5_DB",
    "UL_TX_GAIN_P_6_DB",
    "UL_TX_GAIN_P_7_DB",
    "UL_TX_GAIN_P_8_DB",
    "UL_TX_GAIN_P_9_DB",
    "UL_TX_GAIN_P_10_DB",
    "UL_TX_GAIN_P_11_DB",
    "UL_TX_GAIN_P_12_DB",
    "UL_TX_GAIN_P_13_DB",
    "UL_TX_GAIN_P_14_DB",
    "UL_TX_GAIN_P_15_DB",
    "UL_TX_GAIN_P_16_DB",
    "UL_TX_GAIN_N_1_DB",
    "UL_TX_GAIN_N_2_DB",
    "UL_TX_GAIN_N_3_DB",
    "UL_TX_GAIN_N_4_DB",
    "UL_TX_GAIN_N_5_DB",
    "UL_TX_GAIN_N_6_DB",
    "UL_TX_GAIN_N_7_DB",
    "UL_TX_GAIN_N_8_DB",
    "UL_TX_GAIN_N_9_DB",
    "UL_TX_GAIN_N_10_DB",
    "UL_TX_GAIN_N_11_DB",
    "UL_TX_GAIN_N_12_DB",
    "UL_TX_GAIN_N_13_DB",
    "UL_TX_GAIN_N_14_DB",
    "UL_TX_GAIN_N_15_DB",
    "UL_TX_GAIN_N_16_DB",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_tx_gain, enum_ul_tx_gain);


static const char* const enum_ul_tx_mute[] = {
    "UL_TX_MUTE_DISABLE",
    "UL_TX_MUTE_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_tx_mute, enum_ul_tx_mute);


static const char* const enum_nr_mode1_en[] = {
    "NR_MODE1_EN_DISABLE",
    "NR_MODE1_EN_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_en, enum_nr_mode1_en);


static const char* const enum_nr_mode2_en[] = {
    "NR_MODE2_EN_DISABLE",
    "NR_MODE2_EN_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode2_en, enum_nr_mode2_en);


static const char* const enum_nr_mode3_en[] = {
    "NR_MODE3_EN_DISABLE",
    "NR_MODE3_EN_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode3_en, enum_nr_mode3_en);


static const char* const enum_nr_mode1_floor[] = {
    "NR_MODE1_FLOOR_LVL_DEFAULT",
    "NR_MODE1_FLOOR_LVL_0",
    "NR_MODE1_FLOOR_LVL_1",
    "NR_MODE1_FLOOR_LVL_2",
    "NR_MODE1_FLOOR_LVL_3",
    "NR_MODE1_FLOOR_LVL_4",
    "NR_MODE1_FLOOR_LVL_5",
    "NR_MODE1_FLOOR_LVL_6",
    "NR_MODE1_FLOOR_LVL_7",
    "NR_MODE1_FLOOR_LVL_8",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_floor, enum_nr_mode1_floor);


static const char* const enum_nr_mode1_od[] = {
    "NR_MODE1_OD_LVL_DEFAULT",
    "NR_MODE1_OD_LVL_0",
    "NR_MODE1_OD_LVL_1",
    "NR_MODE1_OD_LVL_2",
    "NR_MODE1_OD_LVL_3",
    "NR_MODE1_OD_LVL_4",
    "NR_MODE1_OD_LVL_5",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_od, enum_nr_mode1_od);


static const char* const enum_nr_mode1_thr[] = {
    "NR_MODE1_THR_LVL_DEFAULT",
    "NR_MODE1_THR_LVL_0",
    "NR_MODE1_THR_LVL_1",
    "NR_MODE1_THR_LVL_2",
    "NR_MODE1_THR_LVL_3",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_thr, enum_nr_mode1_thr);


static const char* const enum_nr_mode1_smooth_mode[] = {
    "NR_MODE1_SMOOTH_MODE_MODE0",
    "NR_MODE1_SMOOTH_MODE_MODE1",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_smooth_mode, enum_nr_mode1_smooth_mode);


static const char* const enum_nr_mode1_pp_param10[] = {
    "NR_MODE1_PP_PARAM10_LVL_0",
    "NR_MODE1_PP_PARAM10_LVL_1",
    "NR_MODE1_PP_PARAM10_LVL_2",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_pp_param10, enum_nr_mode1_pp_param10);


static const char* const enum_nr_mode1_pp_param41[] = {
    "NR_MODE1_PP_PARAM41_LVL_DEFAULT",
    "NR_MODE1_PP_PARAM41_LVL_0",
    "NR_MODE1_PP_PARAM41_LVL_1",
    "NR_MODE1_PP_PARAM41_LVL_2",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_pp_param41, enum_nr_mode1_pp_param41);


static const char* const enum_nr_mode1_pp_param43[] = {
    "NR_MODE1_PP_PARAM43_DISABLE",
    "NR_MODE1_PP_PARAM43_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_pp_param43, enum_nr_mode1_pp_param43);


static const char* const enum_nr_mode1_pp_param46[] = {
    "NR_MODE1_PP_PARAM46_LVL_DEFAULT",
    "NR_MODE1_PP_PARAM46_LVL_0",
    "NR_MODE1_PP_PARAM46_LVL_1",
    "NR_MODE1_PP_PARAM46_LVL_2",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_pp_param46, enum_nr_mode1_pp_param46);


static const char* const enum_nr_mode1_pp_param47[] = {
    "NR_MODE1_PP_PARAM47_DISABLE",
    "NR_MODE1_PP_PARAM47_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode1_pp_param47, enum_nr_mode1_pp_param47);


static const char* const enum_nr_mode3_param0[] = {
    "NR_MODE3_PARAM0_DISABLE",
    "NR_MODE3_PARAM0_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_mode3_param0, enum_nr_mode3_param0);


static const char* const enum_nr_signle_tone_detect_en[] = {
    "NR_SIGNLE_TONE_DETECT_EN_DISABLE",
    "NR_SIGNLE_TONE_DETECT_EN_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_nr_signle_tone_detect_en, enum_nr_signle_tone_detect_en);


static const char* const enum_aec_param5[] = {
    "AEC_PARAM5_DISABLE",
    "AEC_PARAM5_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_aec_param5, enum_aec_param5);


static const char* const enum_aec_ref_gain[] = {
    "AEC_REF_GAIN_0_DB",
    "AEC_REF_GAIN_6_DB",
    "AEC_REF_GAIN_12_DB",
    "AEC_REF_GAIN_18_DB",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_aec_ref_gain, enum_aec_ref_gain);


static const char* const enum_vad_kws_mode[] = {
    "VAD_KWS_MODE_DEFAULT",
    "VAD_KWS_MODE_M0",
    "VAD_KWS_MODE_M1",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_vad_kws_mode, enum_vad_kws_mode);


static const char* const enum_dbg_aec_rec_en[] = {
    "DBG_AEC_REC_EN_DISABLE",
    "DBG_AEC_REC_EN_48K",
    "DBG_AEC_REC_EN_16K_STEREO",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dbg_aec_rec_en, enum_dbg_aec_rec_en);


static const char* const enum_dbg_streaming[] = {
    "DBG_STREAMING_DISABLE",
    "DBG_STREAMING_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dbg_streaming, enum_dbg_streaming);


static const char* const enum_power_mode[] = {
    "POWER_MODE_STANDBY",
    "POWER_MODE_WORKING",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_power_mode, enum_power_mode);


static const char* const enum_ck_output[] = {
    "CK_OUTPUT_DISABLE",
    "CK_OUTPUT_12M",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ck_output, enum_ck_output);


static const char* const enum_cali_status[] = {
    "CALI_STATUS_NONE",
    "CALI_STATUS_READY",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_cali_status, enum_cali_status);


static const char* const enum_hif_cali_en[] = {
    "HIF_CALI_EN_DISABLE",
    "HIF_CALI_EN_QCK_EN",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_hif_cali_en, enum_hif_cali_en);


static const char* const enum_crc_check[] = {
    "CRC_CHECK_FAIL",
    "CRC_CHECK_PASS",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_crc_check, enum_crc_check);


static const char* const enum_switch_mode[] = {
    "SWITCH_MODE_NONE",
    "SWITCH_MODE_VAD",
    "SWITCH_MODE_COMMAND",
    "SWITCH_MODE_BYPASS",
    "SWITCH_MODE_IDLE",
    "SWITCH_MODE_SWBYPASS",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_switch_mode, enum_switch_mode);


static const char* const enum_dl_rx[] = {
    "DL_RX_DISABLE",
    "DL_RX_DAI0_RX_L",
    "DL_RX_DAI0_RX_R",
    "DL_RX_DAI1_RX_L",
    "DL_RX_DAI1_RX_R",
    "DL_RX_DAI2_RX_L",
    "DL_RX_DAI2_RX_R",
    "DL_RX_DAI3_RX_L",
    "DL_RX_DAI3_RX_R",
    "DL_RX_DMIC_M0_P",
    "DL_RX_DMIC_M0_N",
    "DL_RX_DMIC_M1_P",
    "DL_RX_DMIC_M1_N",
    "DL_RX_DMIC_COMBO_M0_P",
    "DL_RX_DMIC_COMBO_M0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dl_rx, enum_dl_rx);


static const char* const enum_dl_tx[] = {
    "DL_TX_DISABLE",
    "DL_TX_DAI0_TX_L",
    "DL_TX_DAI0_TX_R",
    "DL_TX_DAI1_TX_L",
    "DL_TX_DAI1_TX_R",
    "DL_TX_DAI2_TX_L",
    "DL_TX_DAI2_TX_R",
    "DL_TX_DAI3_TX_L",
    "DL_TX_DAI3_TX_R",
    "DL_TX_DMIC_S0_P",
    "DL_TX_DMIC_S0_N",
    "DL_TX_DMIC_S1_P",
    "DL_TX_DMIC_S1_N",
    "DL_TX_DMIC_COMBO_S0_P",
    "DL_TX_DMIC_COMBO_S0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dl_tx, enum_dl_tx);


static const char* const enum_ul_rx_pri[] = {
    "UL_RX_PRI_DISABLE",
    "UL_RX_PRI_DAI0_RX_L",
    "UL_RX_PRI_DAI0_RX_R",
    "UL_RX_PRI_DAI1_RX_L",
    "UL_RX_PRI_DAI1_RX_R",
    "UL_RX_PRI_DAI2_RX_L",
    "UL_RX_PRI_DAI2_RX_R",
    "UL_RX_PRI_DAI3_RX_L",
    "UL_RX_PRI_DAI3_RX_R",
    "UL_RX_PRI_DMIC_M0_P",
    "UL_RX_PRI_DMIC_M0_N",
    "UL_RX_PRI_DMIC_M1_P",
    "UL_RX_PRI_DMIC_M1_N",
    "UL_RX_PRI_DMIC_COMBO_M0_P",
    "UL_RX_PRI_DMIC_COMBO_M0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_rx_pri, enum_ul_rx_pri);


static const char* const enum_ul_rx_sec0[] = {
    "UL_RX_SEC0_DISABLE",
    "UL_RX_SEC0_DAI0_RX_L",
    "UL_RX_SEC0_DAI0_RX_R",
    "UL_RX_SEC0_DAI1_RX_L",
    "UL_RX_SEC0_DAI1_RX_R",
    "UL_RX_SEC0_DAI2_RX_L",
    "UL_RX_SEC0_DAI2_RX_R",
    "UL_RX_SEC0_DAI3_RX_L",
    "UL_RX_SEC0_DAI3_RX_R",
    "UL_RX_SEC0_DMIC_M0_P",
    "UL_RX_SEC0_DMIC_M0_N",
    "UL_RX_SEC0_DMIC_M1_P",
    "UL_RX_SEC0_DMIC_M1_N",
    "UL_RX_SEC0_DMIC_COMBO_M0_P",
    "UL_RX_SEC0_DMIC_COMBO_M0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_rx_sec0, enum_ul_rx_sec0);


static const char* const enum_ul_rx_sec1[] = {
    "UL_RX_SEC1_DISABLE",
    "UL_RX_SEC1_DAI0_RX_L",
    "UL_RX_SEC1_DAI0_RX_R",
    "UL_RX_SEC1_DAI1_RX_L",
    "UL_RX_SEC1_DAI1_RX_R",
    "UL_RX_SEC1_DAI2_RX_L",
    "UL_RX_SEC1_DAI2_RX_R",
    "UL_RX_SEC1_DAI3_RX_L",
    "UL_RX_SEC1_DAI3_RX_R",
    "UL_RX_SEC1_DMIC_M0_P",
    "UL_RX_SEC1_DMIC_M0_N",
    "UL_RX_SEC1_DMIC_M1_P",
    "UL_RX_SEC1_DMIC_M1_N",
    "UL_RX_SEC1_DMIC_COMBO_M0_P",
    "UL_RX_SEC1_DMIC_COMBO_M0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_rx_sec1, enum_ul_rx_sec1);


static const char* const enum_ul_rx_sec2[] = {
    "UL_RX_SEC2_DISABLE",
    "UL_RX_SEC2_DAI0_RX_L",
    "UL_RX_SEC2_DAI0_RX_R",
    "UL_RX_SEC2_DAI1_RX_L",
    "UL_RX_SEC2_DAI1_RX_R",
    "UL_RX_SEC2_DAI2_RX_L",
    "UL_RX_SEC2_DAI2_RX_R",
    "UL_RX_SEC2_DAI3_RX_L",
    "UL_RX_SEC2_DAI3_RX_R",
    "UL_RX_SEC2_DMIC_M0_P",
    "UL_RX_SEC2_DMIC_M0_N",
    "UL_RX_SEC2_DMIC_M1_P",
    "UL_RX_SEC2_DMIC_M1_N",
    "UL_RX_SEC2_DMIC_COMBO_M0_P",
    "UL_RX_SEC2_DMIC_COMBO_M0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_rx_sec2, enum_ul_rx_sec2);


static const char* const enum_ul_rx_aec[] = {
    "UL_RX_AEC_DISABLE",
    "UL_RX_AEC_DAI0_RX_L",
    "UL_RX_AEC_DAI0_RX_R",
    "UL_RX_AEC_DAI1_RX_L",
    "UL_RX_AEC_DAI1_RX_R",
    "UL_RX_AEC_DAI2_RX_L",
    "UL_RX_AEC_DAI2_RX_R",
    "UL_RX_AEC_DAI3_RX_L",
    "UL_RX_AEC_DAI3_RX_R",
    "UL_RX_AEC_DMIC_M0_P",
    "UL_RX_AEC_DMIC_M0_N",
    "UL_RX_AEC_DMIC_M1_P",
    "UL_RX_AEC_DMIC_M1_N",
    "UL_RX_AEC_DMIC_COMBO_M0_P",
    "UL_RX_AEC_DMIC_COMBO_M0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_rx_aec, enum_ul_rx_aec);


static const char* const enum_ul_tx[] = {
    "UL_TX_DISABLE",
    "UL_TX_DAI0_TX_L",
    "UL_TX_DAI0_TX_R",
    "UL_TX_DAI1_TX_L",
    "UL_TX_DAI1_TX_R",
    "UL_TX_DAI2_TX_L",
    "UL_TX_DAI2_TX_R",
    "UL_TX_DAI3_TX_L",
    "UL_TX_DAI3_TX_R",
    "UL_TX_DMIC_S0_P",
    "UL_TX_DMIC_S0_N",
    "UL_TX_DMIC_S1_P",
    "UL_TX_DMIC_S1_N",
    "UL_TX_DMIC_COMBO_S0_P",
    "UL_TX_DMIC_COMBO_S0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_tx, enum_ul_tx);


static const char* const enum_ul_tx_sidetone[] = {
    "UL_TX_SIDETONE_DISABLE",
    "UL_TX_SIDETONE_DAI0_TX_L",
    "UL_TX_SIDETONE_DAI0_TX_R",
    "UL_TX_SIDETONE_DAI1_TX_L",
    "UL_TX_SIDETONE_DAI1_TX_R",
    "UL_TX_SIDETONE_DAI2_TX_L",
    "UL_TX_SIDETONE_DAI2_TX_R",
    "UL_TX_SIDETONE_DAI3_TX_L",
    "UL_TX_SIDETONE_DAI3_TX_R",
    "UL_TX_SIDETONE_DMIC_S0_P",
    "UL_TX_SIDETONE_DMIC_S0_N",
    "UL_TX_SIDETONE_DMIC_S1_P",
    "UL_TX_SIDETONE_DMIC_S1_N",
    "UL_TX_SIDETONE_DMIC_COMBO_S0_P",
    "UL_TX_SIDETONE_DMIC_COMBO_S0_N",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_ul_tx_sidetone, enum_ul_tx_sidetone);


static const char* const enum_dai_0_mode[] = {
    "DAI_0_MODE_DISABLE",
    "DAI_0_MODE_SLAVE",
    "DAI_0_MODE_MASTER",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_0_mode, enum_dai_0_mode);


static const char* const enum_dai_0_clk_src[] = {
    "DAI_0_CLK_SRC_DISABLE",
    "DAI_0_CLK_SRC_MCLK",
    "DAI_0_CLK_SRC_INTERNAL",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_0_clk_src, enum_dai_0_clk_src);


static const char* const enum_dai_0_clk[] = {
    "DAI_0_CLK_16K",
    "DAI_0_CLK_32K",
    "DAI_0_CLK_48K",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_0_clk, enum_dai_0_clk);


static const char* const enum_dai_0_data_bit[] = {
    "DAI_0_DATA_BIT_32",
    "DAI_0_DATA_BIT_16",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_0_data_bit, enum_dai_0_data_bit);


static const char* const enum_dai_1_mode[] = {
    "DAI_1_MODE_DISABLE",
    "DAI_1_MODE_SLAVE",
    "DAI_1_MODE_MASTER",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_1_mode, enum_dai_1_mode);


static const char* const enum_dai_1_clk_src[] = {
    "DAI_1_CLK_SRC_DISABLE",
    "DAI_1_CLK_SRC_MCLK",
    "DAI_1_CLK_SRC_INTERNAL",
    "DAI_1_CLK_SRC_DAI_0",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_1_clk_src, enum_dai_1_clk_src);


static const char* const enum_dai_1_clk[] = {
    "DAI_1_CLK_16K",
    "DAI_1_CLK_32K",
    "DAI_1_CLK_48K",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_1_clk, enum_dai_1_clk);


static const char* const enum_dai_1_data_bit[] = {
    "DAI_1_DATA_BIT_32",
    "DAI_1_DATA_BIT_16",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_1_data_bit, enum_dai_1_data_bit);


static const char* const enum_dai_2_mode[] = {
    "DAI_2_MODE_DISABLE",
    "DAI_2_MODE_SLAVE",
    "DAI_2_MODE_MASTER",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_2_mode, enum_dai_2_mode);


static const char* const enum_dai_2_clk_src[] = {
    "DAI_2_CLK_SRC_DISABLE",
    "DAI_2_CLK_SRC_MCLK",
    "DAI_2_CLK_SRC_INTERNAL",
    "DAI_2_CLK_SRC_DAI_0",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_2_clk_src, enum_dai_2_clk_src);


static const char* const enum_dai_2_clk[] = {
    "DAI_2_CLK_16K",
    "DAI_2_CLK_32K",
    "DAI_2_CLK_48K",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_2_clk, enum_dai_2_clk);


static const char* const enum_dai_2_data_bit[] = {
    "DAI_2_DATA_BIT_32",
    "DAI_2_DATA_BIT_16",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_2_data_bit, enum_dai_2_data_bit);


static const char* const enum_dai_3_mode[] = {
    "DAI_3_MODE_DISABLE",
    "DAI_3_MODE_SLAVE",
    "DAI_3_MODE_MASTER",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_3_mode, enum_dai_3_mode);


static const char* const enum_dai_3_clk_src[] = {
    "DAI_3_CLK_SRC_DISABLE",
    "DAI_3_CLK_SRC_MCLK",
    "DAI_3_CLK_SRC_INTERNAL",
    "DAI_3_CLK_SRC_DAI_0",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_3_clk_src, enum_dai_3_clk_src);


static const char* const enum_dai_3_clk[] = {
    "DAI_3_CLK_16K",
    "DAI_3_CLK_32K",
    "DAI_3_CLK_48K",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_3_clk, enum_dai_3_clk);


static const char* const enum_dai_3_data_bit[] = {
    "DAI_3_DATA_BIT_32",
    "DAI_3_DATA_BIT_16",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dai_3_data_bit, enum_dai_3_data_bit);


static const char* const enum_dmic_m_clk_src[] = {
    "DMIC_M_CLK_SRC_DMIC_S",
    "DMIC_M_CLK_SRC_MCLK",
    "DMIC_M_CLK_SRC_INTERNAL",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dmic_m_clk_src, enum_dmic_m_clk_src);


static const char* const enum_dmic_input_gain[] = {
    "DMIC_INPUT_GAIN_0_DB",
    "DMIC_INPUT_GAIN_6_DB",
    "DMIC_INPUT_GAIN_12_DB",
    "DMIC_INPUT_GAIN_18_DB",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_dmic_input_gain, enum_dmic_input_gain);


static const char* const enum_hw_bypass_dai_0[] = {
    "HW_BYPASS_DAI_0_DISABLE",
    "HW_BYPASS_DAI_0_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_hw_bypass_dai_0, enum_hw_bypass_dai_0);


static const char* const enum_hw_bypass_dmic_s0[] = {
    "HW_BYPASS_DMIC_S0_DISABLE",
    "HW_BYPASS_DMIC_S0_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_hw_bypass_dmic_s0, enum_hw_bypass_dmic_s0);


static const char* const enum_sw_bypass_en[] = {
    "SW_BYPASS_EN_DISABLE",
    "SW_BYPASS_EN_ENABLE",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_sw_bypass_en, enum_sw_bypass_en);

static int igo_ch_op_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_OP_MODE_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "OP_MODE", IGO_CH_OP_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_op_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_OP_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "OP_MODE", IGO_CH_OP_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_ul_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_NR_UL_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "NR_UL", IGO_CH_NR_UL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_ul_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_UL_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_UL", IGO_CH_NR_UL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_ul_sec_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_ul_sec_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_UL_SEC_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_UL_SEC", IGO_CH_NR_UL_SEC_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_adaptive_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_adaptive_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_ADAPTIVE_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_ADAPTIVE_EN", IGO_CH_NR_ADAPTIVE_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_voice_str_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_NR_VOICE_STR_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "NR_VOICE_STR", IGO_CH_NR_VOICE_STR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_voice_str_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_VOICE_STR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_VOICE_STR", IGO_CH_NR_VOICE_STR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_noisy_level_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_NR_NOISY_LEVEL_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "NR_NOISY_LEVEL", IGO_CH_NR_NOISY_LEVEL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_noisy_level_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_NOISY_LEVEL_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_NOISY_LEVEL", IGO_CH_NR_NOISY_LEVEL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_level_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_NR_LEVEL_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "NR_LEVEL", IGO_CH_NR_LEVEL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_level_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_LEVEL_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_LEVEL", IGO_CH_NR_LEVEL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_EN", IGO_CH_AEC_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aes_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aes_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AES_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AES_EN", IGO_CH_AES_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_bulk_dly_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_bulk_dly_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_BULK_DLY_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_BULK_DLY", IGO_CH_AEC_BULK_DLY_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_status_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_STATUS_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_STATUS", IGO_CH_VAD_STATUS_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_status_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_clear_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_clear_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_CLEAR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_CLEAR", IGO_CH_VAD_CLEAR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_int_mod_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_int_mod_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_INT_MOD_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_INT_MOD", IGO_CH_VAD_INT_MOD_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_int_pin_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_int_pin_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_INT_PIN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_INT_PIN", IGO_CH_VAD_INT_PIN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_keyword_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_KEYWORD_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_KEYWORD", IGO_CH_VAD_KEYWORD_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_keyword_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_key_group_sel_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_key_group_sel_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_KEY_GROUP_SEL_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_KEY_GROUP_SEL", IGO_CH_VAD_KEY_GROUP_SEL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_voice_enhance_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_voice_enhance_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_VOICE_ENHANCE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_VOICE_ENHANCE", IGO_CH_VAD_VOICE_ENHANCE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_voice_enroll_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_voice_enroll_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_VOICE_ENROLL_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_VOICE_ENROLL", IGO_CH_VAD_VOICE_ENROLL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_enroll_cnt_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_ENROLL_CNT_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_ENROLL_CNT", IGO_CH_VAD_ENROLL_CNT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_enroll_cnt_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_enroll_apply_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_ENROLL_APPLY_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_ENROLL_APPLY", IGO_CH_VAD_ENROLL_APPLY_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_enroll_apply_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_ENROLL_APPLY_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_ENROLL_APPLY", IGO_CH_VAD_ENROLL_APPLY_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_enroll_md_sz_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_ENROLL_MD_SZ_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_ENROLL_MD_SZ", IGO_CH_VAD_ENROLL_MD_SZ_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_enroll_md_sz_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_enroll_md_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_ENROLL_MD_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_ENROLL_MD", IGO_CH_VAD_ENROLL_MD_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_enroll_md_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_ENROLL_MD_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_ENROLL_MD", IGO_CH_VAD_ENROLL_MD_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_enroll_rst_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_enroll_rst_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_ENROLL_RST_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_ENROLL_RST", IGO_CH_VAD_ENROLL_RST_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_keyword_hit_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_KEYWORD_HIT_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_KEYWORD_HIT", IGO_CH_VAD_KEYWORD_HIT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_keyword_hit_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_keyword_hit_clear_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_keyword_hit_clear_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_KEYWORD_HIT_CLEAR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_KEYWORD_HIT_CLEAR", IGO_CH_VAD_KEYWORD_HIT_CLEAR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_init_gain_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_init_gain_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_INIT_GAIN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_INIT_GAIN", IGO_CH_VAD_INIT_GAIN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_bg_model_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_bg_model_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_BG_MODEL_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_BG_MODEL_EN", IGO_CH_VAD_BG_MODEL_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_keyword_thr_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_keyword_thr_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_KEYWORD_THR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_KEYWORD_THR", IGO_CH_VAD_KEYWORD_THR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_kws_score_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_KWS_SCORE_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_KWS_SCORE", IGO_CH_VAD_KWS_SCORE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_kws_score_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_voice_score_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_VOICE_SCORE_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_VOICE_SCORE", IGO_CH_VAD_VOICE_SCORE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_voice_score_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_buf_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_VAD_BUF_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "VAD_BUF", IGO_CH_VAD_BUF_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_buf_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_vad_buf_rst_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_buf_rst_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_BUF_RST_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_BUF_RST", IGO_CH_VAD_BUF_RST_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_agc_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_agc_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AGC_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AGC_MODE", IGO_CH_AGC_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_agc_gain_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_agc_gain_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AGC_GAIN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AGC_GAIN", IGO_CH_AGC_GAIN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_asr_gain_max_vol_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_asr_gain_max_vol_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_ASR_GAIN_MAX_VOL_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "ASR_GAIN_MAX_VOL", IGO_CH_ASR_GAIN_MAX_VOL_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_tx_gain_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_tx_gain_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_TX_GAIN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_TX_GAIN", IGO_CH_UL_TX_GAIN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_tx_mute_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_tx_mute_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_TX_MUTE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_TX_MUTE", IGO_CH_UL_TX_MUTE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_EN", IGO_CH_NR_MODE1_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_EN", IGO_CH_NR_MODE2_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_EN", IGO_CH_NR_MODE3_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_floor_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_floor_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_FLOOR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_FLOOR", IGO_CH_NR_MODE1_FLOOR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_od_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_od_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_OD_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_OD", IGO_CH_NR_MODE1_OD_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_thr_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_thr_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_THR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_THR", IGO_CH_NR_MODE1_THR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_smooth_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_smooth_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_SMOOTH_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_SMOOTH_MODE", IGO_CH_NR_MODE1_SMOOTH_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_smooth_floor_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_smooth_floor_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_SMOOTH_FLOOR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_SMOOTH_FLOOR", IGO_CH_NR_MODE1_SMOOTH_FLOOR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_smooth_y_sum_thr_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_smooth_y_sum_thr_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_SMOOTH_Y_SUM_THR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_SMOOTH_Y_SUM_THR", IGO_CH_NR_MODE1_SMOOTH_Y_SUM_THR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_noise_state_small_cnt_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_noise_state_small_cnt_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_NOISE_STATE_SMALL_CNT_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_NOISE_STATE_SMALL_CNT", IGO_CH_NR_MODE1_NOISE_STATE_SMALL_CNT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_noise_state_big_cnt_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_noise_state_big_cnt_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_NOISE_STATE_BIG_CNT_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_NOISE_STATE_BIG_CNT", IGO_CH_NR_MODE1_NOISE_STATE_BIG_CNT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param0_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param0_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM0_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM0", IGO_CH_NR_MODE1_PP_PARAM0_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param1_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param1_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM1_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM1", IGO_CH_NR_MODE1_PP_PARAM1_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param2_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param2_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM2_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM2", IGO_CH_NR_MODE1_PP_PARAM2_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param3_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param3_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM3_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM3", IGO_CH_NR_MODE1_PP_PARAM3_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param4_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param4_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM4_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM4", IGO_CH_NR_MODE1_PP_PARAM4_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param5_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param5_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM5_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM5", IGO_CH_NR_MODE1_PP_PARAM5_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param6_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param6_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM6_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM6", IGO_CH_NR_MODE1_PP_PARAM6_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param7_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param7_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM7_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM7", IGO_CH_NR_MODE1_PP_PARAM7_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param8_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param8_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM8_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM8", IGO_CH_NR_MODE1_PP_PARAM8_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param9_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param9_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM9_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM9", IGO_CH_NR_MODE1_PP_PARAM9_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param10_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param10_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM10_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM10", IGO_CH_NR_MODE1_PP_PARAM10_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param11_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param11_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM11_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM11", IGO_CH_NR_MODE1_PP_PARAM11_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param12_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param12_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM12_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM12", IGO_CH_NR_MODE1_PP_PARAM12_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param13_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param13_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM13_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM13", IGO_CH_NR_MODE1_PP_PARAM13_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param14_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param14_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM14_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM14", IGO_CH_NR_MODE1_PP_PARAM14_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param15_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param15_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM15_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM15", IGO_CH_NR_MODE1_PP_PARAM15_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param16_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param16_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM16_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM16", IGO_CH_NR_MODE1_PP_PARAM16_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param17_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param17_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM17_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM17", IGO_CH_NR_MODE1_PP_PARAM17_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param18_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param18_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM18_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM18", IGO_CH_NR_MODE1_PP_PARAM18_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param19_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param19_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM19_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM19", IGO_CH_NR_MODE1_PP_PARAM19_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param20_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param20_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM20_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM20", IGO_CH_NR_MODE1_PP_PARAM20_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param21_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param21_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM21_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM21", IGO_CH_NR_MODE1_PP_PARAM21_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param22_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param22_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM22_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM22", IGO_CH_NR_MODE1_PP_PARAM22_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param23_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param23_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM23_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM23", IGO_CH_NR_MODE1_PP_PARAM23_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param24_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param24_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM24_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM24", IGO_CH_NR_MODE1_PP_PARAM24_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param25_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param25_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM25_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM25", IGO_CH_NR_MODE1_PP_PARAM25_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param26_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param26_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM26_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM26", IGO_CH_NR_MODE1_PP_PARAM26_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param27_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param27_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM27_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM27", IGO_CH_NR_MODE1_PP_PARAM27_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param28_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param28_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM28_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM28", IGO_CH_NR_MODE1_PP_PARAM28_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param29_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param29_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM29_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM29", IGO_CH_NR_MODE1_PP_PARAM29_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param30_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param30_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM30_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM30", IGO_CH_NR_MODE1_PP_PARAM30_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param31_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param31_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM31_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM31", IGO_CH_NR_MODE1_PP_PARAM31_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param32_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param32_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM32_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM32", IGO_CH_NR_MODE1_PP_PARAM32_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param33_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param33_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM33_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM33", IGO_CH_NR_MODE1_PP_PARAM33_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param34_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param34_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM34_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM34", IGO_CH_NR_MODE1_PP_PARAM34_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param35_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param35_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM35_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM35", IGO_CH_NR_MODE1_PP_PARAM35_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param36_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param36_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM36_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM36", IGO_CH_NR_MODE1_PP_PARAM36_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param37_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param37_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM37_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM37", IGO_CH_NR_MODE1_PP_PARAM37_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param38_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param38_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM38_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM38", IGO_CH_NR_MODE1_PP_PARAM38_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param39_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param39_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM39_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM39", IGO_CH_NR_MODE1_PP_PARAM39_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param40_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param40_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM40_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM40", IGO_CH_NR_MODE1_PP_PARAM40_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param41_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param41_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM41_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM41", IGO_CH_NR_MODE1_PP_PARAM41_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param42_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param42_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM42_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM42", IGO_CH_NR_MODE1_PP_PARAM42_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param43_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param43_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM43_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM43", IGO_CH_NR_MODE1_PP_PARAM43_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param44_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param44_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM44_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM44", IGO_CH_NR_MODE1_PP_PARAM44_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param45_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param45_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM45_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM45", IGO_CH_NR_MODE1_PP_PARAM45_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param46_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param46_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM46_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM46", IGO_CH_NR_MODE1_PP_PARAM46_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param47_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param47_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM47_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM47", IGO_CH_NR_MODE1_PP_PARAM47_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_ul_floor_step_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_ul_floor_step_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_UL_FLOOR_STEP_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_UL_FLOOR_STEP", IGO_CH_NR_MODE1_UL_FLOOR_STEP_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param49_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param49_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM49_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM49", IGO_CH_NR_MODE1_PP_PARAM49_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param50_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param50_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM50_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM50", IGO_CH_NR_MODE1_PP_PARAM50_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param51_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param51_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM51_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM51", IGO_CH_NR_MODE1_PP_PARAM51_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode1_pp_param52_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode1_pp_param52_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE1_PP_PARAM52_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE1_PP_PARAM52", IGO_CH_NR_MODE1_PP_PARAM52_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_floor_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_floor_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_FLOOR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_FLOOR", IGO_CH_NR_MODE2_FLOOR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param0_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param0_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM0_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM0", IGO_CH_NR_MODE2_PARAM0_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param1_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param1_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM1_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM1", IGO_CH_NR_MODE2_PARAM1_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param2_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param2_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM2_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM2", IGO_CH_NR_MODE2_PARAM2_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param3_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param3_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM3_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM3", IGO_CH_NR_MODE2_PARAM3_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param4_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param4_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM4_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM4", IGO_CH_NR_MODE2_PARAM4_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param5_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param5_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM5_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM5", IGO_CH_NR_MODE2_PARAM5_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param6_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param6_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM6_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM6", IGO_CH_NR_MODE2_PARAM6_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param7_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param7_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM7_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM7", IGO_CH_NR_MODE2_PARAM7_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode2_param8_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode2_param8_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE2_PARAM8_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE2_PARAM8", IGO_CH_NR_MODE2_PARAM8_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_floor_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_floor_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_FLOOR_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_FLOOR", IGO_CH_NR_MODE3_FLOOR_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param0_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param0_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM0_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM0", IGO_CH_NR_MODE3_PARAM0_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param1_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param1_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM1_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM1", IGO_CH_NR_MODE3_PARAM1_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param2_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param2_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM2_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM2", IGO_CH_NR_MODE3_PARAM2_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param3_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param3_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM3_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM3", IGO_CH_NR_MODE3_PARAM3_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param4_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param4_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM4_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM4", IGO_CH_NR_MODE3_PARAM4_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param5_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param5_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM5_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM5", IGO_CH_NR_MODE3_PARAM5_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param6_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param6_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM6_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM6", IGO_CH_NR_MODE3_PARAM6_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param7_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param7_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM7_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM7", IGO_CH_NR_MODE3_PARAM7_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param8_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param8_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM8_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM8", IGO_CH_NR_MODE3_PARAM8_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param9_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param9_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM9_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM9", IGO_CH_NR_MODE3_PARAM9_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param10_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param10_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM10_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM10", IGO_CH_NR_MODE3_PARAM10_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param11_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param11_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM11_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM11", IGO_CH_NR_MODE3_PARAM11_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param12_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param12_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM12_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM12", IGO_CH_NR_MODE3_PARAM12_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param13_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param13_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM13_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM13", IGO_CH_NR_MODE3_PARAM13_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param14_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param14_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM14_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM14", IGO_CH_NR_MODE3_PARAM14_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_mode3_param15_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_mode3_param15_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_MODE3_PARAM15_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_MODE3_PARAM15", IGO_CH_NR_MODE3_PARAM15_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_nr_signle_tone_detect_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_nr_signle_tone_detect_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_NR_SIGNLE_TONE_DETECT_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "NR_SIGNLE_TONE_DETECT_EN", IGO_CH_NR_SIGNLE_TONE_DETECT_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param0_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param0_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM0_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM0", IGO_CH_AEC_PARAM0_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param1_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param1_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM1_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM1", IGO_CH_AEC_PARAM1_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param2_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param2_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM2_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM2", IGO_CH_AEC_PARAM2_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param3_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param3_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM3_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM3", IGO_CH_AEC_PARAM3_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param4_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param4_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM4_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM4", IGO_CH_AEC_PARAM4_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param5_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param5_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM5_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM5", IGO_CH_AEC_PARAM5_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param6_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param6_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM6_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM6", IGO_CH_AEC_PARAM6_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param7_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param7_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM7_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM7", IGO_CH_AEC_PARAM7_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param8_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param8_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM8_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM8", IGO_CH_AEC_PARAM8_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param9_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param9_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM9_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM9", IGO_CH_AEC_PARAM9_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param10_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param10_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM10_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM10", IGO_CH_AEC_PARAM10_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param11_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param11_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM11_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM11", IGO_CH_AEC_PARAM11_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_param12_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_param12_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_PARAM12_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_PARAM12", IGO_CH_AEC_PARAM12_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_aec_ref_gain_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_aec_ref_gain_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_AEC_REF_GAIN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "AEC_REF_GAIN", IGO_CH_AEC_REF_GAIN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_vad_kws_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_vad_kws_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_VAD_KWS_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "VAD_KWS_MODE", IGO_CH_VAD_KWS_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dbg_aec_rec_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dbg_aec_rec_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DBG_AEC_REC_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DBG_AEC_REC_EN", IGO_CH_DBG_AEC_REC_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dbg_streaming_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dbg_streaming_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DBG_STREAMING_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DBG_STREAMING", IGO_CH_DBG_STREAMING_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_power_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_POWER_MODE_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "POWER_MODE", IGO_CH_POWER_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_power_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_slow_write(codec->dev, IGO_CH_POWER_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "POWER_MODE", IGO_CH_POWER_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_fw_ver_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_FW_VER_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "FW_VER", IGO_CH_FW_VER_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_fw_ver_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_fw_sub_ver_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_FW_SUB_VER_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "FW_SUB_VER", IGO_CH_FW_SUB_VER_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_fw_sub_ver_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_fw_build_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_FW_BUILD_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "FW_BUILD", IGO_CH_FW_BUILD_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_fw_build_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_chip_id_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_CHIP_ID_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "CHIP_ID", IGO_CH_CHIP_ID_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_chip_id_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_mclk_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_mclk_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_MCLK_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "MCLK", IGO_CH_MCLK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ck_output_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ck_output_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_CK_OUTPUT_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "CK_OUTPUT", IGO_CH_CK_OUTPUT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_cali_status_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_CALI_STATUS_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "CALI_STATUS", IGO_CH_CALI_STATUS_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_cali_status_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_hif_cali_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_hif_cali_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_HIF_CALI_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "HIF_CALI_EN", IGO_CH_HIF_CALI_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_igo_cmd_ver_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_IGO_CMD_VER_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "IGO_CMD_VER", IGO_CH_IGO_CMD_VER_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_igo_cmd_ver_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_crc_check_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_CRC_CHECK_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "CRC_CHECK", IGO_CH_CRC_CHECK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_crc_check_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    return 0;
}

static int igo_ch_switch_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_read(codec->dev, IGO_CH_SWITCH_MODE_ADDR, (unsigned int*)&ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s (0x%08x) : %d ret %d\n", __func__, "SWITCH_MODE", IGO_CH_SWITCH_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_switch_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_SWITCH_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "SWITCH_MODE", IGO_CH_SWITCH_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dl_rx_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dl_rx_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DL_RX_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DL_RX", IGO_CH_DL_RX_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dl_tx_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dl_tx_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DL_TX_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DL_TX", IGO_CH_DL_TX_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_rx_pri_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_rx_pri_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_RX_PRI_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_RX_PRI", IGO_CH_UL_RX_PRI_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_rx_sec0_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_rx_sec0_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_RX_SEC0_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_RX_SEC0", IGO_CH_UL_RX_SEC0_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_rx_sec1_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_rx_sec1_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_RX_SEC1_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_RX_SEC1", IGO_CH_UL_RX_SEC1_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_rx_sec2_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_rx_sec2_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_RX_SEC2_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_RX_SEC2", IGO_CH_UL_RX_SEC2_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_rx_aec_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_rx_aec_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_RX_AEC_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_RX_AEC", IGO_CH_UL_RX_AEC_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_tx_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_tx_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_TX_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_TX", IGO_CH_UL_TX_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_ul_tx_sidetone_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_ul_tx_sidetone_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_UL_TX_SIDETONE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "UL_TX_SIDETONE", IGO_CH_UL_TX_SIDETONE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_0_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_0_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_0_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_0_MODE", IGO_CH_DAI_0_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_0_clk_src_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_0_clk_src_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_0_CLK_SRC_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_0_CLK_SRC", IGO_CH_DAI_0_CLK_SRC_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_0_clk_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_0_clk_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_0_CLK_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_0_CLK", IGO_CH_DAI_0_CLK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_0_data_bit_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_0_data_bit_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_0_DATA_BIT_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_0_DATA_BIT", IGO_CH_DAI_0_DATA_BIT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_1_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_1_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_1_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_1_MODE", IGO_CH_DAI_1_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_1_clk_src_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_1_clk_src_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_1_CLK_SRC_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_1_CLK_SRC", IGO_CH_DAI_1_CLK_SRC_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_1_clk_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_1_clk_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_1_CLK_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_1_CLK", IGO_CH_DAI_1_CLK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_1_data_bit_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_1_data_bit_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_1_DATA_BIT_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_1_DATA_BIT", IGO_CH_DAI_1_DATA_BIT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_2_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_2_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_2_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_2_MODE", IGO_CH_DAI_2_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_2_clk_src_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_2_clk_src_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_2_CLK_SRC_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_2_CLK_SRC", IGO_CH_DAI_2_CLK_SRC_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_2_clk_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_2_clk_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_2_CLK_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_2_CLK", IGO_CH_DAI_2_CLK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_2_data_bit_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_2_data_bit_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_2_DATA_BIT_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_2_DATA_BIT", IGO_CH_DAI_2_DATA_BIT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_3_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_3_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_3_MODE_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_3_MODE", IGO_CH_DAI_3_MODE_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_3_clk_src_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_3_clk_src_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_3_CLK_SRC_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_3_CLK_SRC", IGO_CH_DAI_3_CLK_SRC_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_3_clk_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_3_clk_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_3_CLK_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_3_CLK", IGO_CH_DAI_3_CLK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dai_3_data_bit_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dai_3_data_bit_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DAI_3_DATA_BIT_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DAI_3_DATA_BIT", IGO_CH_DAI_3_DATA_BIT_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dmic_m_clk_src_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dmic_m_clk_src_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DMIC_M_CLK_SRC_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DMIC_M_CLK_SRC", IGO_CH_DMIC_M_CLK_SRC_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dmic_m_bclk_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dmic_m_bclk_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DMIC_M_BCLK_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DMIC_M_BCLK", IGO_CH_DMIC_M_BCLK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dmic_s_bclk_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dmic_s_bclk_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DMIC_S_BCLK_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DMIC_S_BCLK", IGO_CH_DMIC_S_BCLK_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_dmic_input_gain_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_dmic_input_gain_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_DMIC_INPUT_GAIN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "DMIC_INPUT_GAIN", IGO_CH_DMIC_INPUT_GAIN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_hw_bypass_dai_0_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_hw_bypass_dai_0_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_HW_BYPASS_DAI_0_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "HW_BYPASS_DAI_0", IGO_CH_HW_BYPASS_DAI_0_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_hw_bypass_dmic_s0_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_hw_bypass_dmic_s0_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_HW_BYPASS_DMIC_S0_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "HW_BYPASS_DMIC_S0", IGO_CH_HW_BYPASS_DMIC_S0_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static int igo_ch_sw_bypass_en_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    ucontrol->value.integer.value[0] = 0;

    return 0;
}

static int igo_ch_sw_bypass_en_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
    status = igo_ch_write(codec->dev, IGO_CH_SW_BYPASS_EN_ADDR, ucontrol->value.integer.value[0]);
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s (0x%08x) :%d ret %d\n", __func__, "SW_BYPASS_EN", IGO_CH_SW_BYPASS_EN_ADDR, (int)ucontrol->value.integer.value[0], status);

    return 0;
}

static const struct snd_kcontrol_new debussy_snd_controls[] = {
    SOC_ENUM_EXT("IGO OP_MODE", soc_enum_op_mode,
        igo_ch_op_mode_get, igo_ch_op_mode_put),
    SOC_ENUM_EXT("IGO NR_UL", soc_enum_nr_ul,
        igo_ch_nr_ul_get, igo_ch_nr_ul_put),
    SOC_ENUM_EXT("IGO NR_UL_SEC", soc_enum_nr_ul_sec,
        igo_ch_nr_ul_sec_get, igo_ch_nr_ul_sec_put),
    SOC_ENUM_EXT("IGO NR_ADAPTIVE_EN", soc_enum_nr_adaptive_en,
        igo_ch_nr_adaptive_en_get, igo_ch_nr_adaptive_en_put),
    SOC_ENUM_EXT("IGO NR_VOICE_STR", soc_enum_nr_voice_str,
        igo_ch_nr_voice_str_get, igo_ch_nr_voice_str_put),
    SOC_ENUM_EXT("IGO NR_NOISY_LEVEL", soc_enum_nr_noisy_level,
        igo_ch_nr_noisy_level_get, igo_ch_nr_noisy_level_put),
    SOC_ENUM_EXT("IGO NR_LEVEL", soc_enum_nr_level,
        igo_ch_nr_level_get, igo_ch_nr_level_put),
    SOC_ENUM_EXT("IGO AEC_EN", soc_enum_aec_en,
        igo_ch_aec_en_get, igo_ch_aec_en_put),
    SOC_ENUM_EXT("IGO AES_EN", soc_enum_aes_en,
        igo_ch_aes_en_get, igo_ch_aes_en_put),
    SOC_SINGLE_EXT("IGO AEC_BULK_DLY", IGO_CH_AEC_BULK_DLY_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_bulk_dly_get, igo_ch_aec_bulk_dly_put),
    SOC_ENUM_EXT("IGO VAD_STATUS", soc_enum_vad_status,
        igo_ch_vad_status_get, igo_ch_vad_status_put),
    SOC_ENUM_EXT("IGO VAD_CLEAR", soc_enum_vad_clear,
        igo_ch_vad_clear_get, igo_ch_vad_clear_put),
    SOC_ENUM_EXT("IGO VAD_INT_MOD", soc_enum_vad_int_mod,
        igo_ch_vad_int_mod_get, igo_ch_vad_int_mod_put),
    SOC_ENUM_EXT("IGO VAD_INT_PIN", soc_enum_vad_int_pin,
        igo_ch_vad_int_pin_get, igo_ch_vad_int_pin_put),
    SOC_SINGLE_EXT("IGO VAD_KEYWORD", IGO_CH_VAD_KEYWORD_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_keyword_get, igo_ch_vad_keyword_put),
    SOC_ENUM_EXT("IGO VAD_KEY_GROUP_SEL", soc_enum_vad_key_group_sel,
        igo_ch_vad_key_group_sel_get, igo_ch_vad_key_group_sel_put),
    SOC_ENUM_EXT("IGO VAD_VOICE_ENHANCE", soc_enum_vad_voice_enhance,
        igo_ch_vad_voice_enhance_get, igo_ch_vad_voice_enhance_put),
    SOC_ENUM_EXT("IGO VAD_VOICE_ENROLL", soc_enum_vad_voice_enroll,
        igo_ch_vad_voice_enroll_get, igo_ch_vad_voice_enroll_put),
    SOC_ENUM_EXT("IGO VAD_ENROLL_CNT", soc_enum_vad_enroll_cnt,
        igo_ch_vad_enroll_cnt_get, igo_ch_vad_enroll_cnt_put),
    SOC_ENUM_EXT("IGO VAD_ENROLL_APPLY", soc_enum_vad_enroll_apply,
        igo_ch_vad_enroll_apply_get, igo_ch_vad_enroll_apply_put),
    SOC_SINGLE_EXT("IGO VAD_ENROLL_MD_SZ", IGO_CH_VAD_ENROLL_MD_SZ_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_enroll_md_sz_get, igo_ch_vad_enroll_md_sz_put),
    SOC_SINGLE_EXT("IGO VAD_ENROLL_MD", IGO_CH_VAD_ENROLL_MD_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_enroll_md_get, igo_ch_vad_enroll_md_put),
    SOC_ENUM_EXT("IGO VAD_ENROLL_RST", soc_enum_vad_enroll_rst,
        igo_ch_vad_enroll_rst_get, igo_ch_vad_enroll_rst_put),
    SOC_SINGLE_EXT("IGO VAD_KEYWORD_HIT", IGO_CH_VAD_KEYWORD_HIT_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_keyword_hit_get, igo_ch_vad_keyword_hit_put),
    SOC_ENUM_EXT("IGO VAD_KEYWORD_HIT_CLEAR", soc_enum_vad_keyword_hit_clear,
        igo_ch_vad_keyword_hit_clear_get, igo_ch_vad_keyword_hit_clear_put),
    SOC_SINGLE_EXT("IGO VAD_INIT_GAIN", IGO_CH_VAD_INIT_GAIN_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_init_gain_get, igo_ch_vad_init_gain_put),
    SOC_ENUM_EXT("IGO VAD_BG_MODEL_EN", soc_enum_vad_bg_model_en,
        igo_ch_vad_bg_model_en_get, igo_ch_vad_bg_model_en_put),
    SOC_ENUM_EXT("IGO VAD_KEYWORD_THR", soc_enum_vad_keyword_thr,
        igo_ch_vad_keyword_thr_get, igo_ch_vad_keyword_thr_put),
    SOC_SINGLE_EXT("IGO VAD_KWS_SCORE", IGO_CH_VAD_KWS_SCORE_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_kws_score_get, igo_ch_vad_kws_score_put),
    SOC_SINGLE_EXT("IGO VAD_VOICE_SCORE", IGO_CH_VAD_VOICE_SCORE_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_voice_score_get, igo_ch_vad_voice_score_put),
    SOC_SINGLE_EXT("IGO VAD_BUF", IGO_CH_VAD_BUF_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_vad_buf_get, igo_ch_vad_buf_put),
    SOC_ENUM_EXT("IGO VAD_BUF_RST", soc_enum_vad_buf_rst,
        igo_ch_vad_buf_rst_get, igo_ch_vad_buf_rst_put),
    SOC_ENUM_EXT("IGO AGC_MODE", soc_enum_agc_mode,
        igo_ch_agc_mode_get, igo_ch_agc_mode_put),
    SOC_ENUM_EXT("IGO AGC_GAIN", soc_enum_agc_gain,
        igo_ch_agc_gain_get, igo_ch_agc_gain_put),
    SOC_ENUM_EXT("IGO ASR_GAIN_MAX_VOL", soc_enum_asr_gain_max_vol,
        igo_ch_asr_gain_max_vol_get, igo_ch_asr_gain_max_vol_put),
    SOC_ENUM_EXT("IGO UL_TX_GAIN", soc_enum_ul_tx_gain,
        igo_ch_ul_tx_gain_get, igo_ch_ul_tx_gain_put),
    SOC_ENUM_EXT("IGO UL_TX_MUTE", soc_enum_ul_tx_mute,
        igo_ch_ul_tx_mute_get, igo_ch_ul_tx_mute_put),
    SOC_ENUM_EXT("IGO NR_MODE1_EN", soc_enum_nr_mode1_en,
        igo_ch_nr_mode1_en_get, igo_ch_nr_mode1_en_put),
    SOC_ENUM_EXT("IGO NR_MODE2_EN", soc_enum_nr_mode2_en,
        igo_ch_nr_mode2_en_get, igo_ch_nr_mode2_en_put),
    SOC_ENUM_EXT("IGO NR_MODE3_EN", soc_enum_nr_mode3_en,
        igo_ch_nr_mode3_en_get, igo_ch_nr_mode3_en_put),
    SOC_ENUM_EXT("IGO NR_MODE1_FLOOR", soc_enum_nr_mode1_floor,
        igo_ch_nr_mode1_floor_get, igo_ch_nr_mode1_floor_put),
    SOC_ENUM_EXT("IGO NR_MODE1_OD", soc_enum_nr_mode1_od,
        igo_ch_nr_mode1_od_get, igo_ch_nr_mode1_od_put),
    SOC_ENUM_EXT("IGO NR_MODE1_THR", soc_enum_nr_mode1_thr,
        igo_ch_nr_mode1_thr_get, igo_ch_nr_mode1_thr_put),
    SOC_ENUM_EXT("IGO NR_MODE1_SMOOTH_MODE", soc_enum_nr_mode1_smooth_mode,
        igo_ch_nr_mode1_smooth_mode_get, igo_ch_nr_mode1_smooth_mode_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_SMOOTH_FLOOR", IGO_CH_NR_MODE1_SMOOTH_FLOOR_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_smooth_floor_get, igo_ch_nr_mode1_smooth_floor_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_SMOOTH_Y_SUM_THR", IGO_CH_NR_MODE1_SMOOTH_Y_SUM_THR_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_smooth_y_sum_thr_get, igo_ch_nr_mode1_smooth_y_sum_thr_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_NOISE_STATE_SMALL_CNT", IGO_CH_NR_MODE1_NOISE_STATE_SMALL_CNT_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_noise_state_small_cnt_get, igo_ch_nr_mode1_noise_state_small_cnt_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_NOISE_STATE_BIG_CNT", IGO_CH_NR_MODE1_NOISE_STATE_BIG_CNT_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_noise_state_big_cnt_get, igo_ch_nr_mode1_noise_state_big_cnt_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM0", IGO_CH_NR_MODE1_PP_PARAM0_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param0_get, igo_ch_nr_mode1_pp_param0_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM1", IGO_CH_NR_MODE1_PP_PARAM1_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param1_get, igo_ch_nr_mode1_pp_param1_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM2", IGO_CH_NR_MODE1_PP_PARAM2_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param2_get, igo_ch_nr_mode1_pp_param2_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM3", IGO_CH_NR_MODE1_PP_PARAM3_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param3_get, igo_ch_nr_mode1_pp_param3_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM4", IGO_CH_NR_MODE1_PP_PARAM4_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param4_get, igo_ch_nr_mode1_pp_param4_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM5", IGO_CH_NR_MODE1_PP_PARAM5_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param5_get, igo_ch_nr_mode1_pp_param5_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM6", IGO_CH_NR_MODE1_PP_PARAM6_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param6_get, igo_ch_nr_mode1_pp_param6_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM7", IGO_CH_NR_MODE1_PP_PARAM7_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param7_get, igo_ch_nr_mode1_pp_param7_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM8", IGO_CH_NR_MODE1_PP_PARAM8_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param8_get, igo_ch_nr_mode1_pp_param8_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM9", IGO_CH_NR_MODE1_PP_PARAM9_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param9_get, igo_ch_nr_mode1_pp_param9_put),
    SOC_ENUM_EXT("IGO NR_MODE1_PP_PARAM10", soc_enum_nr_mode1_pp_param10,
        igo_ch_nr_mode1_pp_param10_get, igo_ch_nr_mode1_pp_param10_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM11", IGO_CH_NR_MODE1_PP_PARAM11_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param11_get, igo_ch_nr_mode1_pp_param11_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM12", IGO_CH_NR_MODE1_PP_PARAM12_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param12_get, igo_ch_nr_mode1_pp_param12_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM13", IGO_CH_NR_MODE1_PP_PARAM13_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param13_get, igo_ch_nr_mode1_pp_param13_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM14", IGO_CH_NR_MODE1_PP_PARAM14_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param14_get, igo_ch_nr_mode1_pp_param14_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM15", IGO_CH_NR_MODE1_PP_PARAM15_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param15_get, igo_ch_nr_mode1_pp_param15_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM16", IGO_CH_NR_MODE1_PP_PARAM16_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param16_get, igo_ch_nr_mode1_pp_param16_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM17", IGO_CH_NR_MODE1_PP_PARAM17_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param17_get, igo_ch_nr_mode1_pp_param17_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM18", IGO_CH_NR_MODE1_PP_PARAM18_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param18_get, igo_ch_nr_mode1_pp_param18_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM19", IGO_CH_NR_MODE1_PP_PARAM19_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param19_get, igo_ch_nr_mode1_pp_param19_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM20", IGO_CH_NR_MODE1_PP_PARAM20_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param20_get, igo_ch_nr_mode1_pp_param20_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM21", IGO_CH_NR_MODE1_PP_PARAM21_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param21_get, igo_ch_nr_mode1_pp_param21_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM22", IGO_CH_NR_MODE1_PP_PARAM22_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param22_get, igo_ch_nr_mode1_pp_param22_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM23", IGO_CH_NR_MODE1_PP_PARAM23_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param23_get, igo_ch_nr_mode1_pp_param23_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM24", IGO_CH_NR_MODE1_PP_PARAM24_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param24_get, igo_ch_nr_mode1_pp_param24_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM25", IGO_CH_NR_MODE1_PP_PARAM25_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param25_get, igo_ch_nr_mode1_pp_param25_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM26", IGO_CH_NR_MODE1_PP_PARAM26_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param26_get, igo_ch_nr_mode1_pp_param26_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM27", IGO_CH_NR_MODE1_PP_PARAM27_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param27_get, igo_ch_nr_mode1_pp_param27_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM28", IGO_CH_NR_MODE1_PP_PARAM28_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param28_get, igo_ch_nr_mode1_pp_param28_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM29", IGO_CH_NR_MODE1_PP_PARAM29_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param29_get, igo_ch_nr_mode1_pp_param29_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM30", IGO_CH_NR_MODE1_PP_PARAM30_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param30_get, igo_ch_nr_mode1_pp_param30_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM31", IGO_CH_NR_MODE1_PP_PARAM31_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param31_get, igo_ch_nr_mode1_pp_param31_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM32", IGO_CH_NR_MODE1_PP_PARAM32_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param32_get, igo_ch_nr_mode1_pp_param32_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM33", IGO_CH_NR_MODE1_PP_PARAM33_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param33_get, igo_ch_nr_mode1_pp_param33_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM34", IGO_CH_NR_MODE1_PP_PARAM34_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param34_get, igo_ch_nr_mode1_pp_param34_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM35", IGO_CH_NR_MODE1_PP_PARAM35_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param35_get, igo_ch_nr_mode1_pp_param35_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM36", IGO_CH_NR_MODE1_PP_PARAM36_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param36_get, igo_ch_nr_mode1_pp_param36_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM37", IGO_CH_NR_MODE1_PP_PARAM37_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param37_get, igo_ch_nr_mode1_pp_param37_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM38", IGO_CH_NR_MODE1_PP_PARAM38_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param38_get, igo_ch_nr_mode1_pp_param38_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM39", IGO_CH_NR_MODE1_PP_PARAM39_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param39_get, igo_ch_nr_mode1_pp_param39_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM40", IGO_CH_NR_MODE1_PP_PARAM40_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param40_get, igo_ch_nr_mode1_pp_param40_put),
    SOC_ENUM_EXT("IGO NR_MODE1_PP_PARAM41", soc_enum_nr_mode1_pp_param41,
        igo_ch_nr_mode1_pp_param41_get, igo_ch_nr_mode1_pp_param41_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM42", IGO_CH_NR_MODE1_PP_PARAM42_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param42_get, igo_ch_nr_mode1_pp_param42_put),
    SOC_ENUM_EXT("IGO NR_MODE1_PP_PARAM43", soc_enum_nr_mode1_pp_param43,
        igo_ch_nr_mode1_pp_param43_get, igo_ch_nr_mode1_pp_param43_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM44", IGO_CH_NR_MODE1_PP_PARAM44_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param44_get, igo_ch_nr_mode1_pp_param44_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM45", IGO_CH_NR_MODE1_PP_PARAM45_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param45_get, igo_ch_nr_mode1_pp_param45_put),
    SOC_ENUM_EXT("IGO NR_MODE1_PP_PARAM46", soc_enum_nr_mode1_pp_param46,
        igo_ch_nr_mode1_pp_param46_get, igo_ch_nr_mode1_pp_param46_put),
    SOC_ENUM_EXT("IGO NR_MODE1_PP_PARAM47", soc_enum_nr_mode1_pp_param47,
        igo_ch_nr_mode1_pp_param47_get, igo_ch_nr_mode1_pp_param47_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_UL_FLOOR_STEP", IGO_CH_NR_MODE1_UL_FLOOR_STEP_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_ul_floor_step_get, igo_ch_nr_mode1_ul_floor_step_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM49", IGO_CH_NR_MODE1_PP_PARAM49_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param49_get, igo_ch_nr_mode1_pp_param49_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM50", IGO_CH_NR_MODE1_PP_PARAM50_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param50_get, igo_ch_nr_mode1_pp_param50_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM51", IGO_CH_NR_MODE1_PP_PARAM51_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param51_get, igo_ch_nr_mode1_pp_param51_put),
    SOC_SINGLE_EXT("IGO NR_MODE1_PP_PARAM52", IGO_CH_NR_MODE1_PP_PARAM52_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode1_pp_param52_get, igo_ch_nr_mode1_pp_param52_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_FLOOR", IGO_CH_NR_MODE2_FLOOR_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_floor_get, igo_ch_nr_mode2_floor_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM0", IGO_CH_NR_MODE2_PARAM0_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param0_get, igo_ch_nr_mode2_param0_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM1", IGO_CH_NR_MODE2_PARAM1_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param1_get, igo_ch_nr_mode2_param1_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM2", IGO_CH_NR_MODE2_PARAM2_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param2_get, igo_ch_nr_mode2_param2_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM3", IGO_CH_NR_MODE2_PARAM3_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param3_get, igo_ch_nr_mode2_param3_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM4", IGO_CH_NR_MODE2_PARAM4_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param4_get, igo_ch_nr_mode2_param4_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM5", IGO_CH_NR_MODE2_PARAM5_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param5_get, igo_ch_nr_mode2_param5_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM6", IGO_CH_NR_MODE2_PARAM6_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param6_get, igo_ch_nr_mode2_param6_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM7", IGO_CH_NR_MODE2_PARAM7_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param7_get, igo_ch_nr_mode2_param7_put),
    SOC_SINGLE_EXT("IGO NR_MODE2_PARAM8", IGO_CH_NR_MODE2_PARAM8_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode2_param8_get, igo_ch_nr_mode2_param8_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_FLOOR", IGO_CH_NR_MODE3_FLOOR_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_floor_get, igo_ch_nr_mode3_floor_put),
    SOC_ENUM_EXT("IGO NR_MODE3_PARAM0", soc_enum_nr_mode3_param0,
        igo_ch_nr_mode3_param0_get, igo_ch_nr_mode3_param0_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM1", IGO_CH_NR_MODE3_PARAM1_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param1_get, igo_ch_nr_mode3_param1_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM2", IGO_CH_NR_MODE3_PARAM2_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param2_get, igo_ch_nr_mode3_param2_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM3", IGO_CH_NR_MODE3_PARAM3_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param3_get, igo_ch_nr_mode3_param3_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM4", IGO_CH_NR_MODE3_PARAM4_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param4_get, igo_ch_nr_mode3_param4_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM5", IGO_CH_NR_MODE3_PARAM5_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param5_get, igo_ch_nr_mode3_param5_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM6", IGO_CH_NR_MODE3_PARAM6_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param6_get, igo_ch_nr_mode3_param6_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM7", IGO_CH_NR_MODE3_PARAM7_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param7_get, igo_ch_nr_mode3_param7_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM8", IGO_CH_NR_MODE3_PARAM8_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param8_get, igo_ch_nr_mode3_param8_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM9", IGO_CH_NR_MODE3_PARAM9_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param9_get, igo_ch_nr_mode3_param9_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM10", IGO_CH_NR_MODE3_PARAM10_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param10_get, igo_ch_nr_mode3_param10_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM11", IGO_CH_NR_MODE3_PARAM11_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param11_get, igo_ch_nr_mode3_param11_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM12", IGO_CH_NR_MODE3_PARAM12_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param12_get, igo_ch_nr_mode3_param12_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM13", IGO_CH_NR_MODE3_PARAM13_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param13_get, igo_ch_nr_mode3_param13_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM14", IGO_CH_NR_MODE3_PARAM14_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param14_get, igo_ch_nr_mode3_param14_put),
    SOC_SINGLE_EXT("IGO NR_MODE3_PARAM15", IGO_CH_NR_MODE3_PARAM15_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_nr_mode3_param15_get, igo_ch_nr_mode3_param15_put),
    SOC_ENUM_EXT("IGO NR_SIGNLE_TONE_DETECT_EN", soc_enum_nr_signle_tone_detect_en,
        igo_ch_nr_signle_tone_detect_en_get, igo_ch_nr_signle_tone_detect_en_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM0", IGO_CH_AEC_PARAM0_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param0_get, igo_ch_aec_param0_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM1", IGO_CH_AEC_PARAM1_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param1_get, igo_ch_aec_param1_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM2", IGO_CH_AEC_PARAM2_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param2_get, igo_ch_aec_param2_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM3", IGO_CH_AEC_PARAM3_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param3_get, igo_ch_aec_param3_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM4", IGO_CH_AEC_PARAM4_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param4_get, igo_ch_aec_param4_put),
    SOC_ENUM_EXT("IGO AEC_PARAM5", soc_enum_aec_param5,
        igo_ch_aec_param5_get, igo_ch_aec_param5_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM6", IGO_CH_AEC_PARAM6_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param6_get, igo_ch_aec_param6_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM7", IGO_CH_AEC_PARAM7_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param7_get, igo_ch_aec_param7_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM8", IGO_CH_AEC_PARAM8_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param8_get, igo_ch_aec_param8_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM9", IGO_CH_AEC_PARAM9_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param9_get, igo_ch_aec_param9_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM10", IGO_CH_AEC_PARAM10_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param10_get, igo_ch_aec_param10_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM11", IGO_CH_AEC_PARAM11_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param11_get, igo_ch_aec_param11_put),
    SOC_SINGLE_EXT("IGO AEC_PARAM12", IGO_CH_AEC_PARAM12_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_aec_param12_get, igo_ch_aec_param12_put),
    SOC_ENUM_EXT("IGO AEC_REF_GAIN", soc_enum_aec_ref_gain,
        igo_ch_aec_ref_gain_get, igo_ch_aec_ref_gain_put),
    SOC_ENUM_EXT("IGO VAD_KWS_MODE", soc_enum_vad_kws_mode,
        igo_ch_vad_kws_mode_get, igo_ch_vad_kws_mode_put),
    SOC_ENUM_EXT("IGO DBG_AEC_REC_EN", soc_enum_dbg_aec_rec_en,
        igo_ch_dbg_aec_rec_en_get, igo_ch_dbg_aec_rec_en_put),
    SOC_ENUM_EXT("IGO DBG_STREAMING", soc_enum_dbg_streaming,
        igo_ch_dbg_streaming_get, igo_ch_dbg_streaming_put),
    SOC_ENUM_EXT("IGO POWER_MODE", soc_enum_power_mode,
        igo_ch_power_mode_get, igo_ch_power_mode_put),
    SOC_SINGLE_EXT("IGO FW_VER", IGO_CH_FW_VER_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_fw_ver_get, igo_ch_fw_ver_put),
    SOC_SINGLE_EXT("IGO FW_SUB_VER", IGO_CH_FW_SUB_VER_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_fw_sub_ver_get, igo_ch_fw_sub_ver_put),
    SOC_SINGLE_EXT("IGO FW_BUILD", IGO_CH_FW_BUILD_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_fw_build_get, igo_ch_fw_build_put),
    SOC_SINGLE_EXT("IGO CHIP_ID", IGO_CH_CHIP_ID_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_chip_id_get, igo_ch_chip_id_put),
    SOC_SINGLE_EXT("IGO MCLK", IGO_CH_MCLK_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_mclk_get, igo_ch_mclk_put),
    SOC_ENUM_EXT("IGO CK_OUTPUT", soc_enum_ck_output,
        igo_ch_ck_output_get, igo_ch_ck_output_put),
    SOC_ENUM_EXT("IGO CALI_STATUS", soc_enum_cali_status,
        igo_ch_cali_status_get, igo_ch_cali_status_put),
    SOC_ENUM_EXT("IGO HIF_CALI_EN", soc_enum_hif_cali_en,
        igo_ch_hif_cali_en_get, igo_ch_hif_cali_en_put),
    SOC_SINGLE_EXT("IGO IGO_CMD_VER", IGO_CH_IGO_CMD_VER_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_igo_cmd_ver_get, igo_ch_igo_cmd_ver_put),
    SOC_ENUM_EXT("IGO CRC_CHECK", soc_enum_crc_check,
        igo_ch_crc_check_get, igo_ch_crc_check_put),
    SOC_ENUM_EXT("IGO SWITCH_MODE", soc_enum_switch_mode,
        igo_ch_switch_mode_get, igo_ch_switch_mode_put),
    SOC_ENUM_EXT("IGO DL_RX", soc_enum_dl_rx,
        igo_ch_dl_rx_get, igo_ch_dl_rx_put),
    SOC_ENUM_EXT("IGO DL_TX", soc_enum_dl_tx,
        igo_ch_dl_tx_get, igo_ch_dl_tx_put),
    SOC_ENUM_EXT("IGO UL_RX_PRI", soc_enum_ul_rx_pri,
        igo_ch_ul_rx_pri_get, igo_ch_ul_rx_pri_put),
    SOC_ENUM_EXT("IGO UL_RX_SEC0", soc_enum_ul_rx_sec0,
        igo_ch_ul_rx_sec0_get, igo_ch_ul_rx_sec0_put),
    SOC_ENUM_EXT("IGO UL_RX_SEC1", soc_enum_ul_rx_sec1,
        igo_ch_ul_rx_sec1_get, igo_ch_ul_rx_sec1_put),
    SOC_ENUM_EXT("IGO UL_RX_SEC2", soc_enum_ul_rx_sec2,
        igo_ch_ul_rx_sec2_get, igo_ch_ul_rx_sec2_put),
    SOC_ENUM_EXT("IGO UL_RX_AEC", soc_enum_ul_rx_aec,
        igo_ch_ul_rx_aec_get, igo_ch_ul_rx_aec_put),
    SOC_ENUM_EXT("IGO UL_TX", soc_enum_ul_tx,
        igo_ch_ul_tx_get, igo_ch_ul_tx_put),
    SOC_ENUM_EXT("IGO UL_TX_SIDETONE", soc_enum_ul_tx_sidetone,
        igo_ch_ul_tx_sidetone_get, igo_ch_ul_tx_sidetone_put),
    SOC_ENUM_EXT("IGO DAI_0_MODE", soc_enum_dai_0_mode,
        igo_ch_dai_0_mode_get, igo_ch_dai_0_mode_put),
    SOC_ENUM_EXT("IGO DAI_0_CLK_SRC", soc_enum_dai_0_clk_src,
        igo_ch_dai_0_clk_src_get, igo_ch_dai_0_clk_src_put),
    SOC_ENUM_EXT("IGO DAI_0_CLK", soc_enum_dai_0_clk,
        igo_ch_dai_0_clk_get, igo_ch_dai_0_clk_put),
    SOC_ENUM_EXT("IGO DAI_0_DATA_BIT", soc_enum_dai_0_data_bit,
        igo_ch_dai_0_data_bit_get, igo_ch_dai_0_data_bit_put),
    SOC_ENUM_EXT("IGO DAI_1_MODE", soc_enum_dai_1_mode,
        igo_ch_dai_1_mode_get, igo_ch_dai_1_mode_put),
    SOC_ENUM_EXT("IGO DAI_1_CLK_SRC", soc_enum_dai_1_clk_src,
        igo_ch_dai_1_clk_src_get, igo_ch_dai_1_clk_src_put),
    SOC_ENUM_EXT("IGO DAI_1_CLK", soc_enum_dai_1_clk,
        igo_ch_dai_1_clk_get, igo_ch_dai_1_clk_put),
    SOC_ENUM_EXT("IGO DAI_1_DATA_BIT", soc_enum_dai_1_data_bit,
        igo_ch_dai_1_data_bit_get, igo_ch_dai_1_data_bit_put),
    SOC_ENUM_EXT("IGO DAI_2_MODE", soc_enum_dai_2_mode,
        igo_ch_dai_2_mode_get, igo_ch_dai_2_mode_put),
    SOC_ENUM_EXT("IGO DAI_2_CLK_SRC", soc_enum_dai_2_clk_src,
        igo_ch_dai_2_clk_src_get, igo_ch_dai_2_clk_src_put),
    SOC_ENUM_EXT("IGO DAI_2_CLK", soc_enum_dai_2_clk,
        igo_ch_dai_2_clk_get, igo_ch_dai_2_clk_put),
    SOC_ENUM_EXT("IGO DAI_2_DATA_BIT", soc_enum_dai_2_data_bit,
        igo_ch_dai_2_data_bit_get, igo_ch_dai_2_data_bit_put),
    SOC_ENUM_EXT("IGO DAI_3_MODE", soc_enum_dai_3_mode,
        igo_ch_dai_3_mode_get, igo_ch_dai_3_mode_put),
    SOC_ENUM_EXT("IGO DAI_3_CLK_SRC", soc_enum_dai_3_clk_src,
        igo_ch_dai_3_clk_src_get, igo_ch_dai_3_clk_src_put),
    SOC_ENUM_EXT("IGO DAI_3_CLK", soc_enum_dai_3_clk,
        igo_ch_dai_3_clk_get, igo_ch_dai_3_clk_put),
    SOC_ENUM_EXT("IGO DAI_3_DATA_BIT", soc_enum_dai_3_data_bit,
        igo_ch_dai_3_data_bit_get, igo_ch_dai_3_data_bit_put),
    SOC_ENUM_EXT("IGO DMIC_M_CLK_SRC", soc_enum_dmic_m_clk_src,
        igo_ch_dmic_m_clk_src_get, igo_ch_dmic_m_clk_src_put),
    SOC_SINGLE_EXT("IGO DMIC_M_BCLK", IGO_CH_DMIC_M_BCLK_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_dmic_m_bclk_get, igo_ch_dmic_m_bclk_put),
    SOC_SINGLE_EXT("IGO DMIC_S_BCLK", IGO_CH_DMIC_S_BCLK_ADDR,
        IGO_CH_SHIFT, IGO_CH_MAX, IGO_CH_NO_INVERT,
        igo_ch_dmic_s_bclk_get, igo_ch_dmic_s_bclk_put),
    SOC_ENUM_EXT("IGO DMIC_INPUT_GAIN", soc_enum_dmic_input_gain,
        igo_ch_dmic_input_gain_get, igo_ch_dmic_input_gain_put),
    SOC_ENUM_EXT("IGO HW_BYPASS_DAI_0", soc_enum_hw_bypass_dai_0,
        igo_ch_hw_bypass_dai_0_get, igo_ch_hw_bypass_dai_0_put),
    SOC_ENUM_EXT("IGO HW_BYPASS_DMIC_S0", soc_enum_hw_bypass_dmic_s0,
        igo_ch_hw_bypass_dmic_s0_get, igo_ch_hw_bypass_dmic_s0_put),
    SOC_ENUM_EXT("IGO SW_BYPASS_EN", soc_enum_sw_bypass_en,
        igo_ch_sw_bypass_en_get, igo_ch_sw_bypass_en_put),
};

void debussy_add_codec_controls(struct snd_soc_codec* codec)
{
    snd_soc_add_codec_controls(codec,
        debussy_snd_controls,
        ARRAY_SIZE(debussy_snd_controls));

        snd_soc_add_codec_controls(codec,
        debussy_ext_controls,
        ARRAY_SIZE(debussy_ext_controls));
}