/* 
 * drivers/input/touchscreen/CST2XX.c
 *
 * hynitron TouchScreen driver. 
 *
 * Copyright (c) 2015  hynitron
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * VERSION      	DATE			AUTHOR
 *  1.0		    2015-10-12		    Tim
 *
 * note: only support mulititouch
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/byteorder/generic.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
//#include <linux/rtpm_prio.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/uaccess.h>
//#include "mt_boot_common.h"
#include <uapi/linux/sched/types.h>

#include "tpd.h"
#include CONFIG_TOUCHSCREEN_MTK_CST3XX_FW_NAME	//"cst3xx_fw.h"

#if defined(CONFIG_PRIZE_HARDWARE_INFO)
#include "../../../../misc/mediatek/hardware_info/hardware_info.h"
extern struct hardware_info current_tp_info;
#endif

//#define GTP_RST_PORT    0
//#define GTP_INT_PORT    1

//#define TPD_PROXIMITY
#ifdef TPD_PROXIMITY
#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include <wakelock.h> 
static u8 tpd_proximity_flag   = 0;     //flag whether start alps
static u8 tpd_proximity_detect = 1;     //0-->close ; 1--> far away
//static struct wake_lock ps_lock;
//static u8 gsl_psensor_data[8]  = {0};
#endif


//#define HYN_GESTURE
#ifdef HYN_GESTURE
//static int hyn_gesture_flag = 0;
static int hyn_lcd_flag = 0;
//static struct input_dev *hyn_power_idev;
//static char hyn_gesture_c = 0;
static int tpd_halt=0;

#if 0
#define  HYN_TPD_GES_COUNT  14
static int tpd_keys_gesture[HYN_TPD_GES_COUNT] ={KEY_POWER, \
												TPD_GES_KEY_DOUBLECLICK,\
												TPD_GES_KEY_LEFT,\
												TPD_GES_KEY_RIGHT,\
												TPD_GES_KEY_UP,\
												TPD_GES_KEY_DOWN,\
												TPD_GES_KEY_O,\
												TPD_GES_KEY_W,  \
												TPD_GES_KEY_M,\
												TPD_GES_KEY_E,\
												TPD_GES_KEY_C,\
												TPD_GES_KEY_S,\
												TPD_GES_KEY_V,\
												TPD_GES_KEY_Z};
#endif

#endif 


//#define CONFIG_TP_ESD_PROTECT
#ifdef CONFIG_TP_ESD_PROTECT
#define SWITCH_ESD_OFF                  0
#define SWITCH_ESD_ON                   1
static struct workqueue_struct *cst3xx_esd_workqueue;
#endif


//#define CST2XX_SUPPORT
//#define ANDROID_TOOL_SURPORT
//#define HYN_SYSFS_NODE_EN
#ifdef ANDROID_TOOL_SURPORT
static  unsigned short g_unnormal_mode = 0;
static unsigned short g_cst3xx_tx = 15;
static unsigned short g_cst3xx_rx = 9;

static  unsigned short hyn_tp_test_flag=0;
static DECLARE_WAIT_QUEUE_HEAD(dbg_waiter);
//static int tpd_flag = 0;

#ifdef HYN_SYSFS_NODE_EN
static struct mutex g_device_mutex;
static DEFINE_MUTEX(g_device_mutex);
static struct kobject *k_obj = NULL;
#endif
#endif



#define ICS_SLOT_REPORT	//A/B
#define REPORT_XY_SWAP
#define SLEEP_CLEAR_POINT
//#define HIGH_SPEED_IIC_TRANSFER
#define TRANSACTION_LENGTH_LIMITED



#pragma pack(1)
typedef struct
{
    u16 pid;                 //product id   //
    u16 vid;                 //version id   //
} st_tpd_info;
#pragma pack()

st_tpd_info tpd_info;

static unsigned char  report_flag = 0;
//static unsigned char  key_index = 0xFF;
static unsigned int   g_cst3xx_ic_version  = 0;
static unsigned int   g_cst3xx_ic_checksum = 0;
static unsigned int   touch_irq = 0;


static DECLARE_WAIT_QUEUE_HEAD(waiter);
static struct task_struct *thread = NULL;
static int tpd_flag = 0;


extern struct tpd_device  *tpd;
static struct i2c_client *g_i2c_client = NULL;
struct input_dev *hyn_input_dev =NULL;





//static unsigned int hyn_ic_type=340;
static int cst3xx_boot_update_fw(struct i2c_client   *client,  unsigned char *pdata);
static unsigned char *pcst3xx_update_firmware = (unsigned char *)cst3_fw ; //the updating firmware
static int cst3xx_update_firmware(struct i2c_client * client, const unsigned char *pdata);
static int cst3xx_firmware_info(struct i2c_client * client);
 

#ifdef ICS_SLOT_REPORT
#include <linux/input/mt.h> // Protocol B, for input_mt_slot(), input_mt_init_slot()
#endif


//#define TPD_HAVE_BUTTON
#ifdef TPD_HAVE_BUTTON
#define TPD_KEY_COUNT	2
#define TPD_KEYS		{KEY_MENU,KEY_BACK}
/* {button_center_x, button_center_y, button_width, button_height*/
#define TPD_KEYS_DIM	{{180, 1360, 60, 60},{540, 1360, 60, 60}}
static int tpd_keys_local[TPD_KEY_COUNT] = TPD_KEYS;
static int tpd_keys_dim_local[TPD_KEY_COUNT][4] = TPD_KEYS_DIM;
#endif


//#define GTP_HAVE_TOUCH_KEY
#ifdef GTP_HAVE_TOUCH_KEY
#define TPD_KEY_COUNT	2
//#define TPD_KEYS		{KEY_MENU, KEY_HOMEPAGE, KEY_BACK, KEY_SEARCH}
#define TPD_KEYS		{KEY_MENU,KEY_BACK}
const u16 touch_key_array[] = TPD_KEYS;
//#define GTP_MAX_KEY_NUM ( sizeof( touch_key_array )/sizeof( touch_key_array[0] ) )
#endif


#define TPD_DEVICE_NAME			    "Hyn_device" //TPD_DEVICE
#define TPD_DRIVER_NAME			    "Hyn_driver"
#define TPD_MAX_FINGERS			    5
#define TPD_MAX_X				    480
#define TPD_MAX_Y				    800
#define CST3XX_I2C_ADDR				0x1a
#define I2C_BUS_NUMBER             1 //IIC bus num for mtk
#define DEBUG_CST3XX               1 //IIC bus num for mtk
#if DEBUG_CST3XX
	#define PRINTK(fmt,arg...)  printk(" linc HYN:[LINE=%d]"fmt,__LINE__, ##arg)							
	#define PRINTK(fmt,arg...)  printk(" linc HYN:[LINE=%d]"fmt,__LINE__, ##arg)

#else
	#define PRINTK(fmt,arg...)   							
	#define PRINTK(fmt,arg...)   
#endif


#ifdef HYN_GESTURE
static u16 hyn_ges_wakeup_switch = 1;
static unsigned char hyn_gesture_c = 0;

#if 0
static char  hyn_ges_wakeup_flag = 0;
static unsigned char hyn_ges_cmd[48] = {0};
static unsigned int hyn_ges_num = 0;
static struct proc_dir_entry   *tp_proc_file = NULL;

static ssize_t proc_gesture_switch_write(struct file *flip,const char __user *buf, size_t count, loff_t *data)
{	
	if (copy_from_user(&hyn_ges_wakeup_flag, buf, count))
		return -EFAULT;
	PRINTK(" linc -liuhx --%s---%c\n",__func__,hyn_ges_wakeup_flag);
	switch(hyn_ges_wakeup_flag)
	{
		case '0':
			hyn_ges_wakeup_switch = 0;
			break;
		default:
			hyn_ges_wakeup_switch = 1;
			break;
	}
	return count;	
}

static ssize_t proc_gesture_switch_read(struct file *flip, char __user *user_buf, size_t count, loff_t *ppos)
{	
	int len = 0;

	PRINTK(" linc --%s--\n",__func__);
	len = simple_read_from_buffer(user_buf, count, ppos, &hyn_ges_wakeup_flag, 1);
	
	return len;
}

static ssize_t proc_gesture_data_write(struct file *flip,const char __user *buffer, size_t count, loff_t *data)
{

	int i =0;

	if(count > 48)
		count =48;
	
	if (copy_from_user(hyn_ges_cmd, buffer, count))
		return -EFAULT;
	
	hyn_ges_num = 0;
	//memcpy(hyn_ges_cmd, buffer, count);
	for(i = 0; i < count; i++){
		if(buffer[i] != 0){
			//--hyn_ges_cmd[hyn_ges_num ++] = buffer[i];	
			PRINTK(" linc  hyn_ges_cmd[%d] :0x%x\n",i,hyn_ges_cmd[i]);
		}

	//DBG("buffer[%d] = %x\n",buffer[i]);
	hyn_ges_num = count;
	}
	
	return count;

}

static ssize_t proc_gesture_data_read(struct file *flip, char __user *user_buf, size_t count, loff_t *ppos)
{
	int len = 0;

	for (len = 0; len < hyn_ges_num; len++)
		PRINTK(" linc buff[%d] = %c,", len, hyn_ges_cmd[len]);
	PRINTK(" linc \n");
	return len;
}

static ssize_t proc_gesture_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	PRINTK(" linc --%s--\n",__func__);

	sscanf(buffer, "%x", (unsigned int *)&hyn_gesture_c);

	return count;

}

static int proc_gesture_show(struct seq_file *m, void *v)
{
	PRINTK(" linc --%s--\n",__func__);

	seq_printf(m, "%#x\n", hyn_gesture_c);
	return 0;

}



static int proc_gesture_open(struct inode *inode, struct file *file)
{
        PRINTK(" linc --%s--\n",__func__);
	return single_open(file, proc_gesture_show, NULL);
}

static const struct file_operations proc_gesture_fops = {
	.owner		= THIS_MODULE,
	.open		= proc_gesture_open,
	.read		= seq_read,
	.write		= proc_gesture_write, 
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations proc_gesture_switch_fops = {
	.write	=	proc_gesture_switch_write,
	.read	=	proc_gesture_switch_read,
};
static const struct file_operations proc_gesture_data_fops = {
	.write	=	proc_gesture_data_write,
	.read	=	proc_gesture_data_read,
};



static int gesture_proc_init(void)
{

   int rc;

   tp_proc_file = proc_create("gesture", 0777, NULL,&proc_gesture_fops);
   if (tp_proc_file == NULL)
   {
      rc = -ENOMEM;
      goto err_del_gesture;
   }

   tp_proc_file = proc_create_data("gesture_switch", 0777, NULL,&proc_gesture_switch_fops,NULL);
   if (tp_proc_file == NULL)
   {
      rc = -ENOMEM;
      goto err_del_gesture_switch;
   }

   tp_proc_file = proc_create_data("gesture_data", 0777, NULL,&proc_gesture_data_fops,NULL);
   if (tp_proc_file == NULL)
   {
      rc = -ENOMEM;
      goto err_del_gesture_data;
   }

   return 0;

err_del_gesture_data:
   remove_proc_entry("gesture_data", NULL);
err_del_gesture_switch:
   remove_proc_entry("gesture_switch", NULL);
err_del_gesture:
   remove_proc_entry("gesture", NULL);
   return rc;
}


#endif
#endif


#ifdef HIGH_SPEED_IIC_TRANSFER
static int cst3xx_i2c_read(struct i2c_client *client, unsigned char *buf, int len) 
{ 
	struct i2c_msg msg; 
	int ret = -1; 
	int retries = 0; 
	
	msg.flags |= I2C_M_RD; 
	msg.addr   = client->addr;
	msg.len    = len; 
	msg.buf    = buf;	

	while (retries < 2) { 
		ret = i2c_transfer(client->adapter, &msg, 1); 
		if(ret == 1)
			break; 
		retries++; 
	} 
	
	return ret; 
} 


/*******************************************************
Function:
    read data from register.
Input:
    buf: first two byte is register addr, then read data store into buf
    len: length of data that to read
Output:
    success: number of messages
    fail:	negative errno
*******************************************************/
static int cst3xx_i2c_read_register(struct i2c_client *client, unsigned char *buf, int len) 
{ 
	struct i2c_msg msgs[2]; 
	int ret = -1; 
	int retries = 0; 
	
	msgs[0].flags = client->flags & I2C_M_TEN;
	msgs[0].addr  = client->addr;  
	msgs[0].len   = 2;
	msgs[0].buf   = buf; 

	msgs[1].flags |= I2C_M_RD;
	msgs[1].addr   = client->addr; 
	msgs[1].len    = len; 
	msgs[1].buf    = buf;

	while (retries < 2) { 
		ret = i2c_transfer(client->adapter, msgs, 2); 
		if(ret == 2)
			break; 
		retries++; 
	} 
	
	return ret; 
} 

static int cst3xx_i2c_write(struct i2c_client *client, unsigned char *buf, int len) 
{ 
	struct i2c_msg msg; 
	int ret = -1; 
	int retries = 0;

	msg.flags = client->flags & I2C_M_TEN; 
	msg.addr  = client->addr; 
	msg.len   = len; 
	msg.buf   = buf;		  
	  
	while (retries < 2) { 
		ret = i2c_transfer(client->adapter, &msg, 1); 
		if(ret == 1)
			break; 
		retries++; 
	} 	
	
	return ret; 
}

#else
static int cst3xx_i2c_read(struct i2c_client *client, unsigned char *buf, int len) 
{ 
	int ret = -1; 
	int retries = 0; 

	while (retries < 2) { 
		ret = i2c_master_recv(client, buf, len); 
		if(ret<=0) 
		    retries++;
        else
            break; 
	} 
	
	return ret; 
} 

static int cst3xx_i2c_write(struct i2c_client *client, unsigned char *buf, int len) 
{ 
	int ret = -1; 
	int retries = 0; 

	while (retries < 2) { 
		ret = i2c_master_send(client, buf, len); 
		if(ret<=0) 
		    retries++;
        else
            break; 
	} 
	
	return ret; 
}

static int cst3xx_i2c_read_register(struct i2c_client *client, unsigned char *buf, int len) 
{ 
	int ret = -1; 
    
    ret = cst3xx_i2c_write(client, buf, 2);

    ret = cst3xx_i2c_read(client, buf, len);
	
    return ret; 
} 
#endif


static void cst3xx_reset_ic(unsigned int ms)
{   

#if 1
	unsigned char buf[4];
	buf[0] = 0xD1;
	buf[1] = 0x0E;	 
	cst3xx_i2c_write(g_i2c_client, buf, 2);
#else

	tpd_gpio_output(GTP_RST_PORT, 0);
	mdelay(100);
	tpd_gpio_output(GTP_RST_PORT, 1);

#endif
	mdelay(ms);
	
}


/*******************************************************
Function:
    test i2c communication
Input:
    client: i2c client
Output:

    success: big than 0
    fail:	negative
*******************************************************/
static int cst3xx_i2c_test(struct i2c_client *client)
{
	int retry = 0;
	int ret=-1;
	unsigned char buf[4];

	buf[0] = 0xD1;
	buf[1] = 0x06;
	while (retry++ < 5) {
		ret = cst3xx_i2c_write(client, buf, 2);
		if (ret > 0)
			return ret;
		
		mdelay(2);		
	}

    if(retry==5) 
		printk("linc cst3xx hyn I2C TEST error.ret:%d;\n", ret);
	
	return ret;
}

#ifdef TPD_PROXIMITY
static int tpd_get_ps_value(void)
{
	return tpd_proximity_detect;  //send to OS to controll backlight on/off
}

static int tpd_enable_ps(int enable)
{
	u8 buf[4];
	
	if (enable) {
		//wake_lock(&ps_lock);
		buf[0] = 0xD0;
		buf[1] = 0x4B;
		buf[2] = 0x80;
		cst3xx_i2c_write(g_i2c_client, buf, 3);
		
		tpd_proximity_flag = 1;
		//add alps of function
		PRINTK(" linc tpd-ps function is on\n");
	}
	else {
		tpd_proximity_flag = 0;
		//wake_unlock(&ps_lock);
		
		buf[0] = 0xD0;
		buf[1] = 0x4B;
		buf[2] = 0x00;
		cst3xx_i2c_write(g_i2c_client, buf, 3);
		
		PRINTK(" linc tpd-ps function is off\n");
	}
	return 0;
}

static int tpd_ps_operate(void* self, uint32_t command, void* buff_in, int size_in,
        void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *sensor_data;

	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int))) {
				PRINTK("linc cst3xxSet delay parameter error!\n");
				err = -EINVAL;
			}
			// Do nothing
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int))) {
				PRINTK("linc cst3xxEnable sensor parameter error!\n");
				err = -EINVAL;
			}
			else {
				value = *(int *)buff_in;
				if(value) {
				
					if((tpd_enable_ps(1) != 0)) {
						PRINTK("linc cst3xxenable ps fail: %d\n", err);
						return -1;
					}
				}
				else {
					if((tpd_enable_ps(0) != 0)) {
						PRINTK("linc cst3xxdisable ps fail: %d\n", err);
						return -1;
					}
				}
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(struct hwm_sensor_data))) {
				PRINTK("linc cst3xxget sensor data parameter error!\n");
				err = -EINVAL;
			}
			else {
				sensor_data = (struct hwm_sensor_data *)buff_out;

				sensor_data->values[0] = tpd_get_ps_value();
				sensor_data->value_divide = 1;
				sensor_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
			}
			break;

		default:
			PRINTK(" linc proxmy sensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}
	return err;
}
#endif

#ifdef HYN_GESTURE
void hyn_key_report(int key_value)
{
#if 1
	input_report_key(tpd->dev, key_value, 1);
	input_sync(tpd->dev);	
	input_report_key(tpd->dev, key_value, 0);
	input_sync(tpd->dev);
#else
	input_report_key(tpd->dev, KEY_POWER, 1);
	input_sync(tpd->dev);
	input_report_key(tpd->dev, KEY_POWER, 0);
	input_sync(tpd->dev);
#endif
}

#if 0
static ssize_t hyn_sysfs_tpgesture_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    ssize_t len=0;
    sprintf(&buf[len],"%s\n","tp gesture is on/off:");
    len += (strlen("tp gesture is on/off:")+1);
    if(hyn_ges_wakeup_switch == 1){
        sprintf(&buf[len],"%s\n","  on  ");
        len += (strlen("  on  ")+1);
    }else if(hyn_ges_wakeup_switch == 0){
        sprintf(&buf[len],"%s\n","  off  ");
        len += (strlen("  off  ")+1);
    }

    sprintf(&buf[len],"%s\n","tp gesture:");
    len += (strlen("tp gesture:")+1);
    sprintf(&buf[len],"%c\n",hyn_gesture_c);
    len += 2;	
    return len;
}
static ssize_t hyn_sysfs_tpgesturet_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t count)
{
    char tmp_buf[16];

    if(copy_from_user(tmp_buf, buf, (count>16?16:count))){
        return -1;
    }
    if(buf[0] == '0'){
       	hyn_ges_wakeup_switch = 0;
        PRINTK("linc cst3xx[HYN_GESTURE] hyn_sysfs_tpgesturet_store off.\n");
    }else if(buf[0] == '1'){
        hyn_ges_wakeup_switch = 1;
        PRINTK("linc cst3xx[HYN_GESTURE] hyn_sysfs_tpgesturet_store on.\n");
    }

    return count;
}

static DEVICE_ATTR(tpgesture, S_IRUGO|S_IWUSR, hyn_sysfs_tpgesture_show, hyn_sysfs_tpgesturet_store);
static void hyn_request_power_idev(void)
{
    struct input_dev *idev;
    int rc = 0;
    idev = input_allocate_device();


    if(!idev){
        return;
    }
    hyn_power_idev = idev; 
    idev->name = "hyn_gesture";  
    idev->id.bustype = BUS_I2C;
    input_set_capability(idev,EV_KEY,KEY_POWER);
    input_set_capability(idev,EV_KEY,KEY_END);
	
    rc = input_register_device(idev);
    if(rc){
        input_free_device(idev);
        hyn_power_idev = NULL;
    }
}
static unsigned int hyn_gesture_init(void)
{
    int ret;
    struct kobject *hyn_debug_kobj;
	
    hyn_debug_kobj = kobject_create_and_add("hyn_gesture", NULL) ;
    if (hyn_debug_kobj == NULL)
    {
        PRINTK("linc cst3xx%s: subsystem_register failed\n", __func__);
        return -ENOMEM;
    }
    ret = sysfs_create_file(hyn_debug_kobj, &dev_attr_tpgesture.attr);
    if (ret)
    {
        PRINTK("linc cst3xx%s: sysfs_create_version_file failed\n", __func__);
        return ret;
    }
    hyn_request_power_idev();
    PRINTK("linc cst3xx hyn_gesture_init success.\n");
    return 1;
}
#endif

#endif



static void cst3xx_touch_down(struct input_dev *input_dev,s32 id,s32 x,s32 y,s32 w)
{
    s32 temp_w = (w>>2);
	
#ifdef ICS_SLOT_REPORT
    input_mt_slot(input_dev, id);
    input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
    input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
    input_report_abs(input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
    input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, temp_w);
    input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, temp_w);
	input_report_abs(input_dev, ABS_MT_PRESSURE, temp_w);
#else
    input_report_key(input_dev, BTN_TOUCH, 1);
    input_report_abs(input_dev, ABS_MT_POSITION_X, x);
    input_report_abs(input_dev, ABS_MT_POSITION_Y, y);
    input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, temp_w);
//  input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, temp_w);
//  input_report_abs(input_dev, ABS_MT_PRESSURE, temp_w);
    input_report_abs(input_dev, ABS_MT_TRACKING_ID, id);
    input_mt_sync(input_dev);
#endif

#if 0
	if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode()) {   
		tpd_button(x, y, 1);  
	}
#endif
}

static void cst3xx_touch_up(struct input_dev *input_dev, int id)
{
#ifdef ICS_SLOT_REPORT
    input_mt_slot(input_dev, id);
    input_report_abs(input_dev, ABS_MT_TRACKING_ID, -1);
    input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
#else
    input_report_key(input_dev, BTN_TOUCH, 0);
    input_mt_sync(input_dev);
#endif

#if 0
	if (FACTORY_BOOT == get_boot_mode()|| RECOVERY_BOOT == get_boot_mode()) {   
	    tpd_button(0, 0, 0);  
	}
#endif
}

#ifdef ANDROID_TOOL_SURPORT   //debug tool support
#define CST3XX_PROC_DIR_NAME	"cst1xx_ts"
#define CST3XX_PROC_FILE_NAME	"cst1xx-update"
static struct proc_dir_entry *g_proc_dir, *g_update_file;
static int CMDIndex = 0;
#if 0
static struct file *cst3xx_open_fw_file(char *path)
{
	struct file * filp = NULL;
	int ret;
	
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
        ret = PTR_ERR(filp);
        return NULL;
    }
    filp->f_op->llseek(filp, 0, 0);
	
    return filp;
}

static void cst3xx_close_fw_file(struct file * filp)
{
	 if(filp)
	 filp_close(filp,NULL);
}

static int cst3xx_read_fw_file(unsigned char *filename, unsigned char *pdata, int *plen)
{
	struct file *fp;
	int size;
	int length;
	int ret = -1;

	if((pdata == NULL) || (strlen(filename) == 0)) {
		PRINTK("linc cst3xxfile name is null.\n");
		return ret;
	}
	fp = cst3xx_open_fw_file(filename);
	if(fp == NULL) {		
        PRINTK("linc cst3xxOpen bin file faild.path:%s.\n", filename);
		goto clean;
	}
	
	length = fp->f_op->llseek(fp, 0, SEEK_END); 
	fp->f_op->llseek(fp, 0, 0);	
	size = fp->f_op->read(fp, pdata, length, &fp->f_pos);
	if(size == length) {
    	ret = 0;
    	*plen = length;
	} 	

clean:
	cst3xx_close_fw_file(fp);
	return ret;
}

#else 

static struct file *cst3xx_open_fw_file(char *path, mm_segment_t * old_fs_p)
{
	struct file * filp;
	int ret;
	
	*old_fs_p = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) 
	{
        ret = PTR_ERR(filp);
        return NULL;
    }
    filp->f_op->llseek(filp, 0, 0);
	
    return filp;
}

static void cst3xx_close_fw_file(struct file * filp,mm_segment_t old_fs)
{
	//set_fs(old_fs);
	if(filp)
	    filp_close(filp,NULL);
}

static int cst3xx_read_fw_file(unsigned char *filename, unsigned char *pdata, int *plen)
{
	struct file *fp;
	int ret = -1;
	loff_t pos;
	off_t fsize;
	struct inode *inode;
	unsigned long magic;
	mm_segment_t old_fs;

	printk("cst3xx_read_fw_file enter.\n");

	if((pdata == NULL) || (strlen(filename) == 0)) {
		printk(" cst3xxfile name is null.\n");
		return ret;
	}
	fp = cst3xx_open_fw_file(filename,&old_fs);
	if(fp == NULL) {
        printk(" cst3xxOpen bin file faild.path:%s.\n", filename);
		goto clean;
	}
	
	#if 0

	length = fp->f_op->llseek(fp, 0, SEEK_END);
	fp->f_op->llseek(fp, 0, 0);
	size = fp->f_op->read(fp, pdata, length, &fp->f_pos);
	if(size == length)
	{
    	ret = 0;
    	*plen = length;
	}
	else
	{
		printk("read bin file length fail****size:%d*******length:%d .\n", size,length);

	}

	#else

	if (IS_ERR(fp)) {
		printk("error occured while opening file %s.\n", filename);
		return -EIO;
	}
	inode = fp->f_inode;
	magic = inode->i_sb->s_magic;
	fsize = inode->i_size;
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	ret=vfs_read(fp, pdata, fsize, &pos);
	if(ret==fsize){
		printk("vfs_read success.ret:%d.\n",ret);
	}else{
		printk("vfs_read fail.ret:%d.\n",ret);
	}
	filp_close(fp, NULL);
	set_fs(old_fs);

	printk("vfs_read done.\n");

	#endif


clean:
	cst3xx_close_fw_file(fp, old_fs);
	return ret;
}


#endif

static int cst3xx_apk_fw_dowmload(struct i2c_client *client,
		unsigned char *pdata, int length) 
{ 
	int ret=-1;

	PRINTK(" linc cst3xx enter cst3xx_apk_fw_dowmload .\n");
	pcst3xx_update_firmware=pdata;

#ifdef CST2XX_SUPPORT
	if(hyn_ic_type==18868) {
	    ret = cst2xx_update_firmware(g_i2c_client, pdata);
	   return ret;
	}else if(hyn_ic_type==340) 
#endif	
	{
		ret = cst3xx_update_firmware(g_i2c_client, pdata);
		return ret;
	}

	return 0;
}

static ssize_t cst3xx_proc_read_foobar(struct file *page,char __user *user_buf, size_t count, loff_t *data)
{	
	unsigned char buf[1024];
	int len = 0;
	int ret;

	printk(" linc cst3xx is entering cst3xx_proc_read_foobar.\n");

#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue != NULL)
	cst3xx_esd_switch(SWITCH_ESD_OFF);
#endif

	if (CMDIndex == 0) {
		buf[0] = 0xD1;
		buf[1] = 0x01;
		ret = cst3xx_i2c_write(g_i2c_client, buf, 2);
		if (ret < 0)
			return -1;

		mdelay(10);

		buf[0] = 0xD1;
		buf[1] = 0xF4;//E8
		ret = cst3xx_i2c_read_register(g_i2c_client, buf, 28);//4   //4
		if (ret < 0)
			return -1;

		g_cst3xx_tx=buf[0];
		g_cst3xx_rx=buf[2];

		printk(buf, "  cst3xx_proc_read_foobar:g_cst3xx_tx:%d,g_cst3xx_rx:%d.\n",g_cst3xx_tx,g_cst3xx_rx);
       	 len = 4;
		ret = copy_to_user(user_buf,buf,len);

	}
	else if (CMDIndex == 1) {
		buf[0] = 0xD1;
		buf[1] = 0x01;
		ret = cst3xx_i2c_write(g_i2c_client, buf, 2);
		if (ret < 0) return -1;

		mdelay(10);

		buf[0] = 0xD1;
		buf[1] = 0xF4;
		ret = cst3xx_i2c_read_register(g_i2c_client, buf, 28);
		if (ret < 0) return -1;

		g_cst3xx_tx=buf[0];
		g_cst3xx_rx=buf[2];

		printk("  cst3xx_proc_read_foobar:g_cst3xx_tx:%d,g_cst3xx_rx:%d.\n",g_cst3xx_tx,g_cst3xx_rx);

		len = 28;
		ret = copy_to_user(user_buf,buf,len);

		buf[0] = 0xD1;
		buf[1] = 0x09;
		ret = cst3xx_i2c_write(g_i2c_client, buf, 2);
	}
	if((CMDIndex == 2 )|| (CMDIndex == 3)||(CMDIndex == 4))
	{		
		unsigned short rx,tx;
		int data_len;
		
		rx = g_cst3xx_rx;
		tx = g_cst3xx_tx;
		data_len = rx*tx*2 + 4 + (tx+rx)*2 + rx + rx; //374


		tpd_flag = 0;
		if(CMDIndex == 2)  //read diff
		{
			buf[0] = 0xD1;
			buf[1] = 0x0D;
		}
		else  if(CMDIndex == 3)        //rawdata
		{
			buf[0] = 0xD1;
			buf[1] = 0x0A;
		}
		else if(CMDIndex == 4)          //factory test
		{
			buf[0] = 0xD1;
			buf[1] = 0x19;
			data_len = rx*tx*4 +(4 + tx + rx)*2;
		}

		hyn_tp_test_flag=1;
		ret = i2c_master_send(g_i2c_client, buf, 2);  
		if(ret < 0) {			
			PRINTK("linc cst3xxWrite command raw/diff mode failed.error:%d.\n", ret);
			goto END;
		}

		g_unnormal_mode = 1;
		mdelay(14);

		wait_event(dbg_waiter, tpd_flag!=0);
		tpd_flag = 0;

	    printk(" cst3xx Read wait_event interrupt");
		if(CMDIndex == 4)
		{
			buf[0] = 0x12;
			buf[1] = 0x15;
		}
		else
		{
			buf[0] = 0x80;
			buf[1] = 0x01;
		}
		ret = cst3xx_i2c_write(g_i2c_client, buf, 2);
		if (ret < 0) {
			printk("Write command(0x8001) failed.error:%d.\n", ret);
			goto END;
		}
		ret = cst3xx_i2c_read(g_i2c_client, &buf[2], data_len);
		if(ret < 0)
		{
			printk("Read raw/diff data failed.error:%d.\n", ret);
			goto END;
		}

		mdelay(2);

		buf[0] = 0xD1;
		buf[1] = 0x08;
		ret = cst3xx_i2c_write(g_i2c_client, buf, 2); 		
		if(ret < 0)
		{
			printk("Write command normal mode failed.error:%d.\n", ret);
			goto END;
		}

		if (4 == CMDIndex) {	//factory test
			buf[0] = 0xD1;
			buf[1] = 0x02;
			ret = cst3xx_i2c_write(g_i2c_client, buf, 2);
			if (ret < 0) {
				printk("Write command normal mode failed.error:%d.\n", ret);
				goto END;
			}
		}

		buf[0] = rx;
		buf[1] = tx;
    	ret = copy_to_user(user_buf,buf,data_len + 2);
    	len = data_len + 2;

		mdelay(2);
	}



		

END:	

			
#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue != NULL)
	cst3xx_esd_switch(SWITCH_ESD_ON);
#endif
	g_unnormal_mode = 0;
	CMDIndex = 0;
	hyn_tp_test_flag=0;
	return len;
}

static ssize_t cst3xx_proc_write_foobar(struct file *file, const char __user *buffer,size_t count, loff_t *data)
{
    unsigned char cmd[128];
    unsigned char *pdata = NULL;
	int len;
	int ret=-1;
    int length = 24*1024;

	if (count > 128) 
		len = 128;
	else 
		len = count;

   PRINTK(" linc cst3xx is entering cst3xx_proc_write_foobar .\n");
    
	if (copy_from_user(cmd, buffer, len))  {
		PRINTK("linc cst3xxcopy data from user space failed.\n");
		return -EFAULT;
	}
	
	 PRINTK(" linc cmd:%d......%d.......len:%d\r\n", cmd[0], cmd[1], len);
	
	if (cmd[0] == 0) {
	    pdata = kzalloc(sizeof(char)*length, GFP_KERNEL);
	    if(pdata == NULL) {
	        PRINTK("linc cst3xxzalloc GFP_KERNEL memory fail.\n");
	        return -ENOMEM;
	    }
		ret = cst3xx_read_fw_file(&cmd[1], pdata, &length);
	  	if(ret < 0) {
			PRINTK("linc cst3xx_read_fw_file failed.\n");
			if(pdata != NULL) {
				kfree(pdata);			
				pdata = NULL;	
			}				
			return -EPERM;
	  	}else{

			PRINTK("linc cst3xx_read_fw_file success.\n");
			if(pdata==NULL){
				PRINTK("linc cst3xx_read_fw_file pdata:FAIL.\n");
			}
		}
		
		ret = cst3xx_apk_fw_dowmload(g_i2c_client, pdata, length);
	  	if(ret < 0){
	        PRINTK("linc cst3xxupdate firmware failed.\n");
			if(pdata != NULL) {
				kfree(pdata);
				pdata = NULL;	
			}	
	        return -EPERM;
		}
        mdelay(50);
		
		cst3xx_firmware_info(g_i2c_client);    
		
		if(pdata != NULL) {
			kfree(pdata);
			pdata = NULL;	
		}
	}
	else if (cmd[0] == 2) {					
		//cst3xx_touch_release();		
		CMDIndex = cmd[1];			
	}			
	else if (cmd[0] == 3) {				
		CMDIndex = 0;		
	}	
			
	return count;
}

static const struct file_operations proc_tool_debug_fops = {

	.owner		= THIS_MODULE,

	.read	    = cst3xx_proc_read_foobar,

	.write		= cst3xx_proc_write_foobar, 

	

};



static int  cst3xx_proc_fs_init(void)

{

	int ret;	

	g_proc_dir = proc_mkdir(CST3XX_PROC_DIR_NAME, NULL);

	if (g_proc_dir == NULL) {

		ret = -ENOMEM;

		goto out;

	}

   g_update_file = proc_create(CST3XX_PROC_FILE_NAME, 0777, g_proc_dir,&proc_tool_debug_fops);

   if (g_update_file == NULL)

   {

      ret = -ENOMEM;

      goto no_foo;

   }
	return 0;

no_foo:

	remove_proc_entry(CST3XX_PROC_FILE_NAME, g_proc_dir);

out:

	return ret;

}

#ifdef HYN_SYSFS_NODE_EN

static ssize_t hyn_tpfwver_show(struct device *dev,	struct device_attribute *attr,char *buf)
{
	ssize_t num_read_chars = 0;
	u8 buf1[20];
	u8 retry;
	int ret=-1;
	unsigned int firmware_version,module_version,project_version,chip_type,checksum,ic_checkcode;

	mutex_lock(&g_device_mutex);
	memset((u8 *)buf1, 0, 20);

#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue != NULL)
	cst3xx_esd_switch(SWITCH_ESD_OFF);
#endif

	firmware_version=0;
	module_version=0;
	project_version=0;
	chip_type=0;
	checksum=0;
	ic_checkcode=0;
	retry=0;

START:
	mdelay(5);
	buf1[0] = 0xD1;
	buf1[1] = 0x01;
	ret = cst3xx_i2c_write(g_i2c_client, buf1, 2);
	if (ret < 0)
	{
	    num_read_chars = snprintf(buf, 50, "hyn_tpfwver_show:write debug info command fail.\n");
		printk("%s : ret = %d. hyn_tpfwver_show:write debug info command fail.\n",__func__,ret);
		goto err_return;
	}

	mdelay(5);

	buf1[0] = 0xD1;
	buf1[1] = 0xFC;
	ret = cst3xx_i2c_read_register(g_i2c_client, buf1, 20);
	if (ret < 0)
	{
		num_read_chars = snprintf(buf, 50, "hyn_tpfwver_show:Read version resgister fail.\n");
		printk("%s : ret = %d. hyn_tpfwver_show:Read version resgister fail.\n",__func__,ret);
		goto err_return;
	}

	chip_type = buf1[11];
	chip_type <<= 8;
	chip_type |= buf1[10];

	project_version = buf1[9];
	project_version <<= 8;
	project_version |= buf1[8];

	firmware_version = buf1[15];
	firmware_version <<= 8;
	firmware_version |= buf1[14];
	firmware_version <<= 8;
	firmware_version |= buf1[13];
	firmware_version <<= 8;
	firmware_version |= buf1[12];

	checksum = buf1[19];
	checksum <<= 8;
	checksum |= buf1[18];
	checksum <<= 8;
	checksum |= buf1[17];
	checksum <<= 8;
	checksum |= buf1[16];

	//0xCACA0000
	ic_checkcode = buf1[3];
	ic_checkcode <<= 8;
	ic_checkcode |= buf1[2];
	ic_checkcode <<= 8;
	ic_checkcode |= buf1[1];
	ic_checkcode <<= 8;
	ic_checkcode |= buf1[0];

	printk("linc cst3xx [cst3xx] the chip ic_checkcode:0x%x£¬retry:0x%x.\r\n",ic_checkcode,retry);

	if((ic_checkcode&0xffff0000)!=0xCACA0000){

		printk("linc cst3xx [cst3xx]  ic_checkcode read error .\r\n");
		goto err_return;
	}

	buf1[0] = 0xD1;
	buf1[1] = 0x09;
	ret = cst3xx_i2c_write(g_i2c_client, buf1, 2);
	if(ret < 0)
	{
		num_read_chars = snprintf(buf, 50, "hyn_tpfwver_show:write normal mode fail.\n");
		printk("%s : ret = %d. hyn_tpfwver_show:write normal mode fail.\n",__func__,ret);
		goto err_return;
	}

	num_read_chars = snprintf(buf, 128, "firmware_version: 0x%02X,module_version:0x%02X,project_version:0x%02X,chip_type:0x%02X,checksum:0x%02X .\n",firmware_version,module_version, project_version,chip_type,checksum);
	
	printk("%s ---> TP info : %s.\n",__func__,buf);

#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue!=NULL)
	cst3xx_esd_switch(SWITCH_ESD_ON);
#endif

	mutex_unlock(&g_device_mutex);
	printk("%s : num_read_chars = %ld.\n",__func__,num_read_chars);
	return num_read_chars;

err_return:
	if(retry<5){
		retry++;
		goto START;
	}else{
		printk("%s : num_read_chars = %ld.\n",__func__,num_read_chars);
		buf1[0] = 0xD1;
		buf1[1] = 0x09;
		ret = cst3xx_i2c_write(g_i2c_client, buf1, 2);

#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue!=NULL)
	cst3xx_esd_switch(SWITCH_ESD_ON);
#endif
		mutex_unlock(&g_device_mutex);
		return -1;
	}

}

static ssize_t hyn_tpfwver_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	/*place holder for future use*/
	return -EPERM;
}

static ssize_t hyn_tprwreg_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	ssize_t num_read_chars = 0;
	u8 buf1[20];
	u8 retry;
	int ret=-1;
	u16 value_0,value_1,value_2,value_3,value_4;

	mutex_lock(&g_device_mutex);
	memset((u8 *)buf1, 0, 20);

#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue != NULL)
	cst3xx_esd_switch(SWITCH_ESD_OFF);
#endif

	retry=0;
	value_0=0;
	value_1=0;
	value_2=0;
	value_3=0;
	value_4=0;

START:
	mdelay(5);
	buf1[0] = 0xD1;
	buf1[1] = 0x01;
	ret = cst3xx_i2c_write(g_i2c_client, buf1, 2);
	if (ret < 0)
	{
	    num_read_chars = snprintf(buf, 50, "hyn_tprwreg_show:write debug info command fail.\n");
		printk("%s : ret = %d. hyn_tpfwver_show:write debug info command fail.\n",__func__,ret);
		goto err_return;
	}

	mdelay(5);

	buf1[0] = 0xD2;
	buf1[1] = 0x20;
	ret = cst3xx_i2c_read_register(g_i2c_client, buf1, 10);
	if (ret < 0)
	{
		num_read_chars = snprintf(buf, 50, "hyn_tprwreg_show:Read version resgister fail.\n");
		printk("%s : ret = %d. hyn_tprwreg_show:Read version resgister fail.\n",__func__,ret);
		goto err_return;
	}
	value_0 = buf1[1];
	value_0 <<= 8;
	value_0 |= buf1[0];

	value_1 = buf1[3];
	value_1 <<= 8;
	value_1 |= buf1[2];
	
	value_2 = buf1[5];
	value_2 <<= 8;
	value_2 |= buf1[4];
	
	value_3 = buf1[7];
	value_3 <<= 8;
	value_3 |= buf1[6];
	
	value_4 = buf1[9];
	value_4 <<= 8;
	value_4 |= buf1[8];
	

	num_read_chars = snprintf(buf, 240, "value_0: 0x%02X,value_1:0x%02X,value_2:0x%02X,value_3:0x%02X,value_4:0x%02X.\n",
	value_0,value_1, value_2,value_3,value_4);

	buf1[0] = 0xD1;
	buf1[1] = 0x09;
	ret = cst3xx_i2c_write(g_i2c_client, buf1, 2);
	if(ret < 0)
	{
		num_read_chars = snprintf(buf, 50, "hyn_tprwreg_show:write normal mode fail.\n");
		printk("%s : ret = %d. hyn_tprwreg_show:write normal mode fail.\n",__func__,ret);
		goto err_return;
	}


#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue!=NULL)
	cst3xx_esd_switch(SWITCH_ESD_ON);
#endif

	mutex_unlock(&g_device_mutex);
	printk("%s : num_read_chars = %ld.\n",__func__,num_read_chars);
	return num_read_chars;

	err_return:
	if(retry<5){
		retry++;
		goto START;
	}else{
		printk("%s : num_read_chars = %ld.\n",__func__,num_read_chars);
		buf1[0] = 0xD1;
		buf1[1] = 0x09;
		ret = cst3xx_i2c_write(g_i2c_client, buf1, 2);

#ifdef CONFIG_TP_ESD_PROTECT
	if(cst3xx_esd_workqueue!=NULL)
	cst3xx_esd_switch(SWITCH_ESD_ON);
#endif
		mutex_unlock(&g_device_mutex);
		return -1;
	}

}

static ssize_t hyn_tprwreg_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	//struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	ssize_t num_read_chars = 0;
	int retval;
	long unsigned int wmreg = 0;
	u16 regaddr = 0xff;
	u8 valbuf[10] = {0};

	memset(valbuf, 0, sizeof(valbuf));
	mutex_lock(&g_device_mutex);
	num_read_chars = count - 1;

	if (num_read_chars != 2) {
		if (num_read_chars != 4) {
			printk("please input 2 or 4 character\n");
			goto error_return;
		}
	}

	memcpy(valbuf, buf, num_read_chars);
	retval = kstrtoul(valbuf, 16, &wmreg);

	if (0 != retval) {
		printk("%s() - ERROR: The given input was: \"%s\"\n",__func__, buf);
		goto error_return;
	}

	if (2 == num_read_chars) {
		/*read register*/
		regaddr = valbuf[0]<<8;
		regaddr |= valbuf[1];

		if(regaddr==0x3838){   //88-ascll
			//disable_irq(touch_irq);
			disable_irq(touch_irq); 
		}else if(regaddr==0x3939){//99-ascll
			//enable_irq(touch_irq);
			enable_irq(touch_irq);
		}else if(regaddr==0x3737){
			cst3xx_reset_ic(10);
		}

		if (cst3xx_i2c_read_register(g_i2c_client, valbuf,num_read_chars) < 0)
			printk("Could not read the register(0x%02x).\n",regaddr);
		else
			printk("the register(0x%02x) is 0x%02x\n",regaddr,valbuf[0] );
	} else {
		regaddr = valbuf[0]<<8;
		regaddr |= valbuf[1];
		if (cst3xx_i2c_read_register(g_i2c_client, valbuf, num_read_chars) < 0)
			printk("Could not write the register(0x%02x)\n",regaddr);
		else
			printk("Write 0x%02x into register(0x%02x) successful\n",regaddr, valbuf[0]);
	}

error_return:
	mutex_unlock(&g_device_mutex);

	return count;

}

static ssize_t hyn_fwupdate_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	/* place holder for future use */
	return -EPERM;
}

/*upgrade from *.i*/
static ssize_t hyn_fwupdate_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	//struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	printk("hyn_fwupdate_store enter.\n");
	mutex_lock(&g_device_mutex);
#ifdef HYN_UPDATE_FIRMWARE_ONLINE
	hyn_boot_update_fw(g_i2c_client);
#endif
	mutex_unlock(&g_device_mutex);
	return count;
}

static ssize_t hyn_fwupgradeapp_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	/*place holder for future use*/
	return -EPERM;
}


/*upgrade from app.bin*/
static ssize_t hyn_fwupgradeapp_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
	char fwname[256];
	int ret;
	unsigned char *pdata = NULL;
	int length = 24*1024;
	//struct i2c_client *client = container_of(dev, struct i2c_client, dev);


	printk("hyn_fwupgradeapp_store enter.\n");

	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "/mnt/%s", buf);
	fwname[count-1+8] = '\0';

	printk("fwname:%s.\n",fwname);
	pdata = kzalloc(sizeof(char)*length, GFP_KERNEL);
    if(pdata == NULL)
	{
        printk("hyn_fwupgradeapp_store GFP_KERNEL memory fail.\n");
        return -ENOMEM;
    }

	mutex_lock(&g_device_mutex);
	ret = cst3xx_read_fw_file(fwname, pdata, &length);
	if(ret < 0)
	{
		printk("cst2xx_read_fw_file fail.\n");
		if(pdata != NULL)
		{
			kfree(pdata);
			pdata = NULL;
		}
	}else{

		ret = cst3xx_apk_fw_dowmload(g_i2c_client, pdata, length);
		if(ret < 0)
		{
	        printk("cst2xx_apk_fw_dowmload failed.\n");
			if(pdata != NULL)
			{
				kfree(pdata);
				pdata = NULL;
			}
		}
	}

	mutex_unlock(&g_device_mutex);

	printk("hyn_fwupgradeapp_store exit.\n");

	return count;
}

/*sysfs */
/*get the fw version
*example:cat hyntpfwver
*/
static DEVICE_ATTR(hyntpfwver, S_IRUGO | S_IWUSR, hyn_tpfwver_show,
			hyn_tpfwver_store);

/*upgrade from *.i
*example: echo 1 > hynfwupdate
*/
static DEVICE_ATTR(hynfwupdate, S_IRUGO | S_IWUSR, hyn_fwupdate_show,
			hyn_fwupdate_store);

/*read and write register
*read example: echo 88 > hyntprwreg ---read register 0x88
*write example:echo 8807 > hyntprwreg ---write 0x07 into register 0x88
*
*note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(hyntprwreg, S_IRUGO | S_IWUSR, hyn_tprwreg_show,
			hyn_tprwreg_store);

/*upgrade from app.bin
*example:echo "*_app.bin" > hynfwupgradeapp
*/
static DEVICE_ATTR(hynfwupgradeapp, S_IRUGO | S_IWUSR, hyn_fwupgradeapp_show,
			hyn_fwupgradeapp_store);

/*add your attr in here*/
static struct attribute *hyn_attributes[] = {
	&dev_attr_hyntpfwver.attr,
	&dev_attr_hynfwupdate.attr,
	&dev_attr_hyntprwreg.attr,
	&dev_attr_hynfwupgradeapp.attr,
	NULL
};

static struct attribute_group hyn_attribute_group = {
	.attrs = hyn_attributes
};
/*create sysfs for debug*/

int hyn_create_sysfs(struct i2c_client *client)
{
	int err;
	g_i2c_client=client;
	if ((k_obj = kobject_create_and_add("hynitron_debug", NULL)) == NULL ) {
		printk("hynitron_debug sys node create error.\n");
    }
	err = sysfs_create_group(k_obj, &hyn_attribute_group);
	if (0 != err) {
		printk("%s() - ERROR: sysfs_create_group() failed.\n",__func__);
		sysfs_remove_group(k_obj, &hyn_attribute_group);
		return -EIO;
	} else {
		mutex_init(&g_device_mutex);
		printk("cst3xx:%s() - sysfs_create_group() succeeded.\n",__func__);
	}
	return err;
}

void hyn_release_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(k_obj, &hyn_attribute_group);
	mutex_destroy(&g_device_mutex);
}

#endif
#endif

#ifdef CONFIG_TP_ESD_PROTECT

static int esd_work_cycle = 200;
static struct delayed_work esd_check_work;
static int esd_running;
struct mutex esd_lock;
static void cst3xx_esd_check_func(struct work_struct *);


void cst3xx_init_esd_protect(void)
{
    esd_work_cycle = 2 * HZ;	/*HZ: clock ticks in 1 second generated by system*/
	PRINTK(" linc Clock ticks for an esd cycle: %d", esd_work_cycle);
	INIT_DELAYED_WORK(&esd_check_work, cst3xx_esd_check_func);
	mutex_init(&esd_lock);


}
	
void cst3xx_esd_switch(s32 on);
{
	mutex_lock(&esd_lock);
	if (SWITCH_ESD_ON == on) {	/* switch on esd check */
		if (!esd_running) {
			esd_running = 1;
			PRINTK(" linc Esd protector started!");
			queue_delayed_work(cst3xx_esd_workqueue, &esd_check_work, esd_work_cycle);
		}
	} else {		/* switch off esd check */
		if (esd_running) {
			esd_running = 0;
			PRINTK(" linc Esd protector stopped!");
			cancel_delayed_work(&esd_check_work);
		}
	}
	mutex_unlock(&esd_lock);

}


static void cst3xx_esd_check_func(struct work_struct *work)
{
	
    int retry = 0;
	int ret;
	unsigned char buf[4];

	if (!esd_running) {
	PRINTK(" linc Esd protector suspended!");
	return;
	}

	buf[0] = 0xD0;
	buf[1] = 0x4C;
	
	while(retry++ < 3) {
		ret = cst3xx_i2c_read_register(g_i2c_client, buf, 1);
		if (ret > 0) break;
		
		mdelay(2);		
	}

    if((retry>3) || ((buf[0]!=226)&&(buf[0]!=237)&&(buf[0]!=240))) {
		
		//cst2xx_chip_init();
		
		//cst2xx_check_code(cst2xx_i2c);

		cst3xx_reset_ic(10);
    }
	
	mutex_lock(&esd_lock);
	if (esd_running)
		queue_delayed_work(cst3xx_esd_workqueue, &esd_check_work, esd_work_cycle);
	else
		PRINTK(" linc Esd protector suspended!");
	mutex_unlock(&esd_lock);
}

#endif


#ifdef CST2XX_SUPPORT

#define CST2XX_BASE_ADDR		(24 * 1024)

#if 0
static int cst2xx_checksum_judge( unsigned char *pdata)
{
	int ret;
	int i;
	unsigned char buf[4];
	unsigned char *pBuf;
	unsigned int fw_checksum;
    //check_sum
	for(i=0; i<5; i++)
	{

		buf[0] = 0xD0;
		buf[1] = 0x4C;
		buf[2] = 0x55;
		ret = cst3xx_i2c_write(g_i2c_client, buf, 3);
		if (ret < 0)
		{
			mdelay(1);
			continue;
		}

		mdelay(80);

		buf[0] = 0xD0;
		buf[1] = 0x08;
		ret = cst3xx_i2c_read_register(g_i2c_client, buf, 4);
		if(ret < 0)
		{
			mdelay(1);
			continue;
		}

		g_cst3xx_ic_checksum = buf[3];
		g_cst3xx_ic_checksum <<= 8;
		g_cst3xx_ic_checksum |= buf[2];
		g_cst3xx_ic_checksum <<= 8;
		g_cst3xx_ic_checksum |= buf[1];
		g_cst3xx_ic_checksum <<= 8;
		g_cst3xx_ic_checksum |= buf[0];	

	}

	

	pBuf = &pdata[6*1024+24-8];
	
	fw_checksum = pBuf[3];
	fw_checksum <<= 8;
	fw_checksum |= pBuf[2];
	fw_checksum <<= 8;
	fw_checksum |= pBuf[1];
	fw_checksum <<= 8;
	fw_checksum |= pBuf[0];	


   PRINTK("linc cst3xx[cst3xx]the updating firmware:fw_checksum:0x%x,the chip checksum:0x%x .\n",
		 fw_checksum,g_cst3xx_ic_checksum);
	if(g_cst3xx_ic_checksum!=fw_checksum)
	{
        return -1;	
	}

	return 0;

}

#endif


static int cst2xx_enter_download_mode(void)
{
	int ret;
	int i;
	unsigned char buf[4];

	cst3xx_reset_ic(5);

	for(i=0; i<40; i++)
	{		
		buf[0] = 0xD1;
		buf[1] = 0x11;
		ret = cst3xx_i2c_write(g_i2c_client, buf, 2);
		if (ret < 0)
		{
			mdelay(1);
			continue;
		}
	
		mdelay(1); //wait enter download mode
		
		buf[0] = 0xD0;
		buf[1] = 0x01;
		ret = cst3xx_i2c_read_register(g_i2c_client, buf, 1);
		if(ret < 0)
		{
			mdelay(1);
			continue;
		}

		if (buf[0] == 0x55)
			break;
	}

	if(buf[0] != 0x55)
	{
		PRINTK(" linc [HYN]reciev 0x55 failed.\r\n");
		return -1;
	}

	cancel_delayed_work(&cst3xx_reset_chip_work);

	buf[0] = 0xD1;
	buf[1] = 0x10;   //enter writer register mode
	ret = cst3xx_i2c_write(g_i2c_client, buf, 2);
	if (ret < 0)
	{
		PRINTK(" linc [HYN]Send cmd 0xD110 failed. \r\n");
		return -1;
	}

	return 0;
}

static int cst2xx_download_program(unsigned char *data, int len)
{	
	int ret;
	int i, j;
	unsigned int wr_addr;
	unsigned char *pData;
	unsigned char *pSrc;
	unsigned char *pDst;
	unsigned char  i2c_buf[8];

	pData = kmalloc(sizeof(unsigned char)*(1024 + 4), GFP_KERNEL);
	if(NULL == pData)
	{
		PRINTK(" linc malloc data buffer failed.\n");
		return -1;
	}
	
	pSrc = data;
	PRINTK(" linc write program data begain:0x%x.\n", len);
	
	for(i=0; i<(len/1024); i++)
	{
		wr_addr  = (i<<10) + CST2XX_BASE_ADDR;
		
		pData[0] = (wr_addr >> 24) & 0xFF;
		pData[1] = (wr_addr >> 16) & 0xFF;
		pData[2] = (wr_addr >> 8) & 0xFF;
		pData[3] =  wr_addr & 0xFF;

		pDst = pData + 4;
		
		for(j=0; j<256; j++)
		{
			*pDst       = *(pSrc + 3);
			*(pDst + 1) = *(pSrc + 2);
			*(pDst + 2) = *(pSrc + 1);
			*(pDst + 3) = *pSrc;
			
			pDst += 4;
			pSrc += 4;
		}

		#ifdef TRANSACTION_LENGTH_LIMITED
		for(j=0; j<256; j++)
		{
            i2c_buf[0] = (wr_addr >> 24) & 0xFF;
    		i2c_buf[1] = (wr_addr >> 16) & 0xFF;
    		i2c_buf[2] = (wr_addr >> 8) & 0xFF;
    		i2c_buf[3] =  wr_addr & 0xFF;

            i2c_buf[4] =  pData[j*4+4+0];
    		i2c_buf[5] =  pData[j*4+4+1];
    		i2c_buf[6] =  pData[j*4+4+2];
    		i2c_buf[7] =  pData[j*4+4+3];    
    		
    		ret = cst3xx_i2c_write(g_i2c_client, i2c_buf, 8);
    		if(ret < 0)
    		{
    			PRINTK(" linc program failed.\n");
    			goto ERR_OUT;
    		}

			wr_addr += 4;
		}
		#else
		ret = cst3xx_i2c_write(g_i2c_client, pData, 1024+4);
		if (ret < 0)
		{
			PRINTK(" linc program failed.\n");
			goto ERR_OUT;
		}
		
		#endif

		mdelay(200);
	}

	mdelay(10);


    //clear update key
	pData[3] = 0x20000FF8 & 0xFF;
	pData[2] = (0x20000FF8>>8)  & 0xFF;
	pData[1] = (0x20000FF8>>16) & 0xFF;	
	pData[0] = (0x20000FF8>>24) & 0xFF;
	pData[4] = 0x00;
	pData[5] = 0x00;
	pData[6] = 0x00;
	pData[7] = 0x00;	
	ret = cst3xx_i2c_write(g_i2c_client, pData, 8);
	if (ret < 0)
	{
		PRINTK(" linc clear update key failed.\n");
		goto ERR_OUT;
	}
	
	pData[3] = 0xD013D013 & 0xFF;
	pData[2] = (0xD013D013>>8)  & 0xFF;
	pData[1] = (0xD013D013>>16) & 0xFF;	
	pData[0] = (0xD013D013>>24) & 0xFF;
	ret = cst3xx_i2c_write(g_i2c_client, pData, 4);
	if (ret < 0)
	{
		PRINTK(" linc exit register read/write mode failed.\n");
		goto ERR_OUT;
	}

	pData[0] = 0xD0;
	pData[1] = 0x00;
	ret = cst3xx_i2c_read_register(g_i2c_client, pData, 1);
	if (ret < 0)
	{
		PRINTK(" linc exit register read/write mode failed.\n");
		goto ERR_OUT;
	}
	if(pData[0]!=0x0A)
	{
        goto ERR_OUT;
	}
	
	PRINTK(" linc --------write program data end--------.\r\n");

	if (pData != NULL)
	{
		kfree(pData);
		pData = NULL;
	}	
	return 0;
	
ERR_OUT:
	if (pData != NULL)
	{
		kfree(pData);
		pData = NULL;
	}
	return -1;	
}

static int cst2xx_read_checksum(void)
{
	int ret;
	int i;
	unsigned int  checksum;
	unsigned int  bin_checksum;
	unsigned char buf[4];
	unsigned char *pData;

	for(i=0; i<10; i++)
	{
		buf[0] = 0xD0;
		buf[1] = 0x00;
		ret = cst3xx_i2c_read_register(g_i2c_client, buf, 1);
		if(ret < 0)
		{
			mdelay(2);
			continue;
		}

		if((buf[0]==0x01) || (buf[0]==0x02))
			break;

		mdelay(2);
	}

	if((buf[0]==0x01) || (buf[0]==0x02))
	{
		buf[0] = 0xD0;
		buf[1] = 0x08;
		ret = cst3xx_i2c_read_register(g_i2c_client, buf, 4);
		
		if(ret < 0)	return -1;
		
		//handle read data  --> checksum
		checksum = buf[0] + (buf[1]<<8) + (buf[2]<<16) + (buf[3]<<24);

        pData = pcst3xx_update_firmware + 6160; //6*1024+16
		bin_checksum = pData[0] + (pData[1]<<8) + (pData[2]<<16) + (pData[3]<<24);

		if(checksum!=bin_checksum)
		{
			PRINTK(" linc Check sum error.\n");
		}

        PRINTK(" linc checksum ic:0x%x. bin:0x%x-----\n", checksum, bin_checksum);
		
		buf[0] = 0xD0;
		buf[1] = 0x01;
		buf[2] = 0xA5;
		ret = cst3xx_i2c_write(g_i2c_client, buf, 3);
		
		if(ret < 0) return -1;
	}
	else
	{
		PRINTK(" linc No checksum.\n");
		return -1;
	}
	
	return 0;
}

static int cst2xx_update_firmware(struct i2c_client * client,
	unsigned char *pdata)
{
	int ret;
	int retry;
    int data_len=6*1024;
	
	retry = 0;
	
start_flow:

	PRINTK(" linc enter the update firmware.\n");

	ret = cst2xx_enter_download_mode();
	if (ret < 0)
	{
		PRINTK(" linc enter download mode failed.\n");
		goto fail_retry;
	}
	
	ret = cst2xx_download_program(pdata, data_len);
	if (ret < 0)
	{
		PRINTK(" linc download program failed.\n");
		goto fail_retry;
	}

	mdelay(3);
		
	ret = cst2xx_read_checksum();
	if (ret < 0)
	{
		PRINTK(" linc checksum failed.\n");
		goto fail_retry;
	}

	cst3xx_reset_ic(100);

	PRINTK(" linc Download firmware succesfully.\n");

	return 0;
	
fail_retry:
	if (retry < 10)
	{
		retry++;
		goto start_flow;
	}
	
	return -1;
}

#endif






/*******************************************************
Function:
    read checksum in bootloader mode
Input:
    client: i2c client
    strict: check checksum value
Output:
    success: 0
    fail:	-1
*******************************************************/

#define CST3XX_BIN_SIZE    (24*1024 + 24)

static int cst3xx_check_checksum(struct i2c_client * client)
{
	int ret;
	int i;
	unsigned int  checksum;
	unsigned int  bin_checksum;
	unsigned char buf[4];
	const unsigned char *pData;

	for(i=0; i<5; i++)
	{
		buf[0] = 0xA0;
		buf[1] = 0x00;
		ret = cst3xx_i2c_read_register(client, buf, 1);
		if(ret < 0)
		{
			mdelay(2);
			continue;
		}

		if(buf[0]!=0)
			break;
		else
		mdelay(2);
	}
    mdelay(2);


    if(buf[0]==0x01)
	{
		buf[0] = 0xA0;
		buf[1] = 0x08;
		ret = cst3xx_i2c_read_register(client, buf, 4);
		
		if(ret < 0)	return -1;
		
		// read chip checksum
		checksum = buf[0] + (buf[1]<<8) + (buf[2]<<16) + (buf[3]<<24);

        pData=(unsigned char  *)pcst3xx_update_firmware +24*1024+16;   //7*1024 +512
		bin_checksum = pData[0] + (pData[1]<<8) + (pData[2]<<16) + (pData[3]<<24);

        PRINTK(" linc hyn the updated ic checksum is :0x%x. the updating firmware checksum is:0x%x------\n", checksum, bin_checksum);
    
        if(checksum!=bin_checksum)
		{
			PRINTK("linc cst3xx hyn check sum error.\n");		
			return -1;
			
		}
		
	}
	else
	{
		PRINTK("linc cst3xx hyn No checksum.\n");
		return -1;
	}	
	return 0;
}
static int cst3xx_into_program_mode(struct i2c_client * client)
{
	int ret;
	unsigned char buf[4];
	
	buf[0] = 0xA0;
	buf[1] = 0x01;	
	buf[2] = 0xAA;	//set cmd to enter program mode		
	ret = cst3xx_i2c_write(client, buf, 3);
	if (ret < 0)  return -1;

	mdelay(2);
	
	buf[0] = 0xA0;
	buf[1] = 0x02;	//check whether into program mode
	ret = cst3xx_i2c_read_register(client, buf, 1);
	if (ret < 0)  return -1;
	
	if (buf[0] != 0x55) return -1;
	
	return 0;
}

static int cst3xx_exit_program_mode(struct i2c_client * client)
{
	int ret;
	unsigned char buf[3];
	
	buf[0] = 0xA0;
	buf[1] = 0x06;
	buf[2] = 0xEE;
	ret = cst3xx_i2c_write(client, buf, 3);
	if (ret < 0)
		return -1;
	
	mdelay(10);	//wait for restart

	
	return 0;
}

static int cst3xx_erase_program_area(struct i2c_client * client)
{
	int ret;
	unsigned char buf[3];
	
	buf[0] = 0xA0;
	buf[1] = 0x02;	
	buf[2] = 0x00;		//set cmd to erase main area		
	ret = cst3xx_i2c_write(client, buf, 3);
	if (ret < 0) return -1;
	
	mdelay(5);
	
	buf[0] = 0xA0;
	buf[1] = 0x03;
	ret = cst3xx_i2c_read_register(client, buf, 1);
	if (ret < 0)  return -1;
	
	if (buf[0] != 0x55) return -1;

	return 0;
}

static int cst3xx_write_program_data(struct i2c_client * client,
		const unsigned char *pdata)
{
	int i, ret;
	unsigned char *i2c_buf;
	unsigned short eep_addr;
	int total_kbyte;
#ifdef TRANSACTION_LENGTH_LIMITED
	unsigned char temp_buf[8];
	unsigned short iic_addr;
	int  j;

#endif
	

	i2c_buf = kmalloc(sizeof(unsigned char)*(1024 + 2), GFP_KERNEL);
	if (i2c_buf == NULL) 
		return -1;
	
	//make sure fwbin len is N*1K
	//total_kbyte = len / 1024;
	total_kbyte = 24;
	for (i=0; i<total_kbyte; i++) {
		i2c_buf[0] = 0xA0;
		i2c_buf[1] = 0x14;
		eep_addr = i << 10;		//i * 1024
		i2c_buf[2] = eep_addr;
		i2c_buf[3] = eep_addr>>8;
		ret = cst3xx_i2c_write(client, i2c_buf, 4);
		if (ret < 0)
			goto error_out;




	
#ifdef TRANSACTION_LENGTH_LIMITED
		memcpy(i2c_buf, pdata + eep_addr, 1024);
		for(j=0; j<256; j++) {
			iic_addr = (j<<2);
    	temp_buf[0] = (iic_addr+0xA018)>>8;
    	temp_buf[1] = (iic_addr+0xA018)&0xFF;
		temp_buf[2] = i2c_buf[iic_addr+0];
		temp_buf[3] = i2c_buf[iic_addr+1];
		temp_buf[4] = i2c_buf[iic_addr+2];
		temp_buf[5] = i2c_buf[iic_addr+3];
    	ret = cst3xx_i2c_write(client, temp_buf, 6);
    		if (ret < 0)
    			goto error_out;		
		}
#else
		
		i2c_buf[0] = 0xA0;
		i2c_buf[1] = 0x18;
		memcpy(i2c_buf + 2, pdata + eep_addr, 1024);
		ret = cst3xx_i2c_write(client, i2c_buf, 1026);
		if (ret < 0)
			goto error_out;
#endif
		
		i2c_buf[0] = 0xA0;
		i2c_buf[1] = 0x04;
		i2c_buf[2] = 0xEE;
		ret = cst3xx_i2c_write(client, i2c_buf, 3);
		if (ret < 0)
			goto error_out;
		
		mdelay(60);	
		
		i2c_buf[0] = 0xA0;
		i2c_buf[1] = 0x05;
		ret = cst3xx_i2c_read_register(client, i2c_buf, 1);
		if (ret < 0)
			goto error_out;
		
		if (i2c_buf[0] != 0x55)
			goto error_out;

	}
	
	i2c_buf[0] = 0xA0;
	i2c_buf[1] = 0x03;
	i2c_buf[2] = 0x00;
	ret = cst3xx_i2c_write(client, i2c_buf, 3);
	if (ret < 0)
		goto error_out;
	
	mdelay(8);	
	
	if (i2c_buf != NULL) {
		kfree(i2c_buf);
		i2c_buf = NULL;
	}

	return 0;
	
error_out:
	if (i2c_buf != NULL) {
		kfree(i2c_buf);
		i2c_buf = NULL;
	}
	return -1;
}

static int cst3xx_update_firmware(struct i2c_client * client, const unsigned char *pdata)
{
	int ret=-1;
	int retry = 0;
	int tmp_iic_addr=0;
	
	PRINTK("linc cst3xx----------upgrade cst3xx begain------------\n");
	
START_FLOW:	
	//g_update_flag = 1;
	
	//disable i2c irq	/////////////*********** \D0\E8要\D0薷牡\C4	
	
	cst3xx_reset_ic(10+retry);

	
	tmp_iic_addr =client->addr;
	client->addr = 0x1A;
	
	ret = cst3xx_into_program_mode(client);
	if (ret < 0) {
		PRINTK("linc cst3xx[cst3xx]into program mode failed.\n");
		goto err_out;
	}

	ret = cst3xx_erase_program_area(client);
	if (ret) {
		PRINTK("linc cst3xx[cst3xx]erase main area failed.\n");
		goto err_out;
	}

	ret = cst3xx_write_program_data(client, pdata);
	if (ret < 0) {
		PRINTK("linc cst3xx[cst3xx]write program data into cstxxx failed.\n");
		goto err_out;
	}

    ret =cst3xx_check_checksum(client);
	if (ret < 0) {
		PRINTK("linc cst3xx[cst3xx] after write program cst3xx_check_checksum failed.\n");
		goto err_out;
	}

	ret = cst3xx_exit_program_mode(client);
	if (ret < 0) {
		PRINTK("linc cst3xx[cst3xx]exit program mode failed.\n");
		goto err_out;
	}


	cst3xx_reset_ic(10);
	
	PRINTK("linc cst3xx hyn----------cst3xx_update_firmware  end------------\n");
	
	return 0;
	
err_out:
	if (retry < 5) {
		retry++;
		goto START_FLOW;
	} 
	else {		
		client->addr = tmp_iic_addr;
		return -1;
	}
}

static int cst3xx_update_judge( unsigned char *pdata, int strict)
{
	unsigned short ic_type, project_id;
	unsigned int fw_checksum, fw_version;
	const unsigned int *p;
	int i;
	unsigned char *pBuf;
		
	fw_checksum = 0x55;
	p = (const unsigned int *)pdata;
	for (i=0; i<(CST3XX_BIN_SIZE-4); i+=4) {
		fw_checksum += (*p);
		p++;
	}
	
	if (fw_checksum != (*p)) {
		PRINTK("linc cst3xx[cst3xx]calculated checksum error:0x%x not equal 0x%x.\n", fw_checksum, *p);
		return -1;	//bad fw, so do not update
	}
	
	pBuf = &pdata[CST3XX_BIN_SIZE-16];
	
	project_id = pBuf[1];
	project_id <<= 8;
	project_id |= pBuf[0];

	ic_type = pBuf[3];
	ic_type <<= 8;
	ic_type |= pBuf[2];

	fw_version = pBuf[7];
	fw_version <<= 8;
	fw_version |= pBuf[6];
	fw_version <<= 8;
	fw_version |= pBuf[5];
	fw_version <<= 8;
	fw_version |= pBuf[4];

	fw_checksum = pBuf[11];
	fw_checksum <<= 8;
	fw_checksum |= pBuf[10];
	fw_checksum <<= 8;
	fw_checksum |= pBuf[9];
	fw_checksum <<= 8;
	fw_checksum |= pBuf[8];	
	
	PRINTK("linc cst3xx[cst3xx]the updating firmware:project_id:0x%04x,ic type:0x%04x,version:0x%x,checksum:0x%x\n",
			project_id, ic_type, fw_version, fw_checksum);

#ifndef CONFIG_TOUCHSCREEN_MTK_CST3XX_FW_UPDATE
	PRINTK("[cst3xx] HYN_UPDATE_FIRMWARE_ENABLE is not open.\n");
    return -1;
#endif

	if (strict > 0) {
		
		if (g_cst3xx_ic_checksum != fw_checksum){
			if (g_cst3xx_ic_version >fw_version){
				PRINTK("[cst3xx]fw version(%d), ic version(%d).\n",fw_version, g_cst3xx_ic_version);
				return -1;
			}
		}else{
			PRINTK("[cst3xx]fw checksum(0x%x), ic checksum(0x%x).\n",fw_checksum, g_cst3xx_ic_checksum);
			return -1;
		}
	}	
	
	return 0;
}


/*******************************************************
Function:
    get firmware version, ic type...
Input:
    client: i2c client
Output:
    success: 0
    fail:	-1
*******************************************************/
static int cst3xx_firmware_info(struct i2c_client * client)
{
	int ret;
	unsigned char buf[28];
//	unsigned short ic_type, project_id;

	
	buf[0] = 0xD1;
	buf[1] = 0x01;
	ret = cst3xx_i2c_write(client, buf, 2);
	if (ret < 0) return -1;
	
	mdelay(10);

	buf[0] = 0xD2;
	buf[1] = 0x08;
	ret = cst3xx_i2c_read_register(client, buf, 8);
	if (ret < 0) return -1;	

	g_cst3xx_ic_version = buf[3];
	g_cst3xx_ic_version <<= 8;
	g_cst3xx_ic_version |= buf[2];
	g_cst3xx_ic_version <<= 8;
	g_cst3xx_ic_version |= buf[1];
	g_cst3xx_ic_version <<= 8;
	g_cst3xx_ic_version |= buf[0];

	g_cst3xx_ic_checksum = buf[7];
	g_cst3xx_ic_checksum <<= 8;
	g_cst3xx_ic_checksum |= buf[6];
	g_cst3xx_ic_checksum <<= 8;
	g_cst3xx_ic_checksum |= buf[5];
	g_cst3xx_ic_checksum <<= 8;
	g_cst3xx_ic_checksum |= buf[4];	

	tpd_info.vid = g_cst3xx_ic_version;
    tpd_info.pid = 0x00;


	PRINTK("linc cst3xx [cst3xx] the chip ic version:0x%x, checksum:0x%x\r\n",
		g_cst3xx_ic_version, g_cst3xx_ic_checksum);

	if(g_cst3xx_ic_version==0xA5A5A5A5)
	{
		PRINTK("linc cst3xx [cst3xx] the chip ic don't have firmware. \n");
		return -1;
	}

    buf[0] = 0xD1;
	buf[1] = 0x09;
	ret = cst3xx_i2c_write(client, buf, 2);
	if (ret < 0) return -1;
    mdelay(5);
	
	
	return 0;
}

static int cst3xx_boot_update_fw(struct i2c_client   *client,  unsigned char *pdata)
{
	int ret;
	int retry = 0;
	int flag = 0;

	while (retry++ < 3) {
		ret = cst3xx_firmware_info(client);
		if (ret == 0) {
			flag = 1;
			break;
		}
	}

	if (flag == 1) {
		ret = cst3xx_update_judge(pdata, 1);
		if (ret < 0) {
			PRINTK("linc cst3xx[cst3xx] no need to update firmware.\n");
			return 0;
		}
	}
	
	ret = cst3xx_update_firmware(client, pdata);
	if (ret < 0){
		PRINTK("linc cst3xx [cst3xx] update firmware failed.\n");
		return -1;
	}

    mdelay(50);

	ret = cst3xx_firmware_info(client);
	if (ret < 0) {
		PRINTK("linc cst3xx [cst3xx] after update read version and checksum fail.\n");
		return -1;
	}

	

	return 0;
}


static int hyn_boot_update_fw(struct i2c_client * client)
{
	unsigned char *ptr_fw;
	int ret=-1;
	ptr_fw = pcst3xx_update_firmware;
	
#ifdef CST2XX_SUPPORT
	if(hyn_ic_type==18868) {
	    ret = cst2xx_update_firmware(client, ptr_fw);
	    return ret;
	}
#endif	
	
	    ret = cst3xx_boot_update_fw(client, ptr_fw);
	    return ret;
	
}


static void cst3xx_touch_report(struct input_dev *input_dev)
{
	unsigned char buf[30];
	unsigned char i2c_buf[8];
	unsigned char key_status, key_id = 0, finger_id, sw;
	unsigned int  input_x = 0; 
	unsigned int  input_y = 0; 
	unsigned int  input_w = 0;
    unsigned char cnt_up, cnt_down;
	int   i, ret, idx; 
	int cnt, i2c_len;
#ifdef TRANSACTION_LENGTH_LIMITED
	int  len_1, len_2;
#endif

#ifdef TPD_PROXIMITY
	int err;
	struct hwm_sensor_data sensor_data;
	if (tpd_proximity_flag == 1) {
		buf[0] = 0xD0;
		buf[1] = 0x4B;
		err = cst3xx_i2c_read_register(g_i2c_client, buf, 1);
        if(err < 0) {
        	PRINTK(" linc iic read proximity data failed.\n");
        	goto OUT_PROCESS;
        }
		
		if(buf[0]&0x7F) {
			tpd_proximity_detect = 0;  //close
		}
		else {
			tpd_proximity_detect = 1;  //far away
		}
		
		PRINTK(" linc cst3xx ps change tpd_proximity_detect = %d  \n",tpd_proximity_detect);
		//map and store data to hwm_sensor_data
		sensor_data.values[0]    = tpd_get_ps_value();
		sensor_data.value_divide = 1;
		sensor_data.status       = SENSOR_STATUS_ACCURACY_MEDIUM;
		//let up layer to know
		if((err = hwmsen_get_interrupt_data(ID_PROXIMITY, &sensor_data))) {
			PRINTK(" linc call hwmsen_get_interrupt_data fail = %d\n", err);
		}
	}
#endif

#ifdef HYN_GESTURE
    if((hyn_ges_wakeup_switch == 1)&&(tpd_halt == 1)){
        int tmp_c;
		int err;
      // unsigned char hyn_gesture_tmp = 0;	
        buf[0] = 0xD0;
		buf[1] = 0x4C;
		err = cst3xx_i2c_read_register(g_i2c_client, buf, 1);
        if(err < 0)
        {
        	PRINTK("linc cst3xxiic read gesture flag failed.\n");
        	goto OUT_PROCESS;
        }
		tmp_c=buf[0]&0x7F;

		PRINTK(" linc [HYN_GESTURE] tmp_c =%d \n",tmp_c);
        if(1)
        {
           			
			if(0 == hyn_lcd_flag){
				
                #if 1

				switch(tmp_c){
	                case 0x20:    //double
						 hyn_key_report(KEY_I);
						 hyn_gesture_c = (char)'*';
						 break;
	                case 0x3:     //left
	                     hyn_key_report(KEY_F);
						 hyn_gesture_c = (char)0xa1fb;
						 break;
	                case 0x1:     //right
	                     hyn_key_report(KEY_R);
						 hyn_gesture_c = (char)0xa1fa;
						 break;
	                case 0x2:     //up
	                     hyn_key_report(KEY_K);
						 hyn_gesture_c = (char)0xa1fc;
						 break;
					case 0x4:     //down
						 hyn_key_report(KEY_L);
						 hyn_gesture_c = (char)0xa1fd;
						 break;
	                case 0x5:     //O
	                     hyn_key_report(KEY_O);
						 hyn_gesture_c = (char)'O';
						 break;
	                case 0x0A:    //W
	                     hyn_key_report(KEY_W);
						 hyn_gesture_c = (char)'W';
						 break;
	                case 0x8: //M
	                case 0x09:
					case 0x0f:
			  	    case 0x10:
					case 0x15:
						 hyn_key_report(KEY_M);
						 hyn_gesture_c = (char)'M';
						 break;
	                case 0x07:            //E
	                     hyn_key_report(KEY_E);
						 hyn_gesture_c = (char)'E';
						 break;
	                case 0x6:       
	                case 0x0e:      //C
	                     hyn_key_report(KEY_C); 
						 hyn_gesture_c = (char)'C';
						 break;
	                case 0x0C:            //S
	                     hyn_key_report(KEY_S);
						 hyn_gesture_c = (char)'S';
						 break;
	                case 0x0B:            //V
	                     hyn_key_report(KEY_V);
						 hyn_gesture_c = (char)'V';
						 break;
	                case 0x0D:            //Z
	                     hyn_key_report(KEY_Z);
						 hyn_gesture_c = (char)'Z';
				         break;
	                default:
	                     break;
					}
                #else
					switch(tmp_c){
	                case 0x20:    //double					
						 hyn_gesture_tmp = (char)'*';
						 break;
	                case 0x3:     //left	                     
						 hyn_gesture_tmp = 0xBB;
						 break;
	                case 0x1:     //right
						 hyn_gesture_tmp = 0xAA;
						 break;
	                case 0x2:     //up
						 hyn_gesture_tmp = 0xBA;
						 break;
		      		case 0x4:     //down
						 hyn_gesture_tmp =0xAB;
						 break;
	                case 0x5:     //O
	                case 0x14:
					case 0x16:
							
						 hyn_gesture_tmp = (char)'o';
						 break;
	                case 0x0A:    //W
						 hyn_gesture_tmp = (char)'w';
						 break;
	                case 0x8: //M
	                case 0x09:
					case 0x0f:
			  	    case 0x10:
					case 0x15:
						 hyn_gesture_tmp = (char)'m';
						 break;
	                case 0x07:            //E
	                case 0x11:
		      case 0x13:
						 hyn_gesture_tmp = 'e';
						 break;
	                case 0x6:       //C
	                case 0x0e:					
						 hyn_gesture_tmp = (char)'c';
						 break;
	                case 0x0C:            //S
	                case 0x12:	
						 hyn_gesture_tmp = (char)'s';
						 break;
	                case 0x0B:            //V
						 hyn_gesture_tmp = (char)'v';
						 break;
	                case 0x0D:            //Z
						 hyn_gesture_tmp = (char)'z';
				         break;
	                default:
	                     break;
						}
				
				
				PRINTK(" linc [GSL_GESTURE] input report KEY_POWER.hyn_gesture_c = %c ,%d\n",hyn_gesture_c,hyn_gesture_c);
				for(i = 0; i < hyn_ges_num; i++){
						if(hyn_gesture_tmp == hyn_ges_cmd[i]){
			 				hyn_gesture_c = hyn_gesture_tmp;
							input_report_key(hyn_power_idev,KEY_POWER,1);
							input_sync(hyn_power_idev);
							input_report_key(hyn_power_idev,KEY_POWER,0);
							input_sync(hyn_power_idev);    
							}
					}
                #endif
			//hyn_lcd_flag = 1;		

			}
            goto i2c_lock; 
        }
        
    }
#endif


    key_status = 0;


	buf[0] = 0xD0;
	buf[1] = 0x00;
	ret = cst3xx_i2c_read_register(g_i2c_client, buf, 7);
	if(ret < 0) {
		PRINTK(" linc iic read touch point data failed.\n");
		goto OUT_PROCESS;
	}
		
	if(buf[6] != 0xAB) {
		//PRINTK(" linc data is not valid..\r\n");
		goto OUT_PROCESS;
	}

	if(buf[5] == 0x80) {
		key_status = buf[0];
		key_id = buf[1];		
		goto KEY_PROCESS;
	} 
	
	cnt = buf[5] & 0x7F;
	if(cnt > TPD_MAX_FINGERS) goto OUT_PROCESS;
	else if(cnt==0)     goto CLR_POINT;
	
	if(cnt == 0x01) {
		goto FINGER_PROCESS;
	} 
	else {
		#ifdef TRANSACTION_LENGTH_LIMITED
		if((buf[5]&0x80) == 0x80) //key
		{
			i2c_len = (cnt - 1)*5 + 3;
			len_1   = i2c_len;
			for(idx=0; idx<i2c_len; idx+=6) {
			    i2c_buf[0] = 0xD0;
				i2c_buf[1] = 0x07+idx;
				
				if(len_1>=6) {
					len_2  = 6;
					len_1 -= 6;
				}
				else {
					len_2 = len_1;
					len_1 = 0;
				}
				
    			ret = cst3xx_i2c_read_register(g_i2c_client, i2c_buf, len_2);
    			if(ret < 0) goto OUT_PROCESS;

				for(i=0; i<len_2; i++) {
                   buf[5+idx+i] = i2c_buf[i];
				}
			}
			
			i2c_len   += 5;
			key_status = buf[i2c_len - 3];
			key_id     = buf[i2c_len - 2];
		} 
		else {			
			i2c_len = (cnt - 1)*5 + 1;
			len_1   = i2c_len;
			
			for(idx=0; idx<i2c_len; idx+=6) {
			    i2c_buf[0] = 0xD0;
				i2c_buf[1] = 0x07+idx;
				
				if(len_1>=6) {
					len_2  = 6;
					len_1 -= 6;
				}
				else {
					len_2 = len_1;
					len_1 = 0;
				}
				
    			ret = cst3xx_i2c_read_register(g_i2c_client, i2c_buf, len_2);
    			if (ret < 0) goto OUT_PROCESS;

				for(i=0; i<len_2; i++) {
                   buf[5+idx+i] = i2c_buf[i];
				}
			}			
			i2c_len += 5;
		}
		#else
		if ((buf[5]&0x80) == 0x80) {
			buf[5] = 0xD0;
			buf[6] = 0x07;
			i2c_len = (cnt - 1)*5 + 3;
			ret = cst3xx_i2c_read_register(g_i2c_client, &buf[5], i2c_len);
			if (ret < 0)
				goto OUT_PROCESS;
			i2c_len += 5;
			key_status = buf[i2c_len - 3];
			key_id = buf[i2c_len - 2];
		} 
		else {			
			buf[5] = 0xD0;
			buf[6] = 0x07;			
			i2c_len = (cnt - 1)*5 + 1;
			ret = cst3xx_i2c_read_register(g_i2c_client, &buf[5], i2c_len);
			if (ret < 0)
				goto OUT_PROCESS;
			i2c_len += 5;
		}
		#endif

		if (buf[i2c_len - 1] != 0xAB) {
			goto OUT_PROCESS;
		}
	}	

    //both key and point
	if((cnt>0)&&(key_status&0x80))  {
        if(report_flag==0xA5) goto KEY_PROCESS; 
	}
	
FINGER_PROCESS:

	i2c_buf[0] = 0xD0;
	i2c_buf[1] = 0x00;
	i2c_buf[2] = 0xAB;
	ret = cst3xx_i2c_write(g_i2c_client, i2c_buf, 3);
	if(ret < 0) {
		PRINTK("linc cst3xx hyn send read touch info ending failed.\r\n"); 
		cst3xx_reset_ic(20);
	}
	
	idx = 0;
    cnt_up = 0;
    cnt_down = 0;
	for (i = 0; i < cnt; i++) {
		
		input_x = (unsigned int)((buf[idx + 1] << 4) | ((buf[idx + 3] >> 4) & 0x0F));
		input_y = (unsigned int)((buf[idx + 2] << 4) | (buf[idx + 3] & 0x0F));	
		input_w = (unsigned int)(buf[idx + 4]);
		sw = (buf[idx] & 0x0F) >> 1;
		finger_id = (buf[idx] >> 4) & 0x0F;
		finger_id +=1;

        #if 0        	
		input_x = TPD_MAX_X - input_x;
		input_y = TPD_MAX_Y - input_y;

		#endif

	   
        //PRINTK("linc cst3xxPoint x:%d, y:%d, id:%d, sw:%d. \n", input_x, input_y, finger_id, sw);

		if (sw == 0x03) {
			cst3xx_touch_down(input_dev, finger_id, input_x, input_y, input_w);
            cnt_down++;
        }
		else {
            cnt_up++;
            #ifdef ICS_SLOT_REPORT
			cst3xx_touch_up(input_dev, finger_id);
            #endif
        }
		idx += 5;
	}
    
    #ifndef ICS_SLOT_REPORT
    if((cnt_up>0) && (cnt_down==0))
        cst3xx_touch_up(input_dev, 0);
    #endif

	if(cnt_down==0)  report_flag = 0;
	else report_flag = 0xCA;

    input_sync(input_dev);
	goto END;

KEY_PROCESS:

	i2c_buf[0] = 0xD0;
	i2c_buf[1] = 0x00;
	i2c_buf[2] = 0xAB;
	ret = cst3xx_i2c_write(g_i2c_client, i2c_buf, 3);
	if (ret < 0) {
		PRINTK("linc cst3xx hyn send read touch info ending failed.\r\n"); 
		cst3xx_reset_ic(20);
	}
	
    #ifdef GTP_HAVE_TOUCH_KEY
	if(key_status&0x80) {
		i = (key_id>>4)-1;
        if((key_status&0x7F)==0x03) {
			if((i==key_index)||(key_index==0xFF)) {
        		//cst3xx_touch_down(input_dev, 0, tpd_keys_dim_local[i][0], tpd_keys_dim_local[i][1], 50);
                input_report_key(input_dev, touch_key_array[i], 1);
    			report_flag = 0xA5;
				key_index   = i;
			}
			else {
                input_report_key(input_dev, touch_key_array[key_index], 0);
				key_index = 0xFF;
			}
		}
    	else {
			input_report_key(input_dev, touch_key_array[i], 0);
            cst3xx_touch_up(input_dev, 0);
			report_flag = 0;	
			key_index = 0xFF;
    	}
	}
	#endif	

    #ifdef TPD_HAVE_BUTTON
	if(key_status&0x80) {
		i = (key_id>>4)-1;
        if((key_status&0x7F)==0x03) {
			if((i==key_index)||(key_index==0xFF)) {
        		cst3xx_touch_down(input_dev, 0, tpd_keys_dim_local[i][0], tpd_keys_dim_local[i][1], 50);
    			report_flag = 0xA5;
				key_index   = i;
			}
			else {
				
				key_index = 0xFF;
			}
		}
    	else {
            cst3xx_touch_up(input_dev, 0);
			report_flag = 0;	
			key_index = 0xFF;
    	}
	}
    

	#endif	
	
	input_sync(input_dev);
    goto END;

CLR_POINT:
#ifdef SLEEP_CLEAR_POINT
	#ifdef ICS_SLOT_REPORT
		for(i=0; i<=10; i++) {	
			input_mt_slot(input_dev, i);
			input_report_abs(input_dev, ABS_MT_TRACKING_ID, -1);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
		}
	#else
	    input_report_key(input_dev, BTN_TOUCH, 0);
		input_mt_sync(input_dev);
	#endif
		input_sync(input_dev);	
#endif		
		
OUT_PROCESS:
	buf[0] = 0xD0;
	buf[1] = 0x00;
	buf[2] = 0xAB;
	ret = cst3xx_i2c_write(g_i2c_client, buf, 3);
	if (ret < 0) {
		PRINTK(" linc send read touch info ending failed.\n"); 
		cst3xx_reset_ic(20);
	}

#ifdef HYN_GESTURE	
i2c_lock:
     key_status = 0;
	;
#endif	
	
END:	
	return;
}
static int cst3xx_touch_handler(void *unused)
{
	struct sched_param param = { .sched_priority = 4 };

    //PRINTK(" linc ===cst3xx_touch_handler2222\n");
	
	sched_setscheduler(current, SCHED_RR, &param);

	do {
        //enable_irq(touch_irq);
		set_current_state(TASK_INTERRUPTIBLE);
		wait_event(waiter, tpd_flag!=0);
		tpd_flag = 0;
        //TPD_DEBUG_SET_TIME;
		set_current_state(TASK_RUNNING);

//        eint_flag = 0;
#ifdef ANDROID_TOOL_SURPORT
		if(hyn_tp_test_flag==1){
			printk("tpd irq interrupt to dbg_waiter\n");
			tpd_flag  = 1;
			wake_up(&dbg_waiter);
			return 0;
		}else
#endif
		{

			cst3xx_touch_report(tpd->dev);
		}


		
		
	} while (!kthread_should_stop());

	return 0;
}

static void cst3xx_ts_irq_handler(void) 
{
  //  PRINTK(" linc ===[hyn]tpd irq interrupt===\n");
	//eint_flag = 1;
	tpd_flag  = 1;
  
 
	wake_up(&waiter);
}

static int tpd_irq_registration(void)
{
	struct device_node *node = NULL;
	int ret = 0;
	u32 ints[2] = {0,0};

	tpd_gpio_as_int(GTP_INT_PORT);
    
	node = of_find_matching_node(node, touch_of_match);
	if (node) {
		/*touch_irq = gpio_to_irq(tpd_int_gpio_number);*/
		of_property_read_u32_array(node,"debounce", ints, ARRAY_SIZE(ints));
		gpio_set_debounce(ints[0], ints[1]);

		touch_irq = irq_of_parse_and_map(node, 0);
       	if(touch_irq ==0){
			PRINTK("linc cst3xx*** Unable to irq_of_parse_and_map() ***\n");
		}
    	else
    	{        
			ret = request_irq(touch_irq, (irq_handler_t)cst3xx_ts_irq_handler,IRQF_TRIGGER_RISING, "TOUCH_PANEL-eint_cst3xx", NULL);
			if (ret > 0){
				PRINTK("linc cst3xxtpd request_irq IRQ LINE NOT AVAILABLE!.\n");
			}
    	}
	} 
    else
	{
		PRINTK(" linc [%s] tpd request_irq can not find touch eint device node!.\n", __func__);
	}

	return 0;
}
//steven add start 20181024
#define PROC_TP_VERSION "driver/tp_version"
static char mtk_tp_version[128] = {0};

static int tp_info_version_read(struct seq_file *m, void *v)
{
    struct input_dev *input_dev =hyn_input_dev;
 //   struct i2c_client *client = hyn_i2c_client;

    mutex_lock(&input_dev->mutex);

    sprintf(mtk_tp_version,"0x%02x,0x%02x",g_cst3xx_ic_version,g_cst3xx_ic_checksum);
    seq_printf(m, "%s\n",mtk_tp_version);

     mutex_unlock(&input_dev->mutex);
	
   return 0;
};

static int proc_tp_version_open_cst3xx(struct inode *inode, struct file *file)
{
    return single_open(file, tp_info_version_read, NULL);
};

static  struct file_operations tp_version_proc_fops1 = {
    .owner = THIS_MODULE,
    .open  = proc_tp_version_open_cst3xx,
    .read  = seq_read,
};
////steven add end 20181024	 

static int cst3xx_tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{	
	int ret=-1;
	int rc=-1;
	//int retry = 0;
	//unsigned char buf[4];

	
#ifdef GTP_HAVE_TOUCH_KEY
    s32 idx = 0;
#endif
	
#ifdef TPD_PROXIMITY
	struct hwmsen_object obj_ps;
#endif

    PRINTK(" linc hyn is entering tpd_i2c_probe. \n");
	
	// if(client->addr != 0x1A)
	if(client->addr != CST3XX_I2C_ADDR)
	{
		// client->addr = 0x1A;
		client->addr = CST3XX_I2C_ADDR;
		PRINTK("Crystal_shen:i2c_client_HYN->addr=0x%x.\n",client->addr); 
	}
	
	tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
	ret = regulator_set_voltage(tpd->reg, 2800000, 2800000);	/*set 2.8v*/
	if (ret) {
		PRINTK("linc cst3xx hyn regulator_set_voltage(%d) failed!\n", ret);
		return -EINVAL;
	}
	
	g_i2c_client = client;

	tpd_gpio_output(GTP_RST_PORT, 0);
	mdelay(100);
	
	PRINTK("linc hyn Device Tree get regulator! \n");
	
    ret = regulator_enable(tpd->reg);
	if (ret != 0)
		PRINTK("linc cst3xx hyn Failed to enable reg-vgp2: %d\n", ret);
	
	mdelay(100);
	tpd_gpio_output(GTP_RST_PORT, 1);	
	tpd_gpio_as_int(GTP_INT_PORT);	
	mdelay(400);
	

	ret = cst3xx_i2c_test(client);
	if (ret < 0) {
		PRINTK("linc cst3xx hyn i2c communication failed.\n");

		rc = cst3xx_update_firmware(client,pcst3xx_update_firmware);
		if(rc < 0){
			PRINTK("linc cst3xx cst3xx_update_firmware failed.\n");
			return -EINVAL;
		}
	}
	else
	{
		mdelay(20);

		rc = hyn_boot_update_fw(client);
		if(rc < 0){
			printk("linc cst3xx hyn_boot_update_fw failed.\n");
			return -1;
		}

	}
	
	// rc = cst3xx_update_firmware(client,pcst3xx_update_firmware);
		// if(rc < 0){
			// PRINTK("linc cst3xx cst3xx_update_firmware failed.\n");
			// return -EINVAL;
		// }
		
#ifdef ANDROID_TOOL_SURPORT
	ret = cst3xx_proc_fs_init();
	if(ret < 0) {
		PRINTK("linc cst3xx hyn create cst3xx proc fs failed.\n");
		return -EINVAL;
	}
#ifdef HYN_SYSFS_NODE_EN
    hyn_create_sysfs(client);
#endif
#endif

	ret = tpd_irq_registration();
	if(ret < 0) {
		PRINTK("linc cst3xx tpd_irq_registration failed.\n");
		return -EINVAL;
	}
    tpd_load_status = 1;

#ifdef HYN_GESTURE
//	for(Hyn_key_set=0;Hyn_key_set<HYN_TPD_GES_COUNT;Hyn_key_set++)	
//	input_set_capability(tpd->dev, EV_KEY,  tpd_keys_gesture[Hyn_key_set]); 
#endif

 	disable_irq(touch_irq);

	thread = kthread_run(cst3xx_touch_handler, 0, TPD_DEVICE_NAME);
	if (IS_ERR(thread)) {
		ret = PTR_ERR(thread);
		PRINTK("linc cst3xx hyn create touch event handler thread failed: %d.\n", ret);
	}



    enable_irq(touch_irq);
		
#ifdef CONFIG_TP_ESD_PROTECT

	cst3xx_esd_workqueue = create_singlethread_workqueue("cst2xx_esd_workqueue");
	if (cst3xx_esd_workqueue == NULL)
		PRINTK("linc cst3xxcreate cst2xx_esd_workqueue failed!");

#endif


#ifdef GTP_HAVE_TOUCH_KEY
    for (idx=0; idx<TPD_KEY_COUNT; idx++) {
        input_set_capability(tpd->dev, EV_KEY, touch_key_array[idx]);
    }
#endif	

#ifdef TPD_PROXIMITY
{
	int err = 0;
	hwmsen_detach(ID_PROXIMITY);
	obj_ps.self    = NULL;
	obj_ps.polling = 0;//interrupt mode
	//obj_ps.polling = 1;//need to confirm what mode is!!!
	obj_ps.sensor_operate = tpd_ps_operate;
	if((err = hwmsen_attach(ID_PROXIMITY, &obj_ps))) {
		PRINTK("linc cst3xxID_PROXIMITY attach fail = %d\n", err);
	}		
	//gsl_gain_psensor_data(g_i2c_client);
	//wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");
}
#endif



#ifdef CONFIG_TP_ESD_PROTECT

	cst3xx_init_esd_protect();
	cst3xx_esd_switch(SWITCH_ESD_ON);

#endif

#ifdef HYN_GESTURE

#if 0

	retval = gesture_proc_init();
	if (retval)
	{
		PRINTK("linc cst3xxCreate gesture_proc failed!\n.");
	}

    hyn_gesture_init();

#endif


	input_set_capability(tpd->dev, EV_KEY, KEY_POWER);
	input_set_capability(tpd->dev, EV_KEY, KEY_C);
	input_set_capability(tpd->dev, EV_KEY, KEY_M);
	input_set_capability(tpd->dev, EV_KEY, KEY_E);
	input_set_capability(tpd->dev, EV_KEY, KEY_O);
	input_set_capability(tpd->dev, EV_KEY, KEY_W);
	input_set_capability(tpd->dev, EV_KEY, KEY_S);
	input_set_capability(tpd->dev, EV_KEY, KEY_UP);
	input_set_capability(tpd->dev, EV_KEY, KEY_LEFT);
	input_set_capability(tpd->dev, EV_KEY, KEY_RIGHT);
	input_set_capability(tpd->dev, EV_KEY, KEY_DOWN);
	input_set_capability(tpd->dev, EV_KEY, KEY_U);


	
#endif


 //steven add start 20181024
    memset(mtk_tp_version,0,128);
    proc_create(PROC_TP_VERSION, 0, NULL, &tp_version_proc_fops1);
	//steven add end 20181024	 

	hyn_input_dev=tpd->dev;
	
	printk(" linc hyn is endding tpd_i2c_probe .\n");
	
#if defined(CONFIG_PRIZE_HARDWARE_INFO)
	cst3xx_firmware_info(client);
	sprintf(current_tp_info.chip,"FW:0x%08x", g_cst3xx_ic_version);
    sprintf(current_tp_info.id,"0x%02x",client->addr);
    strcpy(current_tp_info.vendor,"hynitron");
    //sprintf(current_tp_info.more,"fw:0x%08x",g_cst3xx_ic_version);
#endif

	return 0;
}

static int cst3xx_tpd_remove(struct i2c_client *client)
{
	PRINTK(" linc cst3xx removed.\n");
#ifdef ANDROID_TOOL_SURPORT
#ifdef HYN_SYSFS_NODE_EN
	hyn_release_sysfs(client);
#endif
#endif

	return 0;
}

/*
static int cst3xx_tpd_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, TPD_DEVICE_NAME);
	return 0;
}
*/

static const struct i2c_device_id cst3xx_tpd_id[] = {{TPD_DEVICE_NAME,0},{}};



//static unsigned short force[] = { 0, CST3XX_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END };
//static const unsigned short *const forces[] = { force, NULL };


/*
	static struct i2c_board_info __initdata cst3xx_i2c_tpd = { 
		I2C_BOARD_INFO(TPD_DEVICE_NAME, CST3XX_I2C_ADDR)
	};
*/


static const struct of_device_id tpd_of_match[] = {
	{.compatible = "mediatek,cap_touch"},
	{.compatible = "mediatek,hyn_cst3xx"},
	{},
};

MODULE_DEVICE_TABLE(of, tpd_of_match);


static struct i2c_driver cst3xx_ts_driver = {

    .driver = {
    .name = TPD_DEVICE_NAME,
	.of_match_table = of_match_ptr(tpd_of_match),
  },

  .probe    = cst3xx_tpd_probe,
  .remove   = cst3xx_tpd_remove,
  .id_table = cst3xx_tpd_id,
//  .detect   = cst3xx_tpd_detect,
  //.address_list = (const unsigned short *)forces,
};
static int cst3xx_local_init(void)
{

//	int ret = 0;

	PRINTK(" linc hyn is entering cst3xx_local_init .\n");


	if (i2c_add_driver(&cst3xx_ts_driver) != 0) {
		PRINTK("linc cst3xx hyn unable to add i2c driver.\n");
		return -1;
	}

	if(tpd_load_status == 0)  {
		i2c_del_driver(&cst3xx_ts_driver);
		return -1;
	}
	else {
        #ifdef ICS_SLOT_REPORT
        clear_bit(BTN_TOUCH, tpd->dev->keybit);
		input_set_abs_params(tpd->dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_WIDTH_MINOR, 0, 15, 0, 0);
	    input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (TPD_MAX_FINGERS+1), 0, 0);
    	input_mt_init_slots(tpd->dev,TPD_MAX_FINGERS+1,0);
		#else 
		input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, (TPD_MAX_FINGERS+1), 0, 0);	
        #endif	
	}

	
#ifdef TPD_HAVE_BUTTON
    tpd_button_setting(TPD_KEY_COUNT, tpd_keys_local, tpd_keys_dim_local);// initialize tpd button data
#endif

	if (tpd_dts_data.use_tpd_button) {
		/*initialize tpd button data*/
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
		tpd_dts_data.tpd_key_dim_local);
	}

	/*set vendor string*/
	tpd->dev->id.vendor = 0x00;
	tpd->dev->id.product = tpd_info.pid;
	tpd->dev->id.version = tpd_info.vid;

	tpd_type_cap = 1;
	
	PRINTK(" linc hyn is end %s, %d\n", __FUNCTION__, __LINE__);
	
	return 0;
}
/*
static void cst2xx_checkcode(struct i2c_client *client)
{
	int ret=-1;
	int retry = 0;
	unsigned int  chip_type=0;
	unsigned char buf[4];

    buf[0] = 0xD0;
	buf[1] = 0x49;
	while (retry++ < 5) {
		ret = cst3xx_i2c_read_register(client, buf, 2);
		if (ret < 0)
			return;
		mdelay(2);
	}

	chip_type=(buf[0]<<8)+buf[1];
	PRINTK(" linc cst2xx_checkcode :chip_type:%d.\n",chip_type);
	if(chip_type==18868)
	{
		PRINTK(" linc cst2xx_checkcode :chip_type:18868.\n");
		return;
	}
	else
	{
		
		hyn_boot_update_fw(g_i2c_client);
		PRINTK(" linc cst2xx_checkcode :NULL.\n");	
	}
	
	return;
}
*/
static void cst3xx_enter_sleep(struct i2c_client *client)
{
	int ret;
	int retry = 0;
	unsigned char buf[2];

    buf[0] = 0xD1;
	buf[1] = 0x05;
	while (retry++ < 5) {
		ret = cst3xx_i2c_write(client, buf, 2);
		if (ret > 0)
			return;
		mdelay(2);
	}
	
	return;
}



static void cst3xx_resume(struct device *h)
{	

	//int ret;
	//int retry = 0;
	//unsigned char buf[4];


	
 #ifdef ICS_SLOT_REPORT
	int idx;
#endif	
	PRINTK(" linc cst3xx wake up.\n");	

#ifdef TPD_PROXIMITY
		if (tpd_proximity_flag == 1) {
		//	tpd_enable_ps(1);
		return ;
		}
	
#endif

#ifdef SLEEP_CLEAR_POINT
#ifdef ICS_SLOT_REPORT
	for(idx=0; idx<=10; idx++) {	
		input_mt_slot(tpd->dev, idx);
		input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);
		input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
	}
#else
    input_report_key(tpd->dev, BTN_TOUCH, 0);
	input_mt_sync(tpd->dev);
#endif
	input_sync(tpd->dev);	
#endif	


#ifdef HYN_GESTURE

    if(hyn_ges_wakeup_switch == 1){
        u8 buf[4];
        //close gesture detect
        buf[0] = 0xD0;
		buf[1] = 0x4C;
		buf[2] = 0x00;
		cst3xx_i2c_write(g_i2c_client, buf, 3);
        tpd_halt=0;
		hyn_lcd_flag = 1;	
		
		PRINTK(" linc tpd-gesture detect is closed .\n");
       	
    }
	else
#endif
	
	{

		enable_irq(touch_irq);


	}	

#ifdef CST2XX_SUPPORT
	if(hyn_ic_type==18868)	
	{
		//cst2xx_checkcode(g_i2c_client);	
	}
#endif

//    if(hyn_ic_type==340)
//	hyn_boot_update_fw(g_i2c_client);


#ifdef CONFIG_TP_ESD_PROTECT

	cst3xx_esd_switch(SWITCH_ESD_ON);

#endif
	
#if 1
	if (regulator_disable(tpd->reg)){
		PRINTK("regulator_disable() failed!");
	}
	mdelay(50);
	// set 2.8v
    if (regulator_set_voltage(tpd->reg, 2800000, 2800000))   
		PRINTK("regulator_set_voltage() failed!");
    //enable regulator
     if (regulator_enable(tpd->reg))
		PRINTK("regulator_enable() failed!");
#endif		
	cst3xx_reset_ic(30);

	mdelay(200);
	
	PRINTK(" linc cst3xx wake up done.\n");
}


static void cst3xx_suspend(struct device *h)
{ 

 #ifdef ICS_SLOT_REPORT
	int idx;
#endif
 	
	
	PRINTK(" linc cst3xx enter sleep.\n");

#ifdef TPD_PROXIMITY
	if (tpd_proximity_flag == 1) {
		return ;
	}
#endif

#ifdef HYN_GESTURE
    if(hyn_ges_wakeup_switch == 1){  
        
//		int err;
		u8 buf[4];
        hyn_lcd_flag = 0;
        mdelay(10);			
		tpd_halt=1;
    
        buf[0] = 0xD0;
		buf[1] = 0x4C;
		buf[2] = 0x80;
		cst3xx_i2c_write(g_i2c_client, buf, 3);		
		PRINTK(" linc tpd-gesture detect is opened \n");
		
        mdelay(10);
        return;
    }
#endif

#ifdef CONFIG_TP_ESD_PROTECT

	cst3xx_esd_switch(SWITCH_ESD_OFF);

#endif

    disable_irq(touch_irq);


#ifdef SLEEP_CLEAR_POINT
	#ifdef ICS_SLOT_REPORT
		for(idx=0; idx<=10; idx++) {	
			input_mt_slot(tpd->dev, idx);
			input_report_abs(tpd->dev, ABS_MT_TRACKING_ID, -1);
			input_mt_report_slot_state(tpd->dev, MT_TOOL_FINGER, false);
		}
	#else
	    input_report_key(tpd->dev, BTN_TOUCH, 0);
		input_mt_sync(tpd->dev);
	#endif
		input_sync(tpd->dev);	
#endif	

    cst3xx_enter_sleep(g_i2c_client);
	
	if (regulator_disable(tpd->reg)){
		PRINTK("regulator_disable() failed!");
	}
	
	PRINTK(" linc cst3xx enter sleep done.\n");
}


static struct tpd_driver_t cst3xx_ts_device = {
	.tpd_device_name = "cst3xx",
	.tpd_local_init = cst3xx_local_init,
	.suspend = cst3xx_suspend,
	.resume = cst3xx_resume,

	#ifdef TPD_HAVE_BUTTON
	.tpd_have_button = 1,
    #else
	.tpd_have_button = 0,
    #endif
};



/* called when loaded into kernel */
static int __init cst3xx_ts_init(void)
{
	PRINTK(" linc hyn is entering cst3xx_ts_init.\n");


	//i2c_register_board_info(I2C_BUS_NUMBER, &cst3xx_i2c_tpd, 1);

	tpd_get_dts_info();

	if (tpd_driver_add(&cst3xx_ts_device) < 0)
		PRINTK(" linc add cst3xx driver failed.\n");

	return 0;
}

/* should never be called */
static void __exit cst3xx_ts_exit(void)
{
	PRINTK(" linc hyn is entering cst3xx_ts_exit.\n");
	tpd_driver_remove(&cst3xx_ts_device);
}

module_init(cst3xx_ts_init);
module_exit(cst3xx_ts_exit);

