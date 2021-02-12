/**
 * Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
 * 2018-10-16 File created.
 */

#include "fsm_public.h"
//#include <sound/fsm-dev.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

/* i2s clock ratio = fbclk/flrclk */
#define FSM_BCLK_RATIO 		(64)//mtk: 64, qcom: 32

/* ioctl magic number and cmd. */
#define FSM_IOC_MAGIC		0x7c
#define FSM_IOC_GET_DEVICE	_IOR(FSM_IOC_MAGIC, 1, int)
#define FSM_IOC_SET_SRATE	_IOW(FSM_IOC_MAGIC, 2, int)
#define FSM_IOC_SET_BCLK	_IOW(FSM_IOC_MAGIC, 3, int)
#define FSM_IOC_SET_SCENE	_IOW(FSM_IOC_MAGIC, 4, int)
#define FSM_IOC_INIT		_IOW(FSM_IOC_MAGIC, 5, int)
#define FSM_IOC_CALIBRATE	_IOW(FSM_IOC_MAGIC, 6, int)
#define FSM_IOC_SPEAKER_ON	_IOW(FSM_IOC_MAGIC, 7, int)
#define FSM_IOC_SPEAKER_OFF	_IOW(FSM_IOC_MAGIC, 8, int)
#define FSM_IOC_GET_RE25	_IOR(FSM_IOC_MAGIC, 9, int)
#define FSM_IOC_SET_SLAVE	_IOW(FSM_IOC_MAGIC, 10, int)
#define FSM_IOC_MAXNR		10

/*
 * misc driver for foursemi devices.
 */

#define FSM_MISC_NAME	"smartpa_i2c"

#define MAX_L 128

struct fsm_i2c_cfg
{
	uint8_t addr;
};
struct fsm_i2c_cfg g_i2c_cfg;

static int fsm_misc_check_params(unsigned int cmd, unsigned long arg)
{
	int err = 0;

	if(_IOC_TYPE(cmd) != FSM_IOC_MAGIC || _IOC_NR(cmd) > FSM_IOC_MAXNR)
	{
		return -ENOTTY;
	}

	if(_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	}
	else if(_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	return err ? -EFAULT : 0;
}

static int fsm_misc_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	return ret;
}

static void fsm_misc_set_slave(uint8_t slave)
{
	struct fsm_i2c_cfg *i2c_cfg = &g_i2c_cfg;

	i2c_cfg->addr = slave;
}

static long fsm_misc_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int ret = 0, value = 0;
	char str_buffer[MAX_L];
	int len = 0;
    fsm_config_t *config = fsm_get_config();

	ret = fsm_misc_check_params(cmd, arg);
	if(ret)
	{
		pr_err("invaild parameters: %x, %lx\n", cmd, arg);
		return ret;
	}

	switch(cmd)
	{
	case FSM_IOC_SET_SLAVE:
		fsm_mutex_lock();
		fsm_misc_set_slave((uint8_t)arg);
		fsm_mutex_unlock();
		break;
	case FSM_IOC_GET_DEVICE:
		value = fsm_i2c_event(FSM_EVENT_GET_DEVICE, 0 /* unused */);
		if(copy_to_user((int *)arg, &value, sizeof(int)))
			return -EFAULT;
		break;
	case FSM_IOC_SET_SRATE:
		fsm_i2c_event(FSM_EVENT_SET_SRATE, arg);
		value = (unsigned int)(arg * FSM_BCLK_RATIO);
		if(arg == 32000)
			value += 32;
		fsm_i2c_event(FSM_EVENT_SET_BCLK, value);
		break;
	case FSM_IOC_SET_BCLK:
		fsm_i2c_event(FSM_EVENT_SET_BCLK, arg);
		break;
	case FSM_IOC_SET_SCENE:
        config->next_scene = BIT(arg);
		fsm_i2c_event(FSM_EVENT_SET_SCENE, arg);
		break;
	case FSM_IOC_SPEAKER_ON:
		ret = fsm_i2c_event(FSM_EVENT_LOAD_FW, 0);
		ret |= fsm_i2c_event(FSM_EVENT_INIT, 0);
		ret = fsm_i2c_event(FSM_EVENT_SPEAKER_ON, 0);
		//ret |= fsm_i2c_event(FSM_EVENT_CALIBRATE, 0);
		break;
	case FSM_IOC_SPEAKER_OFF:
		ret = fsm_i2c_event(FSM_EVENT_SPEAKER_OFF, arg);
		break;
	case FSM_IOC_INIT:
		ret = fsm_i2c_event(FSM_EVENT_LOAD_FW, 0);
		//ret |= fsm_i2c_event(FSM_EVENT_INIT, 1);
		break;
	case FSM_IOC_CALIBRATE:
		ret = fsm_i2c_event(FSM_EVENT_CALIBRATE, arg);
		break;
	case FSM_IOC_GET_RE25:
		len = fsm_get_r25_str(str_buffer,MAX_L);
		str_buffer[len] = '\0';
		if(copy_to_user((char *)arg, str_buffer, MAX_L))
			return -EFAULT;
		break;
	
	/*	value = fsm_i2c_event(FSM_EVENT_GET_RE25, arg);
		if(copy_to_user((int *)arg, &value, sizeof(int)))
			return -EFAULT;
	*/		
	default:
		ret = -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_COMPAT)
static long fsm_misc_compat_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	return fsm_misc_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static ssize_t fsm_misc_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct fsm_i2c_cfg *i2c_cfg = &g_i2c_cfg;
	struct i2c_client *i2c = NULL;
	int ret = 0, retries = 20;
	char *tmp = NULL;

	tmp = fsm_alloc_mem(count);
	if (tmp == NULL)
		return -ENOMEM;

	do
	{
		fsm_mutex_lock();
		pr_info("addr : 0x%02x ",i2c_cfg->addr);
		i2c = fsm_get_i2c(i2c_cfg->addr);
		if(i2c) {
			ret = i2c_master_recv(i2c, tmp, count);
			ret = (ret == count) ? 0 : ret;
			if(ret)
				fsm_delay_ms(5);
		}
		else
			ret = -EINVAL;
		fsm_mutex_unlock();
	}
	while(ret && (--retries <= 0));
	
	if(ret == 0)
		ret = copy_to_user((char *)buf, tmp, count)? (-EFAULT) : ret;
	else
		pr_err("reading %zu bytes failed, ret: %d.", count, ret);
	if(tmp)
		fsm_free_mem(tmp);

	//i2c_cfg->addr = 0;

	return ret;
}

static ssize_t fsm_misc_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct fsm_i2c_cfg *i2c_cfg = &g_i2c_cfg;
	struct i2c_client *i2c = NULL;
	int ret = 0, retries = 20;
	uint8_t *tmp = NULL;

	tmp = memdup_user(buf, count);
	if (IS_ERR(tmp))
		return PTR_ERR(tmp);

	do
	{
		fsm_mutex_lock();
		pr_info("addr : 0x%02x ",i2c_cfg->addr);
		i2c = fsm_get_i2c(i2c_cfg->addr);
		if(i2c) {
			ret = i2c_master_send(i2c, tmp, count);
			ret = (ret == count) ? 0 : ret;
			if(ret)
				fsm_delay_ms(5);
		}
		else
			ret = -EINVAL;
		fsm_mutex_unlock();
	}
	while(ret && (--retries <= 0));
	if(ret != 0)
		pr_err("writing %zu bytes failed, ret: %d.", count, ret);
	if(tmp)
		fsm_free_mem(tmp);

	return ret;
}

static const struct file_operations fsm_file_ops =
{
	.owner	= THIS_MODULE,
	.open	= fsm_misc_open,
	.unlocked_ioctl = fsm_misc_ioctl,
#if defined(CONFIG_COMPAT)
	.compat_ioctl = fsm_misc_compat_ioctl,
#endif
	.llseek	= no_llseek,
	.read	= fsm_misc_read,
	.write	= fsm_misc_write,
};

struct miscdevice fsm_misc_dev =
{
	.minor = MISC_DYNAMIC_MINOR,
	.name  = FSM_MISC_NAME,
	.fops  = &fsm_file_ops,
};

int fsm_misc_init(void)
{
	struct miscdevice *misc = &fsm_misc_dev;
	int ret;

	pr_debug("enter");
	if (misc->this_device) {
		return 0;
	}
	ret = misc_register(misc);
	if (ret) {
		misc->this_device = NULL;
	}

	return ret;
}

void fsm_misc_deinit(void)
{
	struct miscdevice *misc = &fsm_misc_dev;

	pr_debug("enter");
	if (misc->this_device == NULL) {
		return;
	}
	misc_deregister(misc);
}
