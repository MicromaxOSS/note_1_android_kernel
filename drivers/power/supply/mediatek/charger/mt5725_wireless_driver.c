/******************************************************************************
* function: linux test platform driver
*        
* file  MT5725 wireless driver .c
*        
* author  Yangwl@maxictech.com  11/5/2018
* 
* interrupt and ldo control by author lifenfen@szprize.com  1/2/2019 
******************************************************************************/
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/timer.h>

#include <linux/delay.h>
#include <linux/kernel.h>

#include <linux/poll.h>

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>
#include <linux/types.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
//#include <linux/wakelock.h>
//#include <extcon_usb.h>

#include "mt5725_wireless.h"

#define DEVICE_NAME  "mt5725_iic"

enum REG_INDEX
{
	CHIPID = 0,
	VOUT,
	INT_FLAG,
	INTCTLR,
	VOUTSET,
	VFC,
	CMD,
	INDEX_MAX,
};

/* REG RW permission */
#define REG_NONE_ACCESS 0
#define REG_RD_ACCESS  (1 << 0)
#define REG_WR_ACCESS  (1 << 1)
#define REG_BIT_ACCESS  (1 << 2)
#define REG_MAX         0x0F
/* REG RW permission */

struct reg_attr {
	const char *name;
	u16 addr;
	u8 flag;
};

static struct reg_attr reg_access[INDEX_MAX] = {
    [CHIPID] = {"CHIPID", REG_CHIPID, REG_RD_ACCESS},
    [VOUT] = {"VOUT", REG_VOUT, REG_RD_ACCESS},
    [INT_FLAG] = {"INT_FLAG", REG_INT_FLAG,REG_RD_ACCESS},
    [INTCTLR] = {"INTCLR", REG_INTCLR,REG_WR_ACCESS},
    [VOUTSET] = {"VOUTSET", REG_VOUTSET, REG_RD_ACCESS|REG_WR_ACCESS},
    [VFC] = {"VFC", REG_VFC, REG_RD_ACCESS|REG_WR_ACCESS},
    [CMD] = {"CMD", REG_CMD, REG_RD_ACCESS|REG_WR_ACCESS|REG_BIT_ACCESS},
};

/* INT status reg 0x08 */
typedef enum {
    INT_BC_RECV   = 0x0001,
    INT_LDO_ON    = 0x0002,
    INT_LDO_OFF   = 0x0004,
    INT_OVP_FLAG  = 0x0008,
    INT_OCP_FLAG  = 0x0010,
    INT_PLDO_FLAG = 0x0020,
    INT_AVDD5V_ON = 0x0040,
    INT_AVDD5V_OFF = 0x0080,
    INT_AFC_ON    = 0x0100,
} IntType;
/* INT status reg 0x08 */

/* 
	notice:	little endian;
	value 9000 = 0x2328, ptr[1] = 0x23, ptr[0] = 0x28;
	i2c buffer byte is from index 0, big endian
*/
typedef union{
    u16 value;
    u8  ptr[2];
}vuc;

struct MT5725_dev *mte;
int is_5715_probe_done = 0;

struct MT5725_func {
    int (*read)(struct MT5725_dev *di, u16 reg, u8 *val);
    int (*write)(struct MT5725_dev *di, u16 reg, u8 val);
    int (*read_buf)(struct MT5725_dev *di,
                    u16 reg, u8 *buf, u32 size);
    int (*write_buf)(struct MT5725_dev *di,
                     u16 reg, u8 *buf, u32 size);
};

struct MT5725_dev {
    char                *name;
    struct i2c_client    *client;
    struct device       *dev;
    struct regmap       *regmap;
    struct MT5725_func  bus;
    int irq_gpio;
    int trxset_gpio;
	int otgen_gpio;
	int pwrst_gpio;
	int chipen_gpio;
    struct mutex slock;
    //struct wake_lock wakelock;
    struct delayed_work  eint_work;
    int ldo_status;
	int is_samsung_charge;
	struct mutex ops_mutex;
	atomic_t is_tx_mode;
};

int fast_sv(int temp);


bool MT5725_good_status(void)
{
    pr_err("%s: gpio_get_value(mte->pwrst_gpio) =%d\n", __func__,gpio_get_value(mte->pwrst_gpio));
	return !!(gpio_get_value(mte->pwrst_gpio));
}
EXPORT_SYMBOL(MT5725_good_status);

//by lpp add 20190710 start
bool MT5725_wired_switch_wireless(void)
{
    //printk("LPP----mte->otgen_gpio=%d\n",mte->otgen_gpio);
	return gpio_get_value(mte->otgen_gpio);
}
EXPORT_SYMBOL(MT5725_wired_switch_wireless);
//by lpp add 20190710 start

/* ic power status */
static bool MT5725_power_status(void)
{
   	return !!(gpio_get_value(mte->pwrst_gpio));
}
/* ic power status */

/* platform i2c operation */
int MT5725_read(struct MT5725_dev *di, u16 reg, u8 *val)
{
    unsigned int temp;
    int rc;

    if (MT5725_power_status()) {
	    rc = regmap_read(di->regmap, reg, &temp);
	    if (rc >= 0)
	        *val = (u8)temp;
    } else {
	    pr_err("%s: charger is off state\n", __func__);
	    return -1;
    }
    return rc;
}

int MT5725_write(struct MT5725_dev *di, u16 reg, u8 val)
{
    int rc = 0;

    if (MT5725_power_status()) {
	    rc = regmap_write(di->regmap, reg, val);
	    if (rc < 0)
	        dev_err(di->dev, "NE6153 write error: %d\n", rc);
    } else {
	    pr_err("%s: charger is off state\n", __func__);
	    return -1;
    }

    return rc;
}

int MT5725_read_buffer(struct MT5725_dev *di, u16 reg, u8 *buf, u32 size)
{
	if (MT5725_power_status()) {
		return regmap_bulk_read(di->regmap, reg, buf, size);
	} else {
		pr_err("%s: charger is off state\n", __func__);
		return -1;
	}
}

int MT5725_write_buffer(struct MT5725_dev *di, u16 reg, u8 *buf, u32 size)
{
    int rc = 0;

	#if 0
	/* debug */
	if (size == 2)
		pr_err("%s: 0x%02x%02x\n", __func__, buf[0], buf[1]);
	#endif

    if (MT5725_power_status()) {
    	while (size--) {
    		rc = di->bus.write(di, reg++, *buf++);
    		if (rc < 0) {
    			dev_err(di->dev, "write error: %d\n", rc);
    			return rc;
    		}
    	}
    } else {
    	pr_err("%s: charger is off state\n", __func__);
    	return -1;
    }

    return rc;
}


int MT5725_read_buffer_without_pstate(struct MT5725_dev* di, u16 reg, u8* buf, u32 size)
{
    return regmap_bulk_read(di->regmap, reg, buf, size);
}


int MT5725_write_buffer_without_pstate(struct MT5725_dev* di, u16 reg, u8* buf, u32 size)
{
    int rc = 0;
    while (size--) {
        rc = di->bus.write(di, reg++, *buf++);
        if (rc < 0) {
            dev_err(di->dev, "MT5725 write error: %d\n", rc);
            return rc;
        }
    }
    return rc;
}


/* platform i2c operation */

// /* ic irq read/ write clear */
// static int MT5725_irq_status(int read)
// {
// 	vuc irq_flag;
// 	vuc irq_flag_temp;
// 	int rc = 0;
// 	int count = 3;//i2c err

// 	pr_err("enter %s: \n", __func__);


//     irq_flag_temp.value = 0;
	
// 	if (MT5725_power_status() == 0) {
// 		return -1;//charger off
// 	}

// 	//mutex_lock(&mte->slock);
// 	do {
// 		if (read) {//read
// 			rc = MT5725_read_buffer(mte, reg_access[INT_FLAG].addr, irq_flag.ptr, 2);			
// 			if (rc){
// 				pr_err("%s: MT5725_read_buffer rc =%d \n", __func__,rc);
// 				continue;
// 			}
// 			else {
// 				pr_err("%s: read irq status successful, 0x%02x%02x, 0x%04x\n", __func__, irq_flag.ptr[1], irq_flag.ptr[0], irq_flag.value);
// 				return irq_flag.value;
// 			}
// 		} else {//write clear
// 		    rc = MT5725_read_buffer(mte, reg_access[INT_FLAG].addr, irq_flag.ptr, 2);
// 			if (rc){ 
// 				pr_err("%s: MT5725_read_buffer rc =%d  step1\n", __func__,rc);
// 				continue;
// 			}
// 			irq_flag_temp.value = irq_flag.value;
// 			pr_err("%s: write irq & INTCTLR, 0x%04x, 0x%04x\n", __func__, irq_flag.value, irq_flag.value);

// 			rc = MT5725_write_buffer(mte, reg_access[INTCTLR].addr, irq_flag.ptr, 2);
// 			if (rc){
// 				pr_err("%s: MT5725_read_buffer rc =%d  step2\n", __func__,rc);
// 				continue;
// 			}
// 			irq_flag.value= CLRINT;
// 			pr_err("%s: cmd, 0x%04x, 0x%04x\n", __func__, irq_flag.value, irq_flag.value);
// 			rc = MT5725_write_buffer(mte, reg_access[CMD].addr, irq_flag.ptr, 2);
// 			if (rc){
// 				pr_err("%s: MT5725_read_buffer rc =%d  step3\n", __func__,rc);
// 				continue;
// 			}
// 			return irq_flag_temp.value;
// 		    /*
// 			rc = MT5725_read_buffer(mte, reg_access[INT_FLAG].addr, irq_flag.ptr, 2);
// 			if (rc){ 
// 				pr_err("%s: MT5725_read_buffer rc =%d  step1\n", __func__,rc);
// 				continue;
// 			}

// 			pr_err("%s: read irq, 0x%02x%02x, 0x%04x\n", __func__, irq_flag.ptr[1], irq_flag.ptr[0], irq_flag.value);

//             //irq_flag_temp.ptr[0] = irq_flag.ptr[0];
// 			//irq_flag.ptr[0] = irq_flag_temp.ptr[0] & 0xFE;
// 			irq_flag.value &= ~SS_AFC;
			
// 			pr_err("%s: INTCTLR, 0x%02x%02x, 0x%04x\n", __func__, irq_flag.ptr[1], irq_flag.ptr[0], irq_flag.value);
// 			rc = MT5725_write_buffer(mte, reg_access[INTCTLR].addr, irq_flag.ptr, 2);
// 			if (rc){
// 				pr_err("%s: MT5725_read_buffer rc =%d  step2\n", __func__,rc);
// 				continue;
// 			}

// 			irq_flag.value= CLRINT;
// 			pr_err("%s: cmd, 0x%02x%02x, 0x%04x\n", __func__, irq_flag.ptr[1], irq_flag.ptr[0], irq_flag.value);
// 			rc = MT5725_write_buffer(mte, reg_access[CMD].addr, irq_flag.ptr, 2);
// 			if (rc){
// 				pr_err("%s: MT5725_read_buffer rc =%d  step3\n", __func__,rc);
// 				continue;
// 			}

// 			rc = MT5725_read_buffer(mte, reg_access[INT_FLAG].addr, irq_flag.ptr, 2);
// 			if (rc){ 
// 				pr_err("%s: MT5725_read_buffer rc =%d  step4\n", __func__,rc);
// 				continue;
// 			}
// 			else {
// 				pr_err("%s: read irq after, 0x%02x%02x, 0x%04x\n", __func__, irq_flag.ptr[1], irq_flag.ptr[0], irq_flag.value);
// 				if (irq_flag.value == 0) {
// 					pr_err("%s: clear irq successful\n", __func__);
// 					return irq_flag.value;
// 				}
// 				else {				
// 					pr_err("%s: clear irq fail 0x%04x\n", __func__, irq_flag.value);
// 					msleep(5);
// 					rc = MT5725_read_buffer(mte, reg_access[INT_FLAG].addr, irq_flag.ptr, 2);
// 					if (rc){
// 						pr_err("%s: MT5725_read_buffer rc =%d  step5\n", __func__,rc);
// 						continue;
// 					}
// 					pr_err("%s: clear irq second time 0x%02x%02x 0x%04x\n", __func__, irq_flag.ptr[1], irq_flag.ptr[0], irq_flag.value);
// 					return (irq_flag.ptr[1] << 8 | irq_flag.ptr[0]);
// 				}
// 			}*/
// 		}
// 	} while (count --);
// 	//mutex_unlock(&mte->slock);

// 	pr_err("%s: rc = %d\n", __func__, rc);
// 	return rc;
// }
// /* ic irq read/ write clear */

#if 0
/* LDO voltage */
static int MT5725_ldo_status(bool read, int *vol)
{
    int ret = 0;
    vuc val;
    int vout_value  = 0;
    u8  tmp = 0;

    if (read) {
    	ret = MT5725_read_buffer(mte, reg_access[VOUT].addr, val.ptr, 2);
    	if (ret)
    		return -1;
    	else {
    		vout_value = val.ptr[0] << 8 |  val.ptr[1];
    		pr_err("chip Vout : %d\n",vout_value);
    		*vol = vout_value;
    		return ret;
    	}
    } else {
    	val.value = *vol;
    	if((vout_value < 0) || (vout_value > 20000)) {
    		pr_err("REG_VOUTSET Parameter error\n");
    		return -1;
    	}

    	tmp = val.ptr[0];
    	val.ptr[0] = val.ptr[1];
    	val.ptr[1] = tmp;
    	pr_err("set chip Vout: %02x%02x\n", val.ptr[0], val.ptr[1]);

    	ret = MT5725_write_buffer(mte, REG_VOUTSET, val.ptr,2);
    	if (ret) {
    		pr_err("set chip Vout: %d fail\n",vout_value);
    		return -1;
    	} else {
    		ret = MT5725_read_buffer(mte, reg_access[VOUT].addr, val.ptr, 2);
    		if (ret) {
    			pr_err("read chip Vout: %d fail\n",vout_value);
    			return -1;
    		} else {
    			vout_value = val.ptr[0] << 8 |  val.ptr[1];
    			pr_err("read chip Vout: %d\n",vout_value);
    			return vout_value;
    		}
    	}
    }
}
/* LDO voltage */
#endif

//-----
/*
    Update Tx code from SRAM
    Step 1: check power good pin must clear 0
    Step 2: Pour VBUS into Vout
    Step 3: hold MT5725 cpu
    Step 4: Tx data writing to SRAM
    Setp 5: Reset MT5725
*/
void MT5725_tx_sram_updata(void)
{
    vuc val;
    int len = sizeof(mt5725_tx_test);
    u16 sram_tx = REG_Tx_START;
	int i = 0;
	
    if(MT5725_power_status()){
        pr_err("%s:MT5725 Rx in working condition , must enter power-off state ,move aside \n",__func__);
        return;
    }else{
        //Pour VBUS into Vout
        //...
        //
        val.value = MT5725_WDG_DISABLE;
        MT5725_write_buffer_without_pstate(mte, REG_PMU_WDGEN, val.ptr, 2);
        MT5725_write_buffer_without_pstate(mte, REG_PMU_WDGEN, val.ptr, 2);
        val.value = MT5725_WDT_INTFALG;
        MT5725_write_buffer_without_pstate(mte, REG_PMU_FLAG,val.ptr,2);
        val.value = MT5725_KEY;
        MT5725_write_buffer_without_pstate(mte, REG_SYS_KEY,val.ptr,2);
        MT5725_read_buffer_without_pstate(mte,REG_M0_CTRL,val.ptr,2);
        val.value |=MT5725_M0_HOLD;
        MT5725_write_buffer_without_pstate(mte, REG_M0_CTRL,val.ptr,2);
        val.ptr[0] = 0x0f;
        val.ptr[1] = 0x42;
        MT5725_write_buffer_without_pstate(mte, REG_SRAM_REMAP,val.ptr,2);
        val.value = 0x0E;
        MT5725_write_buffer_without_pstate(mte, REG_CODE_REMAP,val.ptr,2);
        //...wait 0.2 sencond
        //get data leng
        for (i=0; i<len;) {
            if ((i+SRAM_PAGE_SIZE) > len)
                MT5725_write_buffer_without_pstate(mte,sram_tx+i,(u8*)&mt5725_tx_test[i],len%SRAM_PAGE_SIZE);
            else
                MT5725_write_buffer_without_pstate(mte,sram_tx+i,(u8*)&mt5725_tx_test[i],SRAM_PAGE_SIZE);
            i+=SRAM_PAGE_SIZE;
        }
        val.value = MT5725_M0_RESET;
        MT5725_write_buffer_without_pstate(mte, REG_M0_CTRL,val.ptr,2); //sys run

    }

}
EXPORT_SYMBOL(MT5725_tx_sram_updata);

/*
    Send proprietary protocol to Tx
    Step 1: data in REG_PPP
    Step 2: REG_CMD send_ppp
*/
void MT5725_send_ppp(void)
{
    u8 temp[3] = {0x18,0xA3,0x00}; //change  temp not must be three 
    vuc val;
    int len;
    //Calculate the length of a packet of data according to its header
    if (temp[0] < 0x20){      //
        len = 1+1;
    }else if (temp[0] < 0x80){
        len = (2 + ((temp[0] - 0x20) >> 4))+1;
    }else if(temp[0] < 0xe0){
        len = (8 + ((temp[0] - 0x80) >> 3))+1;
    }else{
        len = (20 + ((temp[0] - 0xe0) >> 2))+1;
    }
    MT5725_write_buffer(mte, REG_PPP, temp, len);
    val.value = SENDPPP;
    MT5725_write_buffer(mte, REG_CMD, val.ptr, 2);
}

EXPORT_SYMBOL(MT5725_send_ppp);



//prize modify by sunshuai, wireless charge MT5725  soft get ldo status , 20190302-start
int get_lod_status(void){
    if((mte->ldo_status ==1)&&(MT5725_power_status() == true)){
		return 1;
    }
	else{
		return 0;
	}
}
EXPORT_SYMBOL(get_lod_status);
//prize modify by sunshuai, wireless charge MT5725  soft get ldo status , 20190302-end

//prize modify by sunshuai, wireless charge MT5725 get  good status  , 20190302-start
int get_MT5715on_status(void){
    if(MT5725_power_status() == true)
		return 1;
	else
		return 0;

}
EXPORT_SYMBOL(get_MT5715on_status);
//prize modify by sunshuai, wireless charge MT5725 get  good status  , 20190302-end



//prize modify by sunshuai, wireless charge MT5725  soft Compatible reporting up 9 volts interrupt handling, 20190302-start
// static int MT5725_handle(u16 flag)
// {
// 	//size_t eol;
// 	vuc irq_flag;
// 	//int val;
// 	int ret = 0;

// 	//eol = find_next_bit((unsigned long *)&flag, sizeof(flag), 0);
// 	pr_err("%s:flag = 0x%x, \n", __func__,flag);
//     irq_flag.value = flag;

// 	if(irq_flag.value & LDO_ON){//bit 6
//         mte->ldo_status = 1;
// 		mte->is_samsung_charge =0;
// 		pr_err("%s: INT_LDO_ON mte->ldo_status =%d mte->is_samsung_charge =%d\n" , __func__,mte->ldo_status,mte->is_samsung_charge);
// 		pr_err("%s: MT5725_handle before INT_LDO_ON  fast_sv \n" , __func__);
// 		fast_sv(9000);
// 		pr_err("%s: MT5725_handle after INT_LDO_ON  fast_sv \n" , __func__);
// 	}
	
//     if(irq_flag.value & LDO_OFF){// bit 2
// 		mte->ldo_status = 0;
// 		mte->is_samsung_charge =0;
// 		pr_err("%s: INT_LDO_OFF mte->ldo_status =%d mte->is_samsung_charge=%d\n" , __func__,mte->ldo_status,mte->is_samsung_charge);
// 	}


// 	if(irq_flag.value & SS_AFC){//bit 0
// 		pr_err("%s: before step up 9V \n" , __func__);
// 		fast_sv(9000);
// 		pr_err("%s: after step up 9V \n" , __func__);
// 	}

// 	if(irq_flag.value & INTFLAG_FS){ // FSK 
// 		//read REG_BC data
// 		pr_err("%s: FSK success \n" , __func__);
// 	}
// 	return ret;
// }
//prize modify by sunshuai, wireless charge MT5725  soft Compatible reporting up 9 volts interrupt handling, 20190302-end


/* ldo status */
int MT5725_ldo_on(bool on)
{
	vuc value;
	int rc = 0;
	int count = 2;//i2c err
	pr_err("%s: mte->ldo_status = %d on = %d\n", __func__, mte->ldo_status, on);

	if (mte->ldo_status ^ on) {
		do {
			value.value = LBIT(0)|LBIT(1)|LBIT(2)|LBIT(4);
			pr_err("%s: cmd, enable LDO \n", __func__);
			rc = MT5725_write_buffer(mte,REG_LDO, value.ptr, 2);
			if (rc) 
				continue;
		} while (count --);
	}
	return rc;
}
EXPORT_SYMBOL(MT5725_ldo_on);
/* ldo status */
//prize add by lpp 20190821 start Get whether the device is close when the mobile phone is in a backcharging state
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
static int TXupon_coil=0;
#endif 
//prize add by lpp 20190821 end Get whether the device is close when the mobile phone is in a backcharging state
void MT5725_irq_handle(void)
{
// 	int flag = 0;
// 	int flag_c = 0;
// 	int count = 5;//new irq count, add error handling later
// 	int ret = 0;

// 	pr_err("enter %s:\n", __func__);

// 	while (count) {
// 		flag = MT5725_irq_status(true);
// 		if (flag > 0)
// 		        pr_err("%s: read irq successful, flag = %x\n", __func__, flag);
// 		else if (flag == 0) {
// 		        pr_err("%s: no irq\n", __func__);
// 		        return;
// 		} else {
// 		        pr_err("%s: charger off\n", __func__);
// 		        return;
// 		}

// 		ret = MT5725_handle((u16)flag);

// #if 1
// 		flag_c = MT5725_irq_status(false);
// 		if (flag_c == 0) {
// 		        pr_err("%s: clear irq successful, flag = %x\n", __func__, flag_c);
// 		        return;
// 		}
// 		else if(flag_c > 0) {//new irq
// 		        pr_err("%s: new irq occur, flag_c = %x\n", __func__, flag_c);
// 		        if (flag ^ flag_c) {
// 		                MT5725_handle(flag_c);
// 		                count --;
// 		                pr_err("%s: new irq occur, count = %d\n", __func__, count);
// 		        }
// 		        else {
// 		                pr_err("%s: flag_c = %x, count = %d. same irq occur, clear fail!!!\n", __func__, flag_c, count);
// 		                break;
// 		        }

// 		} else {
// 		        pr_err("%s: ic error, flag_c = %x\n", __func__, flag_c);
// 		        return;
// 		}
// #endif
// 	}
// 	return;

	vuc val;
    vuc temp, fclr, scmd;
    scmd.value = 0;
    pr_err("----------------MT5725_delayed_work-----------------------\n");
    MT5725_read_buffer(mte,REG_FW_VER,val.ptr,2);
	if(val.ptr[1] & LBIT(0)){
		MT5725_read_buffer(mte, REG_INT_FLAG, val.ptr, 2);
		fclr.value = val.value;
		printk("%s ,MT5725-Rx  val:0x%04x\n", __func__, val.value);
		if (val.value & INTFLAG_PO) {
			pr_err("MT5725 %s , INTFALG_PowerON\n", __func__);
		}
		if (val.value & INTFLAG_LO) {
			pr_err("MT5725 %s , LDO ON\n", __func__);
		}
		if (val.value & INTFLAG_RE) {
			pr_err("MT5725 %s , MT5725 is Ready\n", __func__);
		}
		if (val.value & INTFLAG_LF) {
			pr_err("MT5725 %s , MT5725 LDO  off\n", __func__);
		}
		if (val.value & INTFLAG_SA) {
			pr_err("MT5725 %s ,version 0.1 Tx support samsung_afc\n", __func__);
			temp.value = 9000;
			MT5725_write_buffer(mte, REG_VFC, temp.ptr, 2);
			scmd.value |= FASTCHARGE;
		}
		if (val.value & INTFLAG_FS) {
			pr_err("MT5725 %s , FSK successfully  off\n", __func__);
			//read REG_BC
		}
	}else if(val.ptr[1] & LBIT(2)){
		MT5725_read_buffer(mte, REG_INT_FLAG, val.ptr, 2);
		fclr.value = val.value;
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
		TXupon_coil=0;
#endif
		printk("%s ,MT5725-Tx  val:0x%04x\n", __func__, val.value);
		if(val.value & FINDRX){
//prize add by lpp 20190821 start Get whether the device is close when the mobile phone is in a backcharging state
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
			TXupon_coil=1;
			mt_vbus_reverse_on_limited_current();
			
#endif
//prize add by lpp 20190821 end Get whether the device is close when the mobile phone is in a backcharging state
			printk("%s:MT5725-Tx, Found rx which upon coil\n", __func__);
		}
		if(val.value & OCPFRX){
			printk("%s:MT5725-Tx, Load pulled too high\n", __func__);
		}
		if(val.value & FODFRX){
			printk("%s:MT5725-Tx, Foreign body detection\n", __func__);
		}
	}
    scmd.value |= CLRINT;

    //---clrintflag
    MT5725_write_buffer(mte, REG_INTCLR, fclr.ptr, 2);
    printk("%s,version 0.1 write reg_clr : 0x%04x,\n", __func__, fclr.value);

    MT5725_write_buffer(mte, REG_CMD, scmd.ptr, 2);
    printk("%s,version 0.1 write reg_cmd : 0x%04x,\n", __func__, scmd.value);

    MT5725_read_buffer(mte, REG_INT_FLAG, val.ptr, 2);
    //if (val.value != 0)
     //   MT5725_irq_handle();
}
EXPORT_SYMBOL(MT5725_irq_handle);

static void MT5725_eint_work(struct work_struct *work)
{
	MT5725_irq_handle();
}

static irqreturn_t MT5725_irq(int irq, void *data)
{
    struct MT5725_dev *mt5725 = data;
	pr_err("enter %s: \n", __func__);
	schedule_delayed_work(&mt5725->eint_work, 0);
    return IRQ_HANDLED;
}

static int MT5725_parse_dt(struct i2c_client *client, struct MT5725_dev *mt5725)
{
	mt5725->trxset_gpio = of_get_named_gpio(client->dev.of_node, "trxset-gpio", 0);
	if (mt5725->trxset_gpio < 0) {
		pr_err("%s: no trxset_gpio gpio provided\n", __func__);
		return -1;
	} else {
		pr_info("%s: trxset_gpio gpio provided ok. mt5725->trxset_gpio = %d\n", __func__, mt5725->trxset_gpio);
	}

	mt5725->irq_gpio = of_get_named_gpio(client->dev.of_node, "irq-gpio", 0);
	if (mt5725->irq_gpio < 0) {
		pr_err("%s: no irq gpio provided.\n", __func__);
		return -1;
	} else {
		pr_info("%s: irq gpio provided ok. mt5725->irq_gpio = %d\n", __func__, mt5725->irq_gpio);
	}
	
	mt5725->otgen_gpio = of_get_named_gpio(client->dev.of_node, "otgen-gpio", 0);
	if (mt5725->otgen_gpio < 0) {
		pr_err("%s: no otgen gpio provided.\n", __func__);
		return -1;
	} else {
		pr_info("%s: otgen gpio provided ok. mt5725->otgen_gpio = %d\n", __func__, mt5725->otgen_gpio);
	}
	
	mt5725->pwrst_gpio = of_get_named_gpio(client->dev.of_node, "pwrst-gpio", 0);
	if (mt5725->pwrst_gpio < 0) {
		pr_err("%s: no pwrst gpio provided.\n", __func__);
		return -1;
	} else {
		pr_info("%s: irq pwrst provided ok. mt5725->pwrst_gpio = %d\n", __func__, mt5725->pwrst_gpio);
	}
	
	mt5725->chipen_gpio = of_get_named_gpio(client->dev.of_node, "chipen-gpio", 0);
	if (mt5725->chipen_gpio < 0) {
		pr_err("%s: no chipen gpio provided.\n", __func__);
		return -1;
	} else {
		pr_info("%s: irq chipen provided ok. mt5725->chipen_gpio = %d\n", __func__, mt5725->chipen_gpio);
	}

	return 0;
}

/* attr for debug */
static ssize_t get_reg(struct device* cd,struct device_attribute *attr, char* buf)
{
    u8 val[2];
    ssize_t len = 0;
    int i = 0;
	
    for(i = 0; i< INDEX_MAX ;i++) {
        if(reg_access[i].flag & REG_RD_ACCESS) {
            MT5725_read_buffer(mte,reg_access[i].addr, val, 2);
            len += snprintf(buf+len, PAGE_SIZE-len, "reg:%s 0x%02x=0x%02x%02x\n", reg_access[i].name, reg_access[i].addr,val[0],val[1]);
        }
    }

    return len;
}

#if 0
void get_int_flag(void){
	vuc irq_flag;
	int rc = 0;
	u8  ptr_temp;
	bool flag= 0;
	int val = 0;
	
	 rc = MT5725_read_buffer(mte, reg_access[INT_FLAG].addr, irq_flag.ptr, 2);
	 if (rc){
			pr_err("%s: MT5725_read_buffer rc =%d \n", __func__,rc);
			val = 0;
			return;
	 }
	 else {
			pr_err("%s: read irq status successful, 0x%02x%02x, 0x%04x\n", __func__, irq_flag.ptr[0], irq_flag.ptr[1], irq_flag.value);
			//val = irq_flag.ptr[0] << 8 | irq_flag.ptr[1];
			ptr_temp = irq_flag.ptr[1];
			
            flag = (ptr_temp & 0x04) == 0x00 ? true:false;
			
			if (irq_flag.ptr[0]== 0x01 && flag== true) {
					rc = MT5725_ldo_status(true, &val);
					if (!rc)
						pr_err("%s: ldo = %d\n", __func__, val);

					val = 9000;
					rc = MT5725_ldo_status(false, &val);
					pr_err("%s: set ldo:%d, act ldo:%d\n", __func__, val, rc);
			}
			return;
	 }
			
}
EXPORT_SYMBOL(get_int_flag);
#endif

static ssize_t set_reg(struct device* cd, struct device_attribute *attr, const char* buf, size_t len)
{
    unsigned int databuf[2];
    vuc val;	
    u8  tmp[2];
    u16 regdata;
    int i = 0;
	int ret = 0;

	ret = sscanf(buf,"%x %x",&databuf[0], &databuf[1]);

    if(2 == ret) {
		for(i = 0; i< INDEX_MAX ;i++) {
			if(databuf[0] == reg_access[i].addr) {
				if (reg_access[i].flag & REG_WR_ACCESS) {
					val.ptr[0] = (databuf[1] & 0xff00) >> 8;
					val.ptr[1] = databuf[1] & 0x00ff;   //big endian
					if (reg_access[i].flag & REG_BIT_ACCESS) {
						MT5725_read_buffer(mte, databuf[0], tmp,2);	
						regdata = tmp[0] << 8 | tmp[1];
						val.value |= regdata;
						printk("get reg: 0x%04x  set reg: 0x%04x \n", regdata, val.value);
						MT5725_write_buffer(mte, databuf[0], val.ptr, 2);
					}
					else {
						printk("Set reg : [0x%04x]  0x%x 0x%x \n",databuf[1], val.ptr[0], val.ptr[1]);
						MT5725_write_buffer(mte, databuf[0], val.ptr, 2);
					}
				}
				break;
			}
		}
    }
    return len;
}


static ssize_t chip_version_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    u8 fwver[2];
    ssize_t len = 0;// must to set 0
    MT5725_read_buffer(mte, REG_FW_VER, fwver, 2);
    len += snprintf(buf+len, PAGE_SIZE-len, "chip_version : %02x,%02x\n", fwver[0],fwver[1]);
    return len;
}

/* voltage limit attrs */
static ssize_t chip_vout_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    unsigned char   fwver[2];
    unsigned short  vout_value;
    ssize_t len = 0;

    MT5725_read_buffer(mte, REG_VOUT, fwver,2);
    vout_value = fwver[0] << 8 |  fwver[1];
    pr_debug("chip Vout : %d\n", vout_value);
    len += snprintf(buf+len, PAGE_SIZE-len, "chip Vout : %d mV\n", vout_value);
    return len;
}

static ssize_t chip_vout_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    vuc val;
    int error;
    unsigned int temp;
    u8 vptemp;
    error = kstrtouint(buf, 10, &temp);
    if (error)
        return error;
    if( (temp < 0) ||( temp > 20000)){
        pr_debug(" Parameter error\n");
        return count;
    }
    val.value = temp;
    vptemp = val.ptr[0];
    val.ptr[0] = val.ptr[1];
    val.ptr[1] = vptemp;
    MT5725_write_buffer(mte, REG_VOUTSET, val.ptr,2);
    pr_err("Set Vout : %d \n", val.value);

    return count;
}


int  fast_sv_no_samsung(int temp){
	vuc val;
	int rc;
	u8  fcflag;
	
    if( (temp < 0) ||( temp > 20000)){
        pr_debug(" Parameter error\n");
        return 0;
    }
	
#if 0
	//Raise the voltage to 9V
	val.value = temp;  // 9000  0x2328
    fcflag = val.ptr[0]; //swap
    val.ptr[0] = val.ptr[1];
    val.ptr[1] = fcflag;  //0x2823
    MT5725_write_buffer(mte, REG_VOUTSET, val.ptr, 2);
#endif

			//Raise the voltage to 9V
	val.value = temp;  // 9000  0x2328
    fcflag = val.ptr[0]; //swap
    val.ptr[0] = val.ptr[1];
    val.ptr[1] = fcflag;  //0x2823
    MT5725_write_buffer(mte, REG_VFC, val.ptr, 2);
    pr_err("FC send data step up 9V : 0x%02x,0x%02x \n", val.ptr[0],val.ptr[1]);
	val.ptr[0] = 0x00;
	val.ptr[1] = 0x10;
    MT5725_write_buffer(mte, REG_CMD, val.ptr, 2);
	
    pr_err("fast_sv_no_samsung  step up REG_VFC 9V : 0x%02x,0x%02x \n", val.ptr[0],val.ptr[1]);

	
    MT5725_read_buffer(mte,REG_VOUT,val.ptr,2);
    rc = val.ptr[0] << 8 |  val.ptr[1];
	dev_info(mte->dev,"%s:vol read  vol=%d\n", __func__,rc);
	
	if(rc > 8000) return 1;
	else          return 0;

	
    return 1;    
	
}
EXPORT_SYMBOL(fast_sv_no_samsung);


static ldo_disable(void){
	int voutval = 0;
	int ret = 0;
	vuc val;
	ret = MT5725_read_buffer(mte,REG_VOUT,val.ptr,2);
	if(ret){
		pr_err("%s: MT5725_read_buffer REG_VOUT ret  =%d  \n", __func__,ret);
		return (-1);
	}
    //voutval = val.ptr[1] << 8 |  val.ptr[0];
	voutval = val.value;
	if(voutval > 2000){
		 val.value = 0;
		 ret = MT5725_write_buffer(mte, REG_LDO, val.ptr, 2); //0V disable ldo
		 if (ret){
			pr_err("%s: MT5725_write_buffer ret =%d  step3\n", __func__,ret);
			return (-1);
		 }
		 val.value = 3500;  //
		 ret = MT5725_write_buffer(mte, REG_VOUTSET,val.ptr, 2);// 
		 if (ret){
			pr_err("%s: MT5725_write_buffer ret =%d  step4\n", __func__,ret);
			return (-1);
		 }
		
		 pr_err("%s: sucess  ret =%d \n", __func__,ret);
		 return 1;
	}else{
	    pr_err("%s: voutval < 2000  ret =%d \n", __func__,ret);
		return 0; 
	}
}

//prize added by sunshuai, wireless charge MT5725  soft Raise the voltage to 9V, 20190307-start

int  get_mt5725_9V_charge_status(void){
	vuc val;
	int rc = 0;
	int lod_on_status = 0;
	
	lod_on_status = get_lod_status();
    MT5725_read_buffer(mte,REG_VOUT,val.ptr,2);
    rc = val.ptr[0] << 8 |  val.ptr[1];
	pr_err("%s: vol read  vol=%d\n", __func__,rc);	
	if((rc > 8000) && (lod_on_status == 1))
		return 1;
	else
		return 0;	
}

EXPORT_SYMBOL(get_mt5725_9V_charge_status);


//prize added by sunshuai, wireless charge MT5725  soft Raise the voltage to 9V, 20190307-end

int get_is_samsung_charge (void){
   pr_err("%s: is_samsung_charge %d\n", __func__, mte->is_samsung_charge);
   return  mte->is_samsung_charge;
}
EXPORT_SYMBOL(get_is_samsung_charge);

int set_is_samsung_charge(int temp){
    mte->is_samsung_charge =temp;
	return 1;
}
EXPORT_SYMBOL(set_is_samsung_charge);


//prize added by sunshuai, wireless charge MT5725  soft Raise the voltage to 9V, 20190223-start
int  fast_sv(int temp){
	vuc val;
	vuc irq_flag;
	int rc;
	MT5725_read_buffer(mte,REG_INT_FLAG,val.ptr,2);
	pr_err("%s: read irq status successful, 0x%02x%02x, 0x%04x is_samsung_charge=%d\n", __func__, val.ptr[1], val.ptr[0], val.value,mte->is_samsung_charge);

	
    if(val.ptr[1] & 0x01) {
		//Clear interrupt
		mte->is_samsung_charge =1;
		pr_err("%s: set is_samsung_charge =%d\n", __func__,mte->is_samsung_charge);
		//irq_flag.ptr[0] = 0x00;
		//irq_flag.ptr[1] = 0x01;
		irq_flag.value = SS_AFC;
		rc = MT5725_write_buffer(mte, reg_access[INTCTLR].addr, irq_flag.ptr, 2); 
		if (rc){
			dev_err(mte->dev,"%s:clean irq fail rc =%d  step1, pwr_sate:%d\n", __func__,rc,gpio_get_value(mte->pwrst_gpio));
			return (-1);
		}
		irq_flag.value= CLRINT;
		pr_err("%s: cmd, 0x%02x%02x, 0x%04x\n", __func__, irq_flag.ptr[1], irq_flag.ptr[0], irq_flag.value);
		rc = MT5725_write_buffer(mte, reg_access[CMD].addr, irq_flag.ptr, 2);
		if (rc){
			dev_err(mte->dev,"%s:clean irq fail rc =%d  step2, pwr_sate:%d\n", __func__,rc,gpio_get_value(mte->pwrst_gpio));
			return (-1);
		}
		//Raise the voltage to 9V
		val.value = temp;  // 9000  0x2328
        //fcflag = val.ptr[0]; //swap
        //val.ptr[0] = val.ptr[1];
        //val.ptr[1] = fcflag;  //0x2823
        MT5725_write_buffer(mte, REG_VFC, val.ptr, 2);
        pr_err("FC send data step up 9V : 0x%02x,0x%02x \n", val.ptr[0],val.ptr[1]);
		val.value = FASTCHARGE;
        MT5725_write_buffer(mte, REG_CMD, val.ptr, 2);
    }
	else{
		if(mte->is_samsung_charge ==1){
			pr_err("%s:  is_samsung_charge =%d\n", __func__,mte->is_samsung_charge);
			MT5725_read_buffer(mte,REG_VOUT,val.ptr,2);
			rc = val.value;
			pr_err("%s: vol read  vol=%d\n", __func__,rc);
			
			if(rc > 8000){
               pr_err("%s: vol read  vol=%d\n", __func__,rc);
			   return 1;
			}
			else{
				val.value = temp;  // 9000  0x2328
				//fcflag = val.ptr[0]; //swap
				//val.ptr[0] = val.ptr[1];
				//val.ptr[1] = fcflag;  //0x2823
				MT5725_write_buffer(mte, REG_VFC, val.ptr, 2);
				pr_err("FC send data step up 9V : 0x%02x,0x%02x \n", val.ptr[1],val.ptr[0]);
				val.value = FASTCHARGE;
				MT5725_write_buffer(mte, REG_CMD, val.ptr, 2);
			}
		}
	}

    MT5725_read_buffer(mte,REG_VOUT,val.ptr,2);
    rc = val.value;
	dev_info(mte->dev,"%s:vol read  vol=%d\n", __func__,rc);
	
	if(rc > 8000) return 1;
	else          return 0;
    
	
}

EXPORT_SYMBOL(fast_sv);
//prize added by sunshuai, wireless charge MT5725  soft Raise the voltage to 9V, 20190223-end

void set_typc_otg_gpio(int en){
	
	if(en==1){
	   gpio_direction_output(mte->otgen_gpio,1);
	}else{
	   gpio_direction_output(mte->otgen_gpio,0);
	}
}
EXPORT_SYMBOL(set_typc_otg_gpio);

int mt5725_sw_sel_usb(int is_usb){
	int ret = 0;
	//mutex_lock(&switch_lock);
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
	printk("LPP--mte->otgen_gpio=%d ,is_usb=%d\n",mte->otgen_gpio,is_usb);
#endif
	if (mte->otgen_gpio > 0){
		if (is_usb == 0){	//set to wrx
			printk("%s sel rx\n",__func__);
			//gpio_direction_output(mte->chipen_gpio,1);
			gpio_direction_output(mte->otgen_gpio,0);
		}else{	//Power path set to USB
			printk("%s sel usb\n",__func__);
			//gpio_direction_output(mte->chipen_gpio,0);
			ldo_disable();
			//msleep(2);
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
			gpio_direction_output(mte->trxset_gpio,0);
			atomic_set(&mte->is_tx_mode,0);//prize add by lpp 20190806 
			mt_vbus_off();//prize add by lpp 20190820 close otg host
#endif
			gpio_direction_output(mte->otgen_gpio,1);
		}
	}else{
		//sw_sel_pending = is_usb;
		ret = -EINVAL;
	}
	//mutex_unlock(&switch_lock);
	return ret;
}
EXPORT_SYMBOL(mt5725_sw_sel_usb);

static ssize_t fast_charging_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
   // vuc val;
    int error;
    unsigned int temp;
    //u8  fcflag;
	pr_err("enter fast_charging_store \n");
	
    error = kstrtouint(buf, 10, &temp);//"9000"
    if (error)
        return error;
    if( (temp < 0) ||( temp > 20000)) {
        pr_err(" Parameter error\n");
        return count;
    }
	fast_sv(temp);
   // MT5725_read(mte, 9, &fcflag);
 /*   MT5725_read_buffer(mte,REG_INT_FLAG,val.ptr,2);
    if(val.ptr[0] & 0x01) {
		val.value = temp;  // 9000  0x2328
        fcflag = val.ptr[0]; //swap
        val.ptr[0] = val.ptr[1];
        val.ptr[1] = fcflag;  //0x2823
        MT5725_write_buffer(mte, REG_VFC, val.ptr, 2);
        pr_err("FC send data : 0x%02x,0x%02x \n", val.ptr[0],val.ptr[1]);
		val.ptr[0] = 0x00;
		val.ptr[1] = 0x10;
        MT5725_write_buffer(mte, REG_CMD, val.ptr, 2);
       // pr_debug("FC : %d \n", val.value);
       // MT5725_read_buffer(mte,REG_VFC,val.ptr,2);
       // pr_debug("FC read data : 0x%02x,0x%02x \n", val.ptr[0],val.ptr[1]);
    } else {
        pr_err("Fast charging is not supported \n");
    }*/
    return count;
}

static ssize_t fast_charging_show(struct device* dev, struct device_attribute* attr, char* buf)
{
    unsigned char   fwver[2];
    unsigned short  fc_value;
    ssize_t len = 0;

    MT5725_read_buffer(mte, REG_VFC, fwver,2);
    fc_value = fwver[0] << 8 |  fwver[1];
    pr_debug("FC read data : %d\n", fc_value);
    len += snprintf(buf+len, PAGE_SIZE-len, "FC read data : %d mV\n", fc_value);
    return len;
}

static DEVICE_ATTR(chip_version, S_IRUGO | S_IWUSR, chip_version_show, NULL);
static DEVICE_ATTR(chip_vout, S_IRUGO | S_IWUSR, chip_vout_show, chip_vout_store);
static DEVICE_ATTR(fast_charging,S_IRUGO | S_IWUSR, fast_charging_show, fast_charging_store);
static DEVICE_ATTR(reg,S_IRUGO | S_IWUSR, get_reg, set_reg);

static struct attribute* mt5725_sysfs_attrs[] = {
    &dev_attr_chip_version.attr,
    &dev_attr_chip_vout.attr,
    &dev_attr_fast_charging.attr,
    &dev_attr_reg.attr,//for debug
    NULL,
};
/* attr for debug */

static const struct attribute_group mt5725_sysfs_group = {
    .name = "mt5725group",
    .attrs = mt5725_sysfs_attrs,
};

int wireless_is_tx_mode(void){
	return atomic_read(&mte->is_tx_mode);
}
EXPORT_SYMBOL(wireless_is_tx_mode);

//prize add by lpp 20190821 start Get whether the device is close when the mobile phone is in a backcharging state
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
static ssize_t gettx_flag_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	if(TXupon_coil==1){
		//TXupon_coil=0;
       return sprintf(buf, "%d", atomic_read(&mte->is_tx_mode));
	}else{
		return sprintf(buf, "%d", 0);
	}
}
static DEVICE_ATTR(gettxflag, 0644, gettx_flag_show, NULL);
//prize add by lpp 20190821 end Get whether the device is close when the mobile phone is in a backcharging state

static ssize_t enable_tx_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	/*struct i2c_client *client = to_i2c_client(dev);
	struct MT5725_dev *chip = i2c_get_clientdata(client);
	printk(KERN_INFO"mt5725 enable_tx\n");
	mutex_lock(&chip->ops_mutex);
	atomic_set(&mte->is_tx_mode,1);
	gpio_direction_output(chip->trxset_gpio,1);
	gpio_direction_output(chip->otgen_gpio,0); //select wirelessrx
	//ldo_disable();
	//charger_dev_enable_otg(g_info->primary_charger, true);
	//charger_dev_set_boost_current_limit(g_info->primary_charger, 1500000);
	//charger_dev_kick_wdt(g_info->primary_charger);
	//enable_boost_polling(true);
	mt_vbus_on();
	mutex_unlock(&chip->ops_mutex);*/
    return sprintf(buf, "%d", atomic_read(&mte->is_tx_mode));//atomic_read(&mte->is_tx_mode);// prize add by lpp 20190727
}

//prize  add by lpp 20190727 start

extern int Battery_Charging;
//Reflective Charging Mode  otg=0  trx=1  chipen=0
//Wireless Charging Mode    otg=0  trx=0  chipen=0
static ssize_t enable_tx_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t count)
{
    int error;
    unsigned int temp;
	
	struct i2c_client *client = to_i2c_client(dev);
	struct MT5725_dev *chip = i2c_get_clientdata(client);

    error = kstrtouint(buf, 10, &temp);
	printk("LPP---enable_tx_store temp=%d\n",temp);
		
    if (error)
        return error;
    if(temp==1 && Battery_Charging==0) {
			mutex_lock(&chip->ops_mutex);
			atomic_set(&mte->is_tx_mode,1);
			gpio_direction_output(chip->trxset_gpio,1);
			gpio_direction_output(chip->otgen_gpio,0); //select wirelessrx
			//ldo_disable();
			//charger_dev_enable_otg(g_info->primary_charger, true);
			//charger_dev_set_boost_current_limit(g_info->primary_charger, 1500000);
			//charger_dev_kick_wdt(g_info->primary_charger);
			//enable_boost_polling(true);
			mt_vbus_reverse_on();
			mutex_unlock(&chip->ops_mutex);
			printk(KERN_INFO"mt5725 enable tx\n");
    }else{
			mutex_lock(&chip->ops_mutex);
			gpio_direction_output(chip->trxset_gpio,0);
			//charger_dev_enable_otg(g_info->primary_charger, false);
			//enable_boost_polling(false);
			mt_vbus_off();
			gpio_direction_output(chip->otgen_gpio,0);//prize set to usb mode lpp
			atomic_set(&mte->is_tx_mode,0);
			mutex_unlock(&chip->ops_mutex);
			printk(KERN_INFO"mt5725 disable_tx\n");
	}
    return count;
}

static DEVICE_ATTR(enabletx, 0664, enable_tx_show, enable_tx_store);
//prize  add by lpp 20190727 end

//prize add by lpp  20190806 start
void MT5725_Reverse_charging_selection(void)
{
		mutex_lock(&mte->ops_mutex);
		gpio_direction_output(mte->trxset_gpio,0);
		//charger_dev_enable_otg(g_info->primary_charger, false);
		//enable_boost_polling(false);
		mt_vbus_off();
		gpio_direction_output(mte->otgen_gpio,0);  //prize set to usb mode
		atomic_set(&mte->is_tx_mode,0);
		mutex_unlock(&mte->ops_mutex);
		printk(KERN_INFO"mt5725 MT5725_Reverse_charging_selection\n");
}
EXPORT_SYMBOL(MT5725_Reverse_charging_selection);
//prize add by lpp  20190806 end

static ssize_t disable_tx_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct MT5725_dev *chip = i2c_get_clientdata(client);

	mutex_lock(&chip->ops_mutex);
	gpio_direction_output(chip->trxset_gpio,0);
	//charger_dev_enable_otg(g_info->primary_charger, false);
	//enable_boost_polling(false);
	mt_vbus_off();
	gpio_direction_output(chip->otgen_gpio,0);
	atomic_set(&mte->is_tx_mode,0);
	mutex_unlock(&chip->ops_mutex);
	printk(KERN_INFO"mt5725 disable_tx\n");
	return sprintf(buf, "%d", 1);
}
static DEVICE_ATTR(disabletx, 0644, disable_tx_show, NULL);
#endif
//prize add by lipengpeng 20190830 end 
static const struct of_device_id match_table[] = {
    {.compatible = "maxictech,mt5725",},
    { },
};

static const struct regmap_config MT5725_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .max_register = 0xFFFF,  
};

static int MT5725_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    struct MT5725_dev *chip;
    int irq_flags = 0;
    vuc chipid;
    int rc = 0;

    pr_err("MT5725 probe.\n");
    chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
    if (!chip)
        return -ENOMEM;

    //wake_lock_init(&chip->wakelock, WAKE_LOCK_SUSPEND, "wireless charger suspend wakelock");
    mutex_init(&chip->slock);
	mutex_init(&chip->ops_mutex);

    pr_err("MT5725 chip.\n");
    chip->regmap = regmap_init_i2c(client, &MT5725_regmap_config);
    if (!chip->regmap) {
        pr_err("parent regmap is missing\n");
        return -EINVAL;
    }
    pr_err("MT5725 regmap.\n");
	
    chip->client = client;
    chip->dev = &client->dev;

    chip->bus.read = MT5725_read;
    chip->bus.write = MT5725_write;
    chip->bus.read_buf = MT5725_read_buffer;
    chip->bus.write_buf = MT5725_write_buffer;

    device_init_wakeup(chip->dev, true);
    
    sysfs_create_group(&client->dev.kobj, &mt5725_sysfs_group);
 
    pr_err("MT5725 probed successfully\n");

    mte = chip;
	mte->is_samsung_charge =0;

    INIT_DELAYED_WORK(&chip->eint_work, MT5725_eint_work);
    rc = MT5725_parse_dt(client, chip);
    if (rc ) {
    	pr_err("%s: failed to parse device tree node\n", __func__);
        chip->trxset_gpio = -1;
        chip->irq_gpio = -1;
        chip->otgen_gpio = -1;
        chip->pwrst_gpio = -1;
        chip->chipen_gpio = -1;
    }

    if (gpio_is_valid(chip->trxset_gpio)) {
        rc = devm_gpio_request_one(&client->dev, chip->trxset_gpio, GPIOF_DIR_IN, "mt5725_trxset");
        if (rc){
			pr_err("%s: trxset request failed\n", __func__);
			goto err;
        }
    } else {
		pr_err("%s: trxset_gpio %d is invalid\n", __func__, chip->trxset_gpio);
    }
	
	if (gpio_is_valid(chip->otgen_gpio)) {
        rc = devm_gpio_request_one(&client->dev, chip->otgen_gpio, GPIOF_DIR_IN, "mt5725_otgen");
        if (rc){
			pr_err("%s: mt5725_otgen request failed\n", __func__);
			goto err;
        }
    } else {
		pr_err("%s: otgen_gpio %d is invalid\n", __func__, chip->otgen_gpio);
    }
	
	if (gpio_is_valid(chip->pwrst_gpio)) {
        rc = devm_gpio_request_one(&client->dev, chip->pwrst_gpio, GPIOF_DIR_IN, "mt5725_pwrst");
        if (rc){
			pr_err("%s: pwrst_gpio request failed\n", __func__);
			goto err;
        }
    } else {
		pr_err("%s: pwrst_gpio %d is invalid\n", __func__, chip->pwrst_gpio);
    }
	
	if (gpio_is_valid(chip->chipen_gpio)) {
        rc = devm_gpio_request_one(&client->dev, chip->chipen_gpio, GPIOF_DIR_IN, "mt5725_chipen");
        if (rc){
			pr_err("%s: chipen_gpio request failed\n", __func__);
			goto err;
        }
		gpio_direction_input(chip->chipen_gpio);
    } else {
		pr_err("%s: chipen_gpio %d is invalid\n", __func__, chip->chipen_gpio);
    }
	atomic_set(&chip->is_tx_mode,0);

    if (gpio_is_valid(chip->irq_gpio)) {
        rc = devm_gpio_request_one(&client->dev, chip->irq_gpio,
			GPIOF_DIR_IN, "mt5725_int");
        if (rc) {
			pr_err("%s: irq_gpio request failed\n", __func__);
			goto err;
        }

        irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
        rc = devm_request_threaded_irq(&client->dev, gpio_to_irq(chip->irq_gpio),
						NULL, MT5725_irq, irq_flags, "mt5725", chip);
        if (rc != 0) {
			pr_err("failed to request IRQ %d: %d\n", gpio_to_irq(chip->irq_gpio), rc);
			goto err;
        }
		pr_err("sucess to request IRQ %d: %d\n", gpio_to_irq(chip->irq_gpio), rc);

		//start add by sunshuai
		if(!(gpio_get_value(mte->irq_gpio))){
			pr_err("%s The interruption has come \n", __func__);
			MT5725_irq_handle();
		}
		//end   add by sunshuai
    } else {
		pr_info("%s skipping IRQ registration\n", __func__);
    }

	chipid.value =0;
    MT5725_read_buffer(mte, REG_CHIPID, chipid.ptr,2);
    if(chipid.value == MT5725ID){
		pr_err("ID Correct query\n");
	} else {
		pr_err("ID error :%d\n ", chipid.value);
	}
	rc = sysfs_create_link(kernel_kobj,&client->dev.kobj,"wirelessrx");
	if (rc){
		pr_err(KERN_ERR"mt5725 sysfs_create_link fail\n");
	}

 //prize add by lpp 20190821 start Get whether the device is close when the mobile phone is in a backcharging state
#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
	rc = device_create_file(&client->dev, &dev_attr_enabletx);
	if (rc){
		pr_err(KERN_ERR"mt5725 failed device_create_file(dev_attr_enabletx)\n");
	}
	rc = device_create_file(&client->dev, &dev_attr_disabletx);
	if (rc){
		pr_err(KERN_ERR"mt5725 failed device_create_file(dev_attr_disabletx)\n");
	}
	rc = device_create_file(&client->dev, &dev_attr_gettxflag);
	if (rc){
		pr_err(KERN_ERR"mt5725 failed device_create_file(dev_attr_gettxflag)\n");
	}
#endif
//prize add by lpp 20190821 end Get whether the device is close when the mobile phone is in a backcharging state


	i2c_set_clientdata(client,chip);

	is_5715_probe_done = 1;

err:
    return rc;
}

static int MT5725_remove(struct i2c_client *client) {

	sysfs_remove_link(kernel_kobj,"wirelessrx");

#if defined (CONFIG_PRIZE_REVERE_CHARGING_MODE)
	device_remove_file(&client->dev, &dev_attr_enabletx);
	device_remove_file(&client->dev, &dev_attr_disabletx);
	device_remove_file(&client->dev, &dev_attr_gettxflag);
#endif
    
    sysfs_remove_group(&client->dev.kobj, &mt5725_sysfs_group);

    if (gpio_is_valid(mte->irq_gpio))
        devm_gpio_free(&client->dev, mte->irq_gpio);
    if (gpio_is_valid(mte->trxset_gpio))
        devm_gpio_free(&client->dev, mte->trxset_gpio);
	if (gpio_is_valid(mte->otgen_gpio))
        devm_gpio_free(&client->dev, mte->otgen_gpio);
	if (gpio_is_valid(mte->pwrst_gpio))
        devm_gpio_free(&client->dev, mte->pwrst_gpio);
	if (gpio_is_valid(mte->chipen_gpio))
        devm_gpio_free(&client->dev, mte->chipen_gpio);

    return 0;
}


static const struct i2c_device_id MT5725_dev_id[] = {
    {"MT5715_receiver", 0},
    {},
};
MODULE_DEVICE_TABLE(i2c, MT5725_dev_id);

static struct i2c_driver MT5725_driver = {
    .driver   = {
        .name           = DEVICE_NAME,
        .owner          = THIS_MODULE,
        .of_match_table = match_table,
    },
    .probe    = MT5725_probe,
    .remove   = MT5725_remove,
    .id_table = MT5725_dev_id,
};
module_i2c_driver(MT5725_driver);

MODULE_AUTHOR("Yangwl@maxictech.com");
MODULE_DESCRIPTION("MT5725 Wireless Power Receiver");
MODULE_LICENSE("GPL v2");
