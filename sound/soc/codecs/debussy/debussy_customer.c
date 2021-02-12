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

#include <linux/input.h> 
#include <linux/delay.h>
#include "debussy.h"
#include "debussy_customer.h"

//prize added by huarui 20200416 start
static int dvdd_1v2_gpio = -1;
struct debussy_priv *cus_debussy = NULL;
struct pinctrl *pinctrl = NULL;
struct pinctrl_state *aud_i2s_state = NULL;
struct pinctrl_state *aud_gpio_state = NULL;
//prize added by huarui 20200416 end

#ifdef ENABLE_FACTORY_CHECK
static struct ST_IGO_DEBUSSY_CFG standby_check_cfg[] = {
    {IGO_CH_POWER_MODE_ADDR, POWER_MODE_STANDBY},
    {IGO_CH_WAIT_ADDR, POWER_MODE_DELAY_CHECK},
    {IGO_CH_WAIT_ADDR, 0xFFFFFFFF}
};
	
static struct ST_IGO_DEBUSSY_CFG handset_nr_check_cfg[] = {
	{IGO_CH_POWER_MODE_ADDR,POWER_MODE_STANDBY},
	{IGO_CH_POWER_MODE_ADDR,POWER_MODE_WORKING},
	{IGO_CH_DAI_0_CLK_ADDR,DAI_0_CLK_16K},
	{IGO_CH_DAI_0_MODE_ADDR,DAI_0_MODE_SLAVE},
	{IGO_CH_DAI_0_DATA_BIT_ADDR,DAI_0_DATA_BIT_16},
	{IGO_CH_DMIC_M_CLK_SRC_ADDR,DMIC_M_CLK_SRC_MCLK},
	{IGO_CH_MCLK_ADDR, 12288},
	{IGO_CH_DMIC_M_BCLK_ADDR},
	{IGO_CH_UL_RX_PRI_ADDR,UL_RX_PRI_DMIC_M0_P},
	{IGO_CH_UL_TX_ADDR,UL_TX_DAI0_TX_L},
	{IGO_CH_NR_MODE1_OD_ADDR,NR_MODE1_OD_LVL_0},
	{IGO_CH_NR_MODE1_THR_ADDR,NR_MODE1_THR_LVL_1},
	{IGO_CH_NR_MODE1_FLOOR_ADDR,NR_MODE1_FLOOR_LVL_0},
	{IGO_CH_NR_UL_ADDR,NR_UL_LV_1},
	{IGO_CH_OP_MODE_ADDR,OP_MODE_NR}
};

static struct ST_IGO_DEBUSSY_CFG handset_bypass_check_cfg[] = {
    {IGO_CH_POWER_MODE_ADDR,POWER_MODE_WORKING},
	{IGO_CH_WAIT_ADDR, POWER_MODE_DELAY_CHECK},      
	{IGO_CH_MCLK_ADDR,12288000},
	{IGO_CH_DMIC_M_CLK_SRC_ADDR,DMIC_M_CLK_SRC_MCLK},
	{IGO_CH_DMIC_M_BCLK_ADDR,1536000},
	{IGO_CH_UL_RX_PRI_ADDR,UL_RX_PRI_DMIC_M0_P},
	{IGO_CH_UL_TX_ADDR,UL_TX_DAI1_TX_L},
	{IGO_CH_NR_UL_ADDR,NR_UL_LV_1},
	{IGO_CH_DAI_1_CLK_ADDR,DAI_1_CLK_48K},
	{IGO_CH_DAI_1_MODE_ADDR,DAI_1_MODE_SLAVE},
	{IGO_CH_DAI_1_DATA_BIT_ADDR,DAI_1_DATA_BIT_32},
	{IGO_CH_SW_BYPASS_EN_ADDR,SW_BYPASS_EN_ENABLE},
	{IGO_CH_OP_MODE_ADDR,OP_MODE_BYPASS},
	{IGO_CH_WAIT_ADDR, 0xFFFFFFFF}
};

static struct ST_IGO_DEBUSSY_CFG handset_vad_check_cfg[] = {
    {IGO_CH_POWER_MODE_ADDR,POWER_MODE_WORKING},
	{IGO_CH_WAIT_ADDR, POWER_MODE_DELAY_CHECK},
	{IGO_CH_DMIC_M_CLK_SRC_ADDR,DMIC_M_CLK_SRC_INTERNAL},
	{IGO_CH_DMIC_M_BCLK_ADDR,768000},
	{IGO_CH_UL_RX_PRI_ADDR,UL_RX_PRI_DMIC_M0_P},
	{IGO_CH_VAD_INT_PIN_ADDR,VAD_INT_PIN_DAI2_RXDAT},
	{IGO_CH_VAD_INT_MOD_ADDR,VAD_INT_MOD_EDGE},
	{IGO_CH_VAD_VOICE_ENHANCE_ADDR,VAD_VOICE_ENHANCE_ENABLE},
	{IGO_CH_OP_MODE_ADDR,OP_MODE_VAD},
    {IGO_CH_WAIT_ADDR, 0xFFFFFFFF}
};
	
static struct ST_IGO_DEBUSSY_CFG handset_bargein_check_cfg[] = {
    {IGO_CH_POWER_MODE_ADDR,POWER_MODE_WORKING},
	{IGO_CH_WAIT_ADDR, POWER_MODE_DELAY_CHECK},
	{IGO_CH_DMIC_M_BCLK_ADDR,1228800},
	{IGO_CH_DAI_1_CLK_ADDR,DAI_1_CLK_48K},
	{IGO_CH_DAI_1_MODE_ADDR,DAI_1_MODE_SLAVE},
	{IGO_CH_DAI_1_DATA_BIT_ADDR,DAI_1_DATA_BIT_32},

	{IGO_CH_DAI_0_CLK_ADDR,DAI_0_CLK_48K},
	{IGO_CH_DAI_0_MODE_ADDR,DAI_0_MODE_SLAVE},
	{IGO_CH_DAI_0_DATA_BIT_ADDR,DAI_0_DATA_BIT_32},

	{IGO_CH_UL_RX_PRI_ADDR,UL_RX_PRI_DMIC_M0_P},
	{IGO_CH_UL_TX_ADDR,UL_TX_DAI1_TX_L},
	{IGO_CH_NR_UL_ADDR,NR_UL_LV_1},

	{IGO_CH_UL_RX_AEC_ADDR,UL_RX_AEC_DAI0_RX_L},

	{IGO_CH_VAD_INT_PIN_ADDR,VAD_INT_PIN_DAI2_RXDAT},
	{IGO_CH_VAD_INT_MOD_ADDR,VAD_INT_MOD_EDGE},
	{IGO_CH_AEC_BULK_DLY_ADDR,100} ,
	{IGO_CH_AEC_EN_ADDR,AEC_EN_ENABLE},
	{IGO_CH_OP_MODE_ADDR,OP_MODE_BARGEIN},
    {IGO_CH_WAIT_ADDR, 0xFFFFFFFF}
};


struct ST_IGO_DEBUSSY_CFG *mode_check_table[] = {
    &standby_check_cfg[0],                    
    &handset_nr_check_cfg[0],
    &handset_bypass_check_cfg[0],
    &handset_vad_check_cfg[0],
    &handset_bargein_check_cfg[0]
};
#endif  /* end of ENABLE_FACTORY_CHECK */
	
void debussy_power_enable(int enable) {
    if (enable) {
        // Turn on Power for IG
		//prize added by huarui 20200416 start
		if (gpio_is_valid(dvdd_1v2_gpio)) {
            gpio_direction_output(dvdd_1v2_gpio, GPIO_HIGH);
        }
        else {
            dev_err(cus_debussy->dev, "dvdd_1v2_gpio is invalid\n");
        }
		//prize added by huarui 20200416 end
    }
    else {
        // Turn off power for IG
		//prize added by huarui 20200416 start
		if (gpio_is_valid(dvdd_1v2_gpio)) {
            gpio_direction_output(dvdd_1v2_gpio, GPIO_LOW);
        }
        else {
            dev_err(cus_debussy->dev, "dvdd_1v2_gpio is invalid\n");
        }
		//prize added by huarui 20200416 end
    }
}

void debussy_mic_bias_enable(int enable) {
    if (0 != enable) {
        // Turnon MIC BIAS

    }
    else {
        // Turnoff MIC BIAS

    }
}

void debussy_bb_clk_enable(int enable) {
    if (0 != enable) {
        // Enable BB MCLK Output, like 19.2MHz
    }
    else {
        // Disable BB MCLK Output, like 19.2MHz
    }
}

void debussy_kws_hit(struct debussy_priv* debussy) {
    dev_info(debussy->dev, "%s(+)#######input_report_key#######\n",__func__);
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,13,11)
    input_report_key(debussy->input_dev, SOUND_TRIGGER_KEY, 1);
    input_sync(debussy->input_dev);
    msleep(1);
    input_report_key(debussy->input_dev, SOUND_TRIGGER_KEY, 0);
    input_sync(debussy->input_dev);
#endif
    dev_info(debussy->dev, "%s(-)\n",__func__);
}

void debussy_dts_table_cus(struct debussy_priv *debussy, struct device_node *node) {
//prize added by huarui 20200416 start
	int ret;
	
	cus_debussy = debussy;
	dvdd_1v2_gpio = of_get_named_gpio(node, "ig,dvdd-1v2-gpio", 0);
    if (dvdd_1v2_gpio < 0) {
        dev_err(debussy->dev, "Unable to get \"ig,dvdd-1v2-gpio\"\n");
    }
    else {
        dev_info(debussy->dev, "dvdd_1v2_gpio = %d\n", dvdd_1v2_gpio);

        if (gpio_is_valid(dvdd_1v2_gpio)) {
            if (0 == gpio_request(dvdd_1v2_gpio, "IGO_DVDD_1V2")) {
                //gpio_direction_output(dvdd_1v2_gpio, GPIO_HIGH);
            }
            else {
                dev_err(debussy->dev, "IGO_DVDD_1V2: gpio_request fail\n");
            }
        }
        else {
            dev_err(debussy->dev, "dvdd_1v2_gpio is invalid\n");
        }
    }
	
	pinctrl = devm_pinctrl_get(debussy->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err("Failed to get debussy pinctrl. %d\n",ret);
		//return ret;
	}
	aud_gpio_state = pinctrl_lookup_state(pinctrl,"aud_pins_i2s1_off");
	if (IS_ERR(aud_gpio_state)) {
		ret = PTR_ERR(aud_gpio_state);
		pr_err("Failed to init (aud_pins_i2s1_off) %d\n",ret);
	}
	aud_i2s_state = pinctrl_lookup_state(pinctrl,"aud_pins_i2s1_on");
	if (IS_ERR(aud_i2s_state)) {
		ret = PTR_ERR(aud_i2s_state);
		pr_err("Failed to init (aud_pins_i2s1_on) %d\n",ret);
	}else{		
		ret = pinctrl_select_state(pinctrl,aud_i2s_state);
	}
//prize added by huarui 20200416 end
}



static const char* const enum_voice_mode[] = {
    "VOICE_END",
    "InPhoneCall",
    "InRecord",
    "InVoip",
    "InGoogleVoice",
    "SREEN_OFF",
    "SREEN_ON",
};
static SOC_ENUM_SINGLE_EXT_DECL(soc_enum_voice_mode, enum_voice_mode);



static int igo_ch_voice_mode_get(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    //int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);
    mutex_lock(&debussy->igo_ch_lock);
    ucontrol->value.integer.value[0] = debussy->voice_mode;
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: read %s = %d \n", __func__, "VOICE_MODE", (int)ucontrol->value.integer.value[0]);

    return 0;
}

static int igo_ch_voice_mode_put(struct snd_kcontrol* kcontrol,
    struct snd_ctl_elem_value* ucontrol)
{
    struct snd_soc_codec* codec = snd_soc_kcontrol_codec(kcontrol);
    //int status = IGO_CH_STATUS_DONE;

    struct debussy_priv* debussy;
    debussy = dev_get_drvdata(codec->dev);

    mutex_lock(&debussy->igo_ch_lock);
	debussy->voice_mode = ucontrol->value.integer.value[0];

#if (SEND_VAD_CMD_IN_DRIVER==1)
{
    unsigned int vadModeStage = atomic_read(&debussy->vad_mode_stage);

    if (1 == atomic_read(&debussy->vad_switch_flag))
    {
        if (debussy->voice_mode == SREEN_OFF)
        {
            if (VAD_MODE_ST_DISABLE == vadModeStage ||
                VAD_MODE_ST_ENABLE == vadModeStage ||
                VAD_MODE_ST_BYPASS == vadModeStage)
            {
                dev_info(debussy->dev, "%s(+)================>\n", __func__);
                debussy->set_vad_cmd(debussy->dev);
                dev_info(debussy->dev, "%s(-)================>\n", __func__);
            }    
        }     
        else if(debussy->voice_mode == SREEN_ON && VAD_MODE_ST_ENABLE == vadModeStage)
        {
            debussy->reset_chip(debussy->dev, 1);
            atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_DISABLE);
            dev_info(codec->dev, "%s: vad_mode_stage => 0\n", __func__);
        }

        if (debussy->voice_mode == VOICE_END)
        {
            atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_DISABLE);
        }
    }

}
#endif  /* end of SEND_VAD_CMD_IN_DRIVER */    

    if (debussy->voice_mode == VOICE_END)
    {
        debussy->chip_pull_down(debussy->dev);
    }
    mutex_unlock(&debussy->igo_ch_lock);
    dev_info(codec->dev, "%s: write %s = %d \n", __func__, "VOICE_MODE", (int)ucontrol->value.integer.value[0]);

    return 0;
}


/* do remember to add enum to ext_ctrl_table */
/* in debussy_customer.h when add new EXT IGO CMD */
const struct snd_kcontrol_new debussy_ext_controls[] = {
	SOC_ENUM_EXT("IGO VOICE_MODE", soc_enum_voice_mode,
        igo_ch_voice_mode_get, igo_ch_voice_mode_put),
};



