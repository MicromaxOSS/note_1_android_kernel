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

#ifndef DEBUSSY_CUSTOMER_H

#define DEBUSSY_CUSTOMER_H

#include "debussy_snd_ctrl.h"

typedef struct ST_IGO_DEBUSSY_CFG {
    uint32_t    cmd_addr;
    uint32_t    cmd_data;
} st_igo_scenarios_cfg_table;

enum scenarios_sel_check{
    SCENARIOS_STANDBY_CHECK = 0,
    SCENARIOS_HANDSET_NR_CHECK = 1,
    SCENARIOS_HANDSET_BYPASS_CHECK = 2,
    SCENARIOS_SEL_VAD_CHECK = 3,
    SCENARIOS_SEL_BARGEIN_CHECK = 4,
    SCENARIOS_MAX_CHECK = 5
};
enum voice_mode{
	VOICE_END = 0,
	InPhoneCall = 1,
	InRecord = 2,
	InVoip = 3,
	InGoogleVoice = 4,
	SREEN_OFF = 5,
	SREEN_ON = 6,
};

enum ext_ctrl_table{
    EXT_CTRL_VOICE_MODE = 0,

    EXT_CTRL_MAX        // <- always keep in the last line
};

extern const struct snd_kcontrol_new debussy_ext_controls[EXT_CTRL_MAX];

extern void debussy_power_enable(int);
extern void debussy_mic_bias_enable(int);
extern void debussy_bb_clk_enable(int);
void debussy_kws_hit(struct debussy_priv* debussy);
extern void debussy_dts_table_cus(struct debussy_priv*, struct device_node *);
extern struct ST_IGO_DEBUSSY_CFG *mode_check_table[];

#endif
