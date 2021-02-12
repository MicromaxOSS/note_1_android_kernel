/*
 * Copyright (C) 2016 PRIZE.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __PRIZE_CHARGER_LIMIT_H__
#define __PRIZE_CHARGER_LIMIT_H__


enum charge_temperature_state_enum {
	STEP_INIT = 0,
	STEP_T1,
	STEP_T2,
	STEP_T3
};
extern u32 prize_get_cv_limit(struct charger_manager *info);
extern void prize_charger_check_step(struct charger_manager *info);
extern void prize_set_charge_limit(struct charger_data *pdata);
extern void reset_prize_limit_info(void);

struct prize_charge_limit_manager{
	int current_step;
	int start_step1_temp;
	int step1_max_current;
	int start_step2_temp;
	int start_step3_temp;
	int step3_vot1_current;
    int temp_stp3_cv_voltage;
    int temperature;
	int change_cv_cap_capacity;
};
#endif /* __PRIZE_CHARGER_LIMIT_H__ */
