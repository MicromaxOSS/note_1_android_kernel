/*****************************************************************************
 *
 * Filename:
 * ---------
 *    prize_charge_limit.c
 *
 * Project:
 * --------
 *   Android_Software
 *
 * Description:
 * ------------
 * This Module defines functions of the Anroid Battery service for
 * updating the battery status
 *
 * Author:
 * -------
 * sunshuai
 *
 ****************************************************************************/
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
	 
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/mtk_boot.h>
#include <pmic.h>
#include <mtk_gauge_time_service.h>

#include <mt-plat/mtk_charger.h>
#include "mtk_charger_intf.h"
#include "mtk_charger_init.h"
#include "prize_charge_limit.h"

#define prize_printk(fmt, arg...)  printk("prize_charge_limit : %s : " fmt, __FUNCTION__ ,##arg);

static struct prize_charge_limit_manager *limit_pinfo;

#if (CONFIG_MTK_GAUGE_VERSION == 30)
static struct charger_consumer *prize_consumer;
#endif

static bool is_probe_done = false;

#if defined(CONFIG_MTK_CW2015_SUPPORT)
extern int g_cw2015_capacity;
extern int g_cw2015_vol;
extern int cw2015_exit_flag;
#endif


void prize_charger_check_step(struct charger_manager *info){
	int temperature;
	
	if(is_probe_done == false)
		return;
	
	limit_pinfo->temperature = info->battery_temp;
    temperature = limit_pinfo->temperature;

    if(temperature >= limit_pinfo->start_step1_temp && temperature < limit_pinfo->start_step2_temp){
	   if(limit_pinfo->current_step != STEP_T1){
	       limit_pinfo->current_step = STEP_T1;
		   charger_manager_enable_high_voltage_charging(prize_consumer, false);
		   limit_pinfo->change_cv_cap_capacity = 0;
	   }
    }

	if (temperature >= limit_pinfo->start_step2_temp && temperature < limit_pinfo->start_step3_temp){
		if(limit_pinfo->current_step != STEP_T2){
          limit_pinfo->current_step = STEP_T2;
		  charger_manager_enable_high_voltage_charging(prize_consumer, true);
		  limit_pinfo->change_cv_cap_capacity = 0;
		}
    }

	if (temperature >= limit_pinfo->start_step3_temp){
		if(limit_pinfo->current_step != STEP_T3){
          limit_pinfo->current_step = STEP_T3;
		  charger_manager_enable_high_voltage_charging(prize_consumer, false);
		  limit_pinfo->change_cv_cap_capacity = 0;
		}
    }
	
	prize_printk("%s: current_step [%d] temperature[%d]\n", __func__,limit_pinfo->current_step,temperature);
}
EXPORT_SYMBOL(prize_charger_check_step);

void prize_set_charge_limit(struct charger_data *pdata){

	if(is_probe_done == false)
		return;

	if(limit_pinfo->current_step == STEP_T1){
	   if(pdata->charging_current_limit > limit_pinfo->step1_max_current)
          pdata->charging_current_limit = limit_pinfo->step1_max_current;
	}

	if(limit_pinfo->current_step == STEP_T3){
	   if(pdata->charging_current_limit > limit_pinfo->step3_vot1_current)
	   	 pdata->charging_current_limit = limit_pinfo->step3_vot1_current;  	
	}

	prize_printk("%s: current_step [%d] charging_current_limit[%d]\n", __func__,limit_pinfo->current_step,pdata->charging_current_limit);
}
EXPORT_SYMBOL(prize_set_charge_limit);


u32 prize_get_cv_limit(struct charger_manager *info){
	u32 constant_voltage = 0;
	
	if(is_probe_done == false)
		return info->data.battery_cv;

	constant_voltage = info->data.battery_cv;
    if(limit_pinfo->current_step == STEP_T3){
	   if((g_cw2015_vol > limit_pinfo->temp_stp3_cv_voltage) && (limit_pinfo->change_cv_cap_capacity ==0))
	   	   limit_pinfo->change_cv_cap_capacity = g_cw2015_capacity;

	   if(limit_pinfo->change_cv_cap_capacity != 0)
	   	    constant_voltage = limit_pinfo->temp_stp3_cv_voltage*1000;
    }

	prize_printk("%s: constant_voltage [%d] battery_cv [%d] g_cw2015_vol[%d] change_cv_cap_capacity [%d]\n", __func__,constant_voltage,info->data.battery_cv,g_cw2015_vol,limit_pinfo->change_cv_cap_capacity);
	return constant_voltage;
		
}
EXPORT_SYMBOL(prize_get_cv_limit);

void reset_prize_limit_info(void){
	
	if(is_probe_done == false)
		return;

	limit_pinfo->change_cv_cap_capacity = 0;
	limit_pinfo->current_step = STEP_INIT;
}
	
static int charge_limit_probe(struct platform_device *pdev)
{
 	struct prize_charge_limit_manager *limit_info = NULL;
    struct device_node *np = NULL;
	u32 val;
	
	prize_printk("%s: starts\n", __func__);

	limit_info = devm_kzalloc(&pdev->dev, sizeof(*limit_info), GFP_KERNEL);
	if (!limit_info)
		return -ENOMEM;
	limit_pinfo = limit_info;

	np = pdev->dev.of_node;

	if (!np) {
		prize_printk("%s: no device node\n", __func__);
		return -EINVAL;
	}

	prize_consumer = charger_manager_get_by_name(&pdev->dev, "charger");

	if (of_property_read_u32(np, "start_step1_temp", &val) >= 0) {
			limit_info->start_step1_temp = val;
		} else {
			prize_printk(
				"use default start_step1_temp:%d\n", -5);
			limit_info->start_step1_temp = -5;
	}

	if (of_property_read_u32(np, "start_step2_temp", &val) >= 0) {
			limit_info->start_step2_temp = val;
	} else {
			prize_printk(
				"use default start_step2_temp:%d\n", 15);
			limit_info->start_step2_temp = 15;
	}

	if (of_property_read_u32(np, "start_step3_temp", &val) >= 0) {
			limit_info->start_step3_temp = val;
	} else {
			prize_printk(
				"use default start_step3_temp:%d\n", 45);
			limit_info->start_step3_temp = 45;
	}

	if (of_property_read_u32(np, "step1_max_current", &val) >= 0) {
			limit_info->step1_max_current = val;
	} else {
			prize_printk(
				"use default end_step1_max_current %d\n", 1260000);
			limit_info->step1_max_current = 1260000;
	}

	if (of_property_read_u32(np, "step3_vot1_current", &val) >= 0) {
			limit_info->step3_vot1_current = val;
	} else {
			prize_printk(
				"use default step3_vot1_current %d\n", 1470000);
			limit_info->step3_vot1_current = 1470000;
	}


    if (of_property_read_u32(np, "temp_stp3_cv_voltage", &val) >= 0) {
			limit_info->temp_stp3_cv_voltage = val;
	} else {
			prize_printk(
				"use default temp_stp3_cv_voltage:%d\n", 4350000);
			limit_info->temp_stp3_cv_voltage = 4350000;
	}

    limit_info->current_step = STEP_INIT;
	limit_info->change_cv_cap_capacity = 0;
	is_probe_done =true;
	
	prize_printk("info->step_info.start_step1_temp:%d\n", limit_info->start_step1_temp);
	prize_printk("info->step_info.start_step2_temp:%d\n", limit_info->start_step2_temp);
	prize_printk("info->step_info.start_step3_temp:%d\n", limit_info->start_step3_temp);
	prize_printk("info->step_info.step1_max_current:%d\n", limit_info->step1_max_current);
	prize_printk("info->step_info.step3_vot1_current:%d\n", limit_info->step3_vot1_current);
	prize_printk("info->step_info.temp_stp3_cv_voltage:%d\n", limit_info->temp_stp3_cv_voltage);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id charge_limit_of_match[] = {
	{.compatible = "prize,charge_limit",},
	{},
};
MODULE_DEVICE_TABLE(of, charge_limit_of_match);
#endif


static struct platform_driver charge_limit_driver = {
	.probe = charge_limit_probe,
	.remove = NULL,
	.driver = {
		.name = "charge_limit",
#ifdef CONFIG_OF
		.of_match_table = charge_limit_of_match,
#endif
	},
};

static int __init charge_limit_init(void)
{
	int ret;

	ret = platform_driver_register(&charge_limit_driver);

	chr_err("[%s] Initialization : DONE\n",
		__func__);

	return 0;
}

static void __exit charge_limit_exit(void)
{

}
module_init(charge_limit_init);
module_exit(charge_limit_exit);

MODULE_AUTHOR("sunshuai");
MODULE_DESCRIPTION("Charge Limit");
MODULE_LICENSE("GPL");
