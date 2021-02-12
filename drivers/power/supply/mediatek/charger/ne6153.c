
/************************************************************                            $
*
* file: ne6153_wireless_power.c 
*
* Description: AP to ne6153 IIC firmware
*
*------------------------------------------------------------

*************************************************************/
/************************************************************
*
*SGM2541 Set RX_EN:L,FLAG:L = Manual mode
*SGM2541 Set EN:L = Auto mode
*			When read FLAG high, means that we can enable OTG
*			by set FLAG low.
*
*	***MUST set pin default stat as usb_power in pl lk***
*
*************************************************************/

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/delay.h>
#include <linux/regmap.h>
//#include <linux/qpnp/qpnp-adc.h>
#include <linux/pinctrl/consumer.h>
//#include <linux/qpnp/qpnp-revid.h>

#include "ne6153.h"

#define PRINTK(fmt,args...) printk("NE6153:"fmt,##args)
#define PRINTK_ERR(fmt,args...) printk(KERN_ERR"NE6153:"fmt,##args)

struct dev_info {
	struct i2c_client *client;
	struct device *dev;
    struct regmap *regmap;
	int irq;
	
	struct workqueue_struct *queue;
	struct work_struct wrx_chk_set_work;
	struct hrtimer wrx_chk_set_timer;
	
	struct pinctrl *pctl_wrx;
	//struct pinctrl_state *pst_default;
	//struct pinctrl_state *pst_wrx_en_in;
	//struct pinctrl_state *pst_wrx_en_l;
	//struct pinctrl_state *pst_sw_flag_h;
	//struct pinctrl_state *pst_sw_flag_l;
//prize-add-pengzhipeng-20191016-start
#if defined(CONFIG_PRIZE_NE6153_SUPPORT) || defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
	int otg_en_gpio;
#endif
//prize-add-pengzhipeng-20191016-end
	int vout_try_cnt;
	int target_vout;
	int soc;
	int vout_tune_state;	//0:idle 1:tuning 2:success 3:done fail
};

DEFINE_MUTEX(switch_lock);
DEFINE_MUTEX(ne6153_lock);

static struct dev_info dev_info = {
	.queue = NULL,
	.vout_try_cnt = 0,
	.target_vout = 0,
	.soc = 0,
	.vout_tune_state = 0,
};
static struct dev_info *info = &dev_info;

//static int sw_sel_pending = 0;
static int vout_tune_pending = 0;
#define VOUT_RETRY_TIMES 10
//prize-add-pengzhipeng-20191016-start

#if defined(CONFIG_PRIZE_NE6153_SUPPORT) || defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
int set_otg_gpio(int en)
{

	int ret = 0;
	
	if (gpio_is_valid(info->otg_en_gpio)) {
		if (en)
			gpio_set_value_cansleep(info->otg_en_gpio, 1);
		else
			gpio_set_value_cansleep(info->otg_en_gpio, 0);
	}
	else
	{
		ret = -1;
	}
	return ret;
}

EXPORT_SYMBOL(set_otg_gpio);
#endif
//prize-add-pengzhipeng-20191016-end
static int wrx_read(struct dev_info *di, u16 reg, u8 *val) {
    unsigned int temp = 0;
    int ret = 0;

    ret = regmap_read(di->regmap, reg, &temp);
	if (ret){
		dev_err(di->dev,"read 0x%x fail\n",reg);
	}else{
		*val = (u8)temp;
	}
    return ret;
}

static inline int wrx_write(struct dev_info *di, u16 reg, u8 val) {
    int ret = 0;

    ret = regmap_write(di->regmap, reg, val);
    if (ret)
        dev_err(di->dev, "ne6153 write error: %d\n", ret);

    return ret;
}

static inline int wrx_read_buffer(struct dev_info *di, u16 reg, u8 *buf, u32 size) {
    return regmap_bulk_read(di->regmap, reg, buf, size);
}

__maybe_unused static int wrx_write_buffer(struct dev_info *di, u16 reg, u8 *buf, u32 size) {
    int ret = 0;

    while (size--) {
        ret = wrx_write(di, reg++, *buf++);
        if (ret < 0) {
            dev_err(di->dev, "write error: %d\n", ret);
            return ret;
        }
    }

    return ret;
}

/*start prize added by sunshuai, 20190213,open wrx_disable_vout*/
int wrx_disable_vout()
{

	int ret = 0;
	u8 reg = 0;
	ret = wrx_read(info, REG_STATUS, &reg);
	if (!ret){
		if (reg & 0x80){
			ret = wrx_write(info, REG_COMMAND, 0x01<<1);
		}
	}
	return ret;
}
EXPORT_SYMBOL(wrx_disable_vout);
/*end prize added by sunshuai, 20190213,open wrx_disable_vout*/



static int wrx_chk_alive(){
	int ret = 0;
	u8 reg = 0;
	ret = wrx_read(info, 0x00, &reg);
	PRINTK("wrx_chk_alive ret =%d\n",ret);
	if (ret){
		return -EINVAL;
	}
	if (reg == 0x53){
		return 1;
	}
	wrx_read(info, 0x02, &reg);
	PRINTK("reg 0x02 = 0x%x\n",reg);
	return 0;
}

static int wrx_get_tx_type(){
	int ret = 0;
	u8 reg = 0;
	
	ret = wrx_read(info, REG_STATUS, &reg);
	if (!ret){
		ret = (reg>>4)&0b11;
		if (ret == 0){
			ret = TX_TYPE_NONE;
		}else if (ret == 1){
			ret = TX_TYPE_EPP;
		}else if (ret == 2){
			ret = TX_TYPE_SAMSUNG;
		}else if (ret == 3){
			ret = TX_TYPE_BPP;
		}else{
			ret = TX_TYPE_NONE;
		}
		return ret;
	}
	return TX_TYPE_UNKNOWN;
}

static int wrx_get_vout(){
	
	int ret = 0;
	u8 val[2] = {0};
	
	ret = wrx_read_buffer(info, REG_ADC_VOUT, val, 2);
	if (ret){
		return 0;
	}
	ret = val[1]<<8|val[0];
	PRINTK("val0 0x%x,val1 0x%x,ret 0x%x\n",val[0],val[1],ret);
	return (ret*6*2100)>>12;//reg*6*2.1*1000/4096;
}

static int wrx_set_vout(int mv){
	
	int ret = 0;
	int reg = 0;
	u8 temp = 0;
	
	if (mv < 4000){
		mv = 4000;
	}
	if (mv > 9000){
		mv = 9000;
	}
	reg = (mv - 3500)/100;
	ret = wrx_write(info, REG_VOUT_SET, reg);
	if (ret){
		return ret;
	}
	
	wrx_read(info, REG_VOUT_SET, &temp);

	return (reg == (int)temp)?0:-EINVAL;
}

//prize added by sunshuai Modification did not reach the high temperature 55 standard stop charging problem for ne6153 201900413 start 
bool get_ne6153_ischarge_9V(void)
{
   int real_vout;
    if(wrx_chk_alive()!= 1){//add by sunshuai for judge 6153 is alive
		PRINTK("RX not alive\n");
		return false;
    }

    real_vout = wrx_get_vout();
	PRINTK("wrx_chk_set_work_func   wrx_get_vout = <%d> info->target_vout = <%d>\n",real_vout,info->target_vout);

	if(info->target_vout <= 0 || real_vout <=0)//add by sunshuai for judge 6153 is up 9V
	{
	   	PRINTK("target_vout is 0 or read 6153 voltage is 0\n");
		return false;
	}

	if ((real_vout >= info->target_vout-1000)&&(real_vout <= info->target_vout+1000)){
		PRINTK("tune vout success <%d>\n",real_vout);
		return true;
	}
	else{
		return false;
	}
}
EXPORT_SYMBOL(get_ne6153_ischarge_9V);
//prize added by sunshuai Modification did not reach the high temperature 55 standard stop charging problem for ne6153 201900413 end 



void wrx_chk_set_work_func(struct work_struct *work){
	//read i2c get status
	int ret = 0;
	u8 val = 0;
	ktime_t ktime;
	int tx_type = 0;
	int real_vout;
	PRINTK("wrx_chk_set_work_func enter\n");
	if (!wrx_chk_alive()){
		ktime = ktime_set(5, 0);
		hrtimer_start(&info->wrx_chk_set_timer, ktime, HRTIMER_MODE_REL );
		PRINTK("RX not alive\n");
		return;
	}
	
	//tune vout
	ret = wrx_read(info, REG_STATUS, &val);
	if (!ret){
		PRINTK("wrx_chk_set_work_func REG_STATUS 0x%x\n",val);
		if (val & 0x80){
			//output on
			tx_type = (val & 0x030)>>4;
			if (tx_type >= 2){
				ret = wrx_set_vout(info->target_vout);
			}else{
				ret = -1;
			}
		}else{
			ret = -1;
		}
	}
	
	//chk tune vout result
	if (!ret){
		mdelay(1000);
		real_vout = wrx_get_vout();

		PRINTK("wrx_chk_set_work_func   wrx_get_vout = <%d> info->target_vout = <%d>\n",real_vout,info->target_vout);
		
		if ((real_vout >= info->target_vout-1000)&&(real_vout <= info->target_vout+1000)){
			PRINTK("tune vout success <%d>\n",real_vout);
			info->vout_tune_state = 2;
			return; //prize added by sunshuai, wireless charge  20190128,Read confirmation voltage setting is successful
		}
		else{
			//prize added by sunshuai, wireless charge  20190128,When the set voltage is successful but the
			// read voltage value is not up to standard, try to set the voltage again.
			ret = -1;
		}
	}
	
	//fail
	if (ret){
		ktime = ktime_set(5, 0);
		hrtimer_start(&info->wrx_chk_set_timer, ktime, HRTIMER_MODE_REL );
	}
}

static enum hrtimer_restart wrx_chk_set_timeout(struct hrtimer *timer){
	
	if (info->vout_try_cnt++ < VOUT_RETRY_TIMES){
		queue_work(info->queue,&info->wrx_chk_set_work);
	}else{
		PRINTK("vout tune finish\n");
		info->vout_tune_state = 3;
	}

    return HRTIMER_NORESTART;
}
#if 0
int ne6153_sw_sel_usb(int is_usb){
	int ret = 0;
	mutex_lock(&switch_lock);
	if (!IS_ERR(info->pctl_wrx)){
		if (is_usb == 0){	//set to wrx
			if (!IS_ERR(info->pst_sw_flag_l)){
				pinctrl_select_state(info->pctl_wrx,info->pst_sw_flag_l);
			}else{
				ret = -EINVAL;
			}
		}else{	//Power path set to USB
			if (!IS_ERR(info->pst_sw_flag_h)){
				wrx_disable_vout();
				pinctrl_select_state(info->pctl_wrx,info->pst_sw_flag_h);
			}else{
				ret = -EINVAL;
			}
		}
	}else{
		sw_sel_pending = is_usb;
		ret = -EINVAL;
	}
	mutex_unlock(&switch_lock);
	return ret;
}
EXPORT_SYMBOL(ne6153_sw_sel_usb);
#endif

int ne6153_chk_set_vout(int mv){
	
	PRINTK("ne6153_chk_set_vout %d\n",mv);
	mutex_lock(&ne6153_lock);
	if (info->queue != NULL){
		PRINTK("ne6153_chk_set_vout info->queue != NULL\n");
		hrtimer_cancel(&info->wrx_chk_set_timer);
		cancel_work_sync(&info->wrx_chk_set_work);
		queue_work(info->queue,&info->wrx_chk_set_work);
	}else{
		vout_tune_pending = 1;
		PRINTK("ne6153_chk_set_vout vout_tune_pending =1\n");
	}
	info->vout_try_cnt = 0;
	info->vout_tune_state = 1;
	info->target_vout = mv;
	mutex_unlock(&ne6153_lock);
	return 0;
}
EXPORT_SYMBOL(ne6153_chk_set_vout);

int ne6153_get_max_tx_cap(){
	
	enum tx_type type = 0;
	int ret = 0;
	
	type = wrx_get_tx_type();
	switch(type){
		case TX_TYPE_NONE: ret = 5; break;
		case TX_TYPE_EPP: ret = 5; break;
		case TX_TYPE_SAMSUNG: ret = 10; break;
		case TX_TYPE_BPP: ret = 15; break;
		default: ret = 5;
	}
	return ret;
}
EXPORT_SYMBOL(ne6153_get_max_tx_cap);

int ne6153_get_tune_state(){
	
	return info->vout_tune_state;
}
EXPORT_SYMBOL(ne6153_get_tune_state);

// first step: define regmap_config
static const struct regmap_config ne6153_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .max_register = 0xFFFF,
};
//prize-add-pengzhipeng-20191016-start
static int ne6153_parse_dt(struct dev_info *dev_info,struct device_node *np)
{
    dev_info->otg_en_gpio = of_get_named_gpio(np, "en-gpio", 0);
    if (dev_info->otg_en_gpio < 0) {
        printk("%s: no reset gpio provided, will not HW reset device\n", __func__);
        return -1;
    } else {
        printk( "%s: reset gpio provided ok\n", __func__);
    }
 
    return 0;
}
//prize-add-pengzhipeng-20191016-end

static int ne6153_probe(struct i2c_client *client, const struct i2c_device_id *id) {

	//struct pinctrl_state *pstate_default;
//prize-add-pengzhipeng-20191016-start
#if defined(CONFIG_PRIZE_NE6153_SUPPORT) || defined(CONFIG_PRIZE_WIRELESS_RECEIVER_MAXIC_MT5715)
	int ret;
	struct device_node *np = client->dev.of_node;

	if (np) {
	   ret = ne6153_parse_dt(info,np);
	   if(ret)
		   return ret;
	}
	else
	{
		info->otg_en_gpio = -1;
	}
	
	if (gpio_is_valid(info->otg_en_gpio)) {
        ret = devm_gpio_request_one(&client->dev, info->otg_en_gpio,
              GPIOF_OUT_INIT_LOW, "otg_en_rst");
        if(ret)
		   return ret;
    }
#endif
//prize-add-pengzhipeng-20191016-end
	PRINTK("%s\n",__func__);

	hrtimer_init(&info->wrx_chk_set_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	info->wrx_chk_set_timer.function = wrx_chk_set_timeout;
	
	INIT_WORK(&info->wrx_chk_set_work,wrx_chk_set_work_func);
	info->queue = create_singlethread_workqueue("wrx_work_queue");

    info->regmap = regmap_init_i2c(client, &ne6153_regmap_config);
    if (!info->regmap) {
        pr_err("parent regmap is missing\n");
        goto ERR_I2C;
    }
    info->client = client;
    info->dev = &client->dev;

    device_init_wakeup(info->dev, true);

	mutex_lock(&ne6153_lock);
	if (vout_tune_pending){
		hrtimer_cancel(&info->wrx_chk_set_timer);
		queue_work(info->queue,&info->wrx_chk_set_work);
	}
	mutex_unlock(&ne6153_lock);
	
    return 0;
ERR_I2C:
	return -EINVAL;
}

static int ne6153_remove(struct i2c_client *client) {
//prize-add-pengzhipeng-20191016-start
	if (gpio_is_valid(info->otg_en_gpio))
        devm_gpio_free(&client->dev, info->otg_en_gpio);
//prize-add-pengzhipeng-20191016-end
    return 0;
}

static const struct of_device_id match_table[] = {
    {.compatible = "newedge,ne6153",},
    { },
};

static const struct i2c_device_id ne6153_dev_id[] = {
    {"ne6153", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, ne6153_dev_id);

static struct i2c_driver ne6153_driver = {
    .driver   = {
        .name           = "ne6153wchg",
        .owner          = THIS_MODULE,
        .of_match_table = match_table,
    },
    .probe    = ne6153_probe,
    .remove   = ne6153_remove,
    .id_table = ne6153_dev_id,
};
module_i2c_driver(ne6153_driver);

MODULE_AUTHOR("HR");
MODULE_DESCRIPTION("ne6153 Wireless Power Charger Monitor driver");
MODULE_LICENSE("GPL v2");
