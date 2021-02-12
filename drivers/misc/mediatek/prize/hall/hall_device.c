/***********************************************************
 *  版权所有 (C) 2015-2020, 深圳市铂睿智恒科技有限公司 
 *
 *  文件名称: hall_device.c
 *  内容摘要: hall driver for hall device
 *  当前版本: V1.0
 *  作    者: 丁俊
 *  完成日期: 2015-04-10
 *  修改记录: 
 *  修改日期: 
 *  版本号  :
 *  修改人  :
 ***********************************************************/

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>

#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/ctype.h>

#include <linux/semaphore.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/workqueue.h>
//#include <linux/switch.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/kthread.h>
#include <linux/input.h>
#if defined(CONFIG_PM_WAKELOCKS)
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include <linux/time.h>

#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

//#include <mach/mt_typedefs.h>
//#include <mach/mt_reg_base.h>
//#include <mach/irqs.h>
//#include <accdet_custom.h>
//#include <accdet_custom_def.h>

//#include <cust_eint.h>
//#include <cust_gpio_usage.h>

//#include <mach/mt_gpio.h>
//#include <mach/eint.h>

//#include <linux/mtgpio.h>
#include <linux/gpio.h>
#include <linux/input.h>


/*----------------------------------------------------------------------
static variable defination
----------------------------------------------------------------------*/
#define HALL_DEVNAME    "hall_dev"

#define EN_DEBUG

#if defined(EN_DEBUG)
		
#define TRACE_FUNC 	printk("[hall_dev] function: %s, line: %d \n", __func__, __LINE__);

#define HALL_DEBUG  printk
#else

#define TRACE_FUNC(x,...)

#define HALL_DEBUG(x,...)
#endif

#define  HALL_CLOSE   0
#define  HALL_OPEN    1

//#define HALL_SWITCH_EINT        CUST_EINT_HALL_1_NUM
//#define HALL_SWITCH_DEBOUNCE    CUST_EINT_HALL_1_DEBOUNCE_CN		/* ms */
//#define HALL_SWITCH_TYPE        CUST_EINT_HALL_1_TYPE           /*EINTF_TRIGGER_LOW*/
//#define HALL_SWITCH_SENSITIVE   MT_LEVEL_SENSITIVE

/****************************************************************/
/*******static function defination                             **/
/****************************************************************/
static dev_t g_hall_devno;
static struct cdev *g_hall_cdev = NULL;
static struct class *hall_class = NULL;
static struct device *hall_nor_device = NULL;
static struct input_dev *hall_input_dev;
static struct kobject *g_hall_sys_device;
static volatile int cur_hall_status = HALL_OPEN;
//prize modified by huarui, update with kernel version 20200408 start
#if defined(CONFIG_PM_WAKELOCKS)
struct wakeup_source hall_key_lock;
#else
struct wake_lock hall_key_lock;
#endif
//prize modified by huarui, update with kernel version 20200408 end
static int hall_key_event = 0;
static int g_hall_first = 1;
unsigned int hall_irq = 1;
unsigned int hall_eint_type;
//u32 ints[2] = {0, 0};

static struct pinctrl *hall_pinctrl;
static struct pinctrl_state *hall_eint_default;
static struct pinctrl_state *hall_eint_as_int;

static const struct of_device_id hall_dt_match[] = {
	{.compatible = "mediatek,hall_1"},
	{},
};

static void hall_eint_handler(unsigned long data);
static DECLARE_TASKLET(hall_tasklet, hall_eint_handler, 0);

static atomic_t send_event_flag = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(send_event_wq);
extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);

/****************************************************************/
/*******export function defination                             **/
/****************************************************************/

static ssize_t hall_status_info_show(struct device *dev,
                struct device_attribute *attr,
                char *buf)
{
	HALL_DEBUG("[hall_dev] cur_hall_status=%d\n", cur_hall_status);
	return sprintf(buf, "%u\n", cur_hall_status);
}

static ssize_t hall_status_info_store(struct device *dev,
                struct device_attribute *attr,
                const char *buf, size_t size)
{
	HALL_DEBUG("[hall_dev] %s ON/OFF value = %d:\n ", __func__, cur_hall_status);

	if(sscanf(buf, "%u", &cur_hall_status) != 1)
	{
		HALL_DEBUG("[hall_dev]: Invalid values\n");
		return -EINVAL;
	}
	return size;
}

static DEVICE_ATTR(hall_status, 0644, hall_status_info_show,  hall_status_info_store);

static irqreturn_t switch_hall_eint_handler(int irq, void *dev_id)
{
    TRACE_FUNC;
    tasklet_schedule(&hall_tasklet);
	disable_irq_nosync(hall_irq);
	return IRQ_HANDLED;
}

static int sendKeyEvent(void *unuse)
{
    while(1)
    {
        HALL_DEBUG("[hall_dev]:sendKeyEvent wait\n");
        //wait for signal
        wait_event_interruptible(send_event_wq, (atomic_read(&send_event_flag) != 0));

//prize modified by huarui, update with kernel version 20200408 start
	#if defined(CONFIG_PM_WAKELOCKS)
		__pm_wakeup_event(&hall_key_lock, 2*HZ);
	#else
        wake_lock_timeout(&hall_key_lock, 2*HZ);    //set the wake lock.
	#endif
//prize modified by huarui, update with kernel version 20200408 end
        HALL_DEBUG("[hall_dev]:going to send event %d\n", hall_key_event);

        //send key event
        if(HALL_OPEN == hall_key_event)
          {
                HALL_DEBUG("[hall_dev]:HALL_OPEN!\n");
		/*prize-lixuefeng-20150602-start*/
                input_report_key(hall_input_dev, KEY_F8, 1);
                input_report_key(hall_input_dev, KEY_F8, 0);
		/*prize-lixuefeng-20150602-end*/
                input_sync(hall_input_dev);
          }
	      else if(HALL_CLOSE == hall_key_event)
          {
                HALL_DEBUG("[hall_dev]:HALL_CLOSE!\n");
		/*prize-lixuefeng-20150602-start*/
                input_report_key(hall_input_dev, KEY_F7, 1);
                input_report_key(hall_input_dev, KEY_F7, 0);
		/*prize-lixuefeng-20150602-end*/
                input_sync(hall_input_dev);
          }
        atomic_set(&send_event_flag, 0);
    }
    return 0;
}

static ssize_t notify_sendKeyEvent(int event)
{
    hall_key_event = event;
    atomic_set(&send_event_flag, 1);
    wake_up(&send_event_wq);
    HALL_DEBUG("[hall_dev]:notify_sendKeyEvent !\n");
    return 0;
}

static void hall_eint_handler(unsigned long data)
{
   // u8 old_hall_state = cur_hall_status;
    
    TRACE_FUNC;

   cur_hall_status = !cur_hall_status;
		
   if(cur_hall_status)
   {
   	HALL_DEBUG("[hall_dev]:HALL_opened \n");
	irq_set_irq_type(hall_irq, IRQ_TYPE_EDGE_FALLING);
	  notify_sendKeyEvent(HALL_OPEN);
   }
   else
   {
   	HALL_DEBUG("[hall_dev]:HALL_closed \n");
	irq_set_irq_type(hall_irq, IRQ_TYPE_EDGE_RISING);
	  notify_sendKeyEvent(HALL_CLOSE);
	  
   }
	
    /* for detecting the return to old_hall_state */
    mdelay(10); 
    enable_irq(hall_irq);// mt_eint_unmask(HALL_SWITCH_EINT);
}

static long hall_unlocked_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
    HALL_DEBUG("[hall_dev]:hall_unlocked_ioctl \n");

    return 0;
}

static int hall_open(struct inode *inode, struct file *file)
{ 
   	return 0;
}

static int hall_release(struct inode *inode, struct file *file)
{
    return 0;
}

static struct file_operations g_hall_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl		= hall_unlocked_ioctl,
	.open		= hall_open,
	.release	= hall_release,	
};

static int hall_get_dts_fun(struct platform_device *pdev)
{
	int ret = 0;

	hall_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(hall_pinctrl)) {
		HALL_DEBUG("Cannot find hall pinctrl!");
		ret = PTR_ERR(hall_pinctrl);
	}
	
	//hall eint pin initialization 
	hall_eint_default= pinctrl_lookup_state(hall_pinctrl, "default");
	if (IS_ERR(hall_eint_default)) {
		ret = PTR_ERR(hall_eint_default);
		HALL_DEBUG("%s : init err, hall_eint_default\n", __func__);
	}

	hall_eint_as_int = pinctrl_lookup_state(hall_pinctrl, "hall_eint");
	if (IS_ERR(hall_eint_as_int)) {
		ret = PTR_ERR(hall_eint_as_int);
		HALL_DEBUG("%s : init err, hall_eint\n", __func__);
	}
	else
		pinctrl_select_state(hall_pinctrl, hall_eint_as_int);
	
	return ret;
}

static int hall_irq_registration(void)
{
	struct device_node *node = NULL;
	u32 ints[2] = { 0, 0 };
	u32 ints1[2] = { 0, 0 };
	
	node = of_find_matching_node(node, hall_dt_match);
	if (node) {
		of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
		of_property_read_u32_array(node, "interrupts", ints1, ARRAY_SIZE(ints1));
		
		hall_eint_type = ints1[1];
		
		//gpio_request(ints[0], "hall_1");
		gpio_set_debounce(ints[0], ints[1]);
		pinctrl_select_state(hall_pinctrl, hall_eint_as_int);
		printk("ints[0] = %d, ints[1] = %d!!\n", ints[0], ints[1]);
	
		hall_irq = irq_of_parse_and_map(node, 0);
		printk("hall_irq = %d\n", hall_irq);
		if (!hall_irq) {
			printk("hall irq_of_parse_and_map fail!!\n");
			return -EINVAL;
		}
		
		if (request_irq(hall_irq, switch_hall_eint_handler, IRQF_TRIGGER_LOW, "hall-eint", NULL)) {
			printk("HALL IRQ LINE NOT AVAILABLE!!\n");
			return -EINVAL;
		}
/* begin, prize-lifenfen-20181220, add for hall irq wakeup func */
		else
			enable_irq_wake(hall_irq);
/* end, prize-lifenfen-20181220, add for hall irq wakeup func */
		//enable_irq(hall_irq);
	} else {
		printk("null hall irq node!!\n");
		return -EINVAL;
	}

	
	return 0;
}

static int hall_probe(struct platform_device *pdev)
{
    int ret = 0;
    struct task_struct *keyEvent_thread = NULL;

    TRACE_FUNC;

   ret = alloc_chrdev_region(&g_hall_devno, 0, 1, HALL_DEVNAME);

  if (ret)
  {
	HALL_DEBUG("[hall_dev]:alloc_chrdev_region: Get Major number error!\n");			
  }

  g_hall_cdev = cdev_alloc();

  if(NULL == g_hall_cdev)
  {
  	unregister_chrdev_region(g_hall_devno, 1);
	HALL_DEBUG("[hall_dev]:Allocate mem for kobject failed\n");
	return -ENOMEM;
  }

  //Attatch file operation.
  cdev_init(g_hall_cdev, &g_hall_fops);
  g_hall_cdev->owner = THIS_MODULE;

  //Add to system
  ret = cdev_add(g_hall_cdev, g_hall_devno, 1);
  
  if(ret)
   {
   	HALL_DEBUG("[hall_dev]:Attatch file operation failed\n");
   	unregister_chrdev_region(g_hall_devno, 1);
	return -ENOMEM;
   }
	
   hall_class = class_create(THIS_MODULE, HALL_DEVNAME);
   if (IS_ERR(hall_class))
   {
        ret = PTR_ERR(hall_class);
        HALL_DEBUG("[hall_dev]:Unable to create class, err = %d\n", ret);
        return ret;
   }

    // if we want auto creat device node, we must call this
   hall_nor_device = device_create(hall_class, NULL, g_hall_devno, NULL, HALL_DEVNAME);  
	
   hall_input_dev = input_allocate_device();
	
   if (!hall_input_dev)
   {
   	HALL_DEBUG("[hall_dev]:hall_input_dev : fail!\n");
       return -ENOMEM;
   }

   __set_bit(EV_KEY, hall_input_dev->evbit);
   /*prize-lixuefeng-20150602-start*/
   __set_bit(KEY_F8, hall_input_dev->keybit);
   __set_bit(KEY_F7, hall_input_dev->keybit);
   /*prize-lixuefeng-20150602-end*/
  hall_input_dev->id.bustype = BUS_HOST;
  hall_input_dev->name = "HALL_DEV";
  if(input_register_device(hall_input_dev))
  {
	HALL_DEBUG("[hall_dev]:hall_input_dev register : fail!\n");
  }else
  {
	HALL_DEBUG("[hall_dev]:hall_input_dev register : success!!\n");
  }

//prize modified by huarui, update with kernel version 20200408 start
#if defined(CONFIG_PM_WAKELOCKS)
	wakeup_source_init(&hall_key_lock, "hall key wakelock");
#else
	wake_lock_init(&hall_key_lock, WAKE_LOCK_SUSPEND, "hall key wakelock");
#endif
//prize modified by huarui, update with kernel version 20200408 end
  
   init_waitqueue_head(&send_event_wq);
   //start send key event thread
   keyEvent_thread = kthread_run(sendKeyEvent, 0, "keyEvent_send");
   if (IS_ERR(keyEvent_thread)) 
   { 
      ret = PTR_ERR(keyEvent_thread);
      HALL_DEBUG("[hall_dev]:failed to create kernel thread: %d\n", ret);
   }

   if(g_hall_first)
   {
    	g_hall_sys_device = kobject_create_and_add("hall_state", NULL);
    	if (g_hall_sys_device == NULL)
	{
        	HALL_DEBUG("[hall_dev]:%s: subsystem_register failed\n", __func__);
        	ret = -ENXIO;
        	return ret ;
	}
	
	ret = sysfs_create_file(g_hall_sys_device, &dev_attr_hall_status.attr);
	if (ret) 
	{
      	HALL_DEBUG("[hall_dev]:%s: sysfs_create_file failed\n", __func__);
       	kobject_del(g_hall_sys_device);
   	}

	/*mt_set_gpio_mode(GPIO_HALL_1_PIN, GPIO_HALL_1_PIN_M_EINT);
	mt_set_gpio_dir(GPIO_HALL_1_PIN, GPIO_DIR_IN);
	mt_eint_set_sens(HALL_SWITCH_EINT, HALL_SWITCH_SENSITIVE);
	mt_eint_set_hw_debounce(HALL_SWITCH_EINT, 5);
	mt_eint_registration(HALL_SWITCH_EINT, HALL_SWITCH_TYPE, switch_hall_eint_handler, 0);*/

	//wyq 20160301 get eint gpio from dts and register
	hall_get_dts_fun(pdev);
	hall_irq_registration();
	
	g_hall_first = 0;
   }	
    return 0;
}

static int hall_remove(struct platform_device *dev)	
{
	HALL_DEBUG("[hall_dev]:hall_remove begin!\n");
	
	device_del(hall_nor_device);
	class_destroy(hall_class);
	cdev_del(g_hall_cdev);
	unregister_chrdev_region(g_hall_devno,1);	
	input_unregister_device(hall_input_dev);
	HALL_DEBUG("[hall_dev]:hall_remove Done!\n");
    
	return 0;
}

static struct platform_driver hall_driver = {
	.probe	= hall_probe,
	.remove  = hall_remove,
	.driver    = {
		.name       = "Hall_Driver",
		.of_match_table = of_match_ptr(hall_dt_match),
	},
};

//static struct platform_device hall_device = {
//	.name = "Hall_Driver",
//	.id = -1
//};

static int __init hall_init(void)
{

    int retval = 0;
    TRACE_FUNC;
	//  retval = platform_device_register(&hall_device);
    printk("[%s]: hall_device, retval=%d \n!", __func__, retval);
	  if (retval != 0) {
		  return retval;
	  }

    platform_driver_register(&hall_driver);

    return 0;
}

static void __exit hall_exit(void)
{
    TRACE_FUNC;
    platform_driver_unregister(&hall_driver);
    //platform_device_unregister(&hall_device);
}

module_init(hall_init);
module_exit(hall_exit);
MODULE_DESCRIPTION("HALL DEVICE driver");
MODULE_AUTHOR("dingjun <dingj@boruizhiheng.com>");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("halldevice");

