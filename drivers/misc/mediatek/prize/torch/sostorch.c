/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/cdev.h>

#include "sostorch.h"

#define SOSTORCH_NAME "sostorch"


struct sostorch_data *sostorch = NULL;

/* define mutex and work queue */

/* sostorch chip data */


/******************************************************************************
 * sostorch operations
 *****************************************************************************/
 /* this current is single led current,total current is x2 */


/*******************************************************************************
* device file
*******************************************************************************/
static ssize_t sostorch_level_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct sostorch_data *sostorch = dev_get_platdata(dev);
	int ret = 0;

	ret = sprintf(buf,"%d\n",atomic_read(&sostorch->level));

	return ret;
}
 static ssize_t sostorch_level_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
 {
	struct sostorch_data *sostorch = dev_get_platdata(dev);
	int level = 0;
	
	sscanf(buf,"%d",&level);
	atomic_set(&sostorch->level,level);
	//sostorch->ops.set_level(atomic_read(&sostorch->level));
	pr_info("sostorch level_sotre %d\n",level);
	
	return size;
 }
static DEVICE_ATTR(level, 0664, sostorch_level_show, sostorch_level_store);

static ssize_t sostorch_mode_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct sostorch_data *sostorch = dev_get_platdata(dev);
	int ret = 0;

	ret = sprintf(buf,"%d\n",atomic_read(&sostorch->mode));

	return ret;
}
 static ssize_t sostorch_mode_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
 {
	struct sostorch_data *sostorch = dev_get_platdata(dev);
	int mode = 0;
	
	sscanf(buf,"%d",&mode);
	atomic_set(&sostorch->mode,mode);
	pr_info("sostorch mode_sotre %d\n",mode);
	
	return size;
 }
static DEVICE_ATTR(mode, 0664, sostorch_mode_show, sostorch_mode_store);

static ssize_t sostorch_enable_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct sostorch_data *sostorch = dev_get_platdata(dev);
	int ret = 0;

	ret = sprintf(buf,"%d\n",atomic_read(&sostorch->on_state));

	return ret;
}
 static ssize_t sostorch_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
 {
	struct sostorch_data *sostorch = dev_get_platdata(dev);
	int state = 0;
	
	sscanf(buf,"%d",&state);
	mutex_lock(&sostorch->mutex_ops);
	if (state){
		//on
		pm_stay_awake(&sostorch->pdev->dev);
		sostorch->ops.enable(sostorch,1);
	}else{
		//off
		sostorch->ops.enable(sostorch,0);
		pm_relax(&sostorch->pdev->dev);
	}
	atomic_set(&sostorch->on_state,state);
	mutex_unlock(&sostorch->mutex_ops);
	pr_info("sostorch enable_sotre %d\n",state);
	
	return size;
 }
static DEVICE_ATTR(enable, 0664, sostorch_enable_show, sostorch_enable_store);

/******************************************************************************
 * chrdev
 *****************************************************************************/



/*******************************************************************************
 * ioctl
 ******************************************************************************/
 #ifdef CONFIG_COMPAT
static long sostorch_compat_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long ret = 0;
	
	compat_uint_t __user *data32;
	unsigned int __user *data;
	compat_uint_t d;

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	data32 = compat_ptr(arg);
	data = compat_alloc_user_space(sizeof(*data));
	if (data == NULL)
		return -EFAULT;
	ret = get_user(d, data32);
	ret |= put_user(d, data);
	if (ret)
		return ret;

	switch (cmd) {
		case IOCTL_COMPAT_SET_LEVEL:
			ret = file->f_op->unlocked_ioctl(file, IOCTL_SET_LEVEL,
							 (unsigned long)data);
			break;
		case IOCTL_COMPAT_SET_MODE:
			ret = file->f_op->unlocked_ioctl(file, IOCTL_SET_MODE,
							 (unsigned long)data);
			break;
		case IOCTL_COMPAT_SET_ENABLE:
			ret = file->f_op->unlocked_ioctl(file, IOCTL_SET_ENABLE,
							 (unsigned long)data);
			break;
		case IOCTL_COMPAT_GET_LEVEL:
			ret = file->f_op->unlocked_ioctl(file, IOCTL_GET_LEVEL,
							 (unsigned long)data);
			break;
		default:
			pr_err("unsupport cmd %d\n",cmd);
	}
	
	ret |= get_user(d, data);
	ret |= put_user(d, data32);
	return ret;
}
#endif

static long sostorch_ioctl(struct file *file, unsigned int cmd, 
					unsigned long arg){
	long ret = 0;
	uint32_t val = 0;
	
	get_user(val,(uint32_t __user *) arg);
	pr_info("cmd %d,arg %d\n",cmd,val);
	
	mutex_lock(&sostorch->mutex_ops);
	switch (cmd) {
		case IOCTL_SET_LEVEL:
			atomic_set(&sostorch->level,val);
			break;
		case IOCTL_SET_MODE:
			atomic_set(&sostorch->mode,val);
			break;
		case IOCTL_SET_ENABLE:
			ret = sostorch->ops.enable(sostorch,val);
			break;
		case IOCTL_GET_LEVEL:
			ret = put_user(atomic_read(&sostorch->level),(unsigned long *)arg);
			break;
		default:
			pr_err("unsupport cmd %d\n",cmd);
			ret = -EINVAL;
	}
	mutex_unlock(&sostorch->mutex_ops);
	return ret;
}

static int sostorch_open(struct inode *a_pstInode, struct file *a_pstFile){
	int ret = 0;
	pr_debug("%s start\n", __func__);
	return ret;
}

static int sostorch_release(struct inode *inode, struct file *file){
	return 0;
}

static ssize_t sostorch_write(struct file *file, const char __user *ubuf,
			      size_t count, loff_t *ppos){
	return 0;
}

static const struct file_operations sostorch_fops = {
	.write = sostorch_write,
	.open = sostorch_open,
	.release = sostorch_release,
	.unlocked_ioctl = sostorch_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = sostorch_compat_ioctl,
#endif
};

static dev_t g_devNum;
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static inline int sostorch_chrdev_reg(void){
	struct device *device = NULL;
	int ret = 0;

	pr_debug("%s Start\n", __func__);

	if (alloc_chrdev_region(&g_devNum, 0, 1, "sostorch")) {
		pr_debug("Allocate device no failed\n");
		return -EAGAIN;
	}

	g_charDrv = cdev_alloc();
	if (g_charDrv == NULL) {
		unregister_chrdev_region(g_devNum, 1);
		pr_debug("Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	cdev_init(g_charDrv, &sostorch_fops);
	g_charDrv->owner = THIS_MODULE;

	if (cdev_add(g_charDrv, g_devNum, 1)) {
		pr_debug("Attatch file operation failed\n");
		unregister_chrdev_region(g_devNum, 1);
		return -EAGAIN;
	}

	g_drvClass = class_create(THIS_MODULE, "sostorch");
	if (IS_ERR(g_drvClass)) {
		ret = PTR_ERR(g_drvClass);
		pr_debug("Unable to create class, err = %d\n", ret);
		return ret;
	}
	device = device_create(g_drvClass, NULL, g_devNum, NULL,"sostorch");
	pr_debug("%s End\n", __func__);

	return 0;
}

static void sostorch_chrdev_unreg(void)
{
	/*Release char driver */
	class_destroy(g_drvClass);
	device_destroy(g_drvClass, g_devNum);
	cdev_del(g_charDrv);
	unregister_chrdev_region(g_devNum, 1);
}
/******************************************************************************
 * Platform device and driver
 *****************************************************************************/
static int sostorch_parse_dt(struct device *dev,struct sostorch_data *sostorch){
	return 0;
}

extern int aw3641_init(struct platform_device *pdev);
static int sostorch_probe(struct platform_device *pdev)
{
	//struct sostorch_data *sostorch = NULL;//dev_get_platdata(&pdev->dev);
	int ret;

	pr_debug("Probe start.\n");

	/* init platform data */
	sostorch = devm_kzalloc(&pdev->dev, sizeof(*sostorch), GFP_KERNEL);
	if (!sostorch) {
		ret = -ENOMEM;
		goto err_free;
	}
	
	sostorch->pdev = pdev;
	
	atomic_set(&sostorch->level,0);
	atomic_set(&sostorch->mode,MODE_NONE);
	atomic_set(&sostorch->on_state,0);

	ret = sostorch_parse_dt(&pdev->dev, sostorch);
	if (ret)
		goto err_free;
	
	pdev->dev.platform_data = sostorch;

	ret = aw3641_init(pdev);
	if (ret){
		goto err_free;
	}
	
	mutex_init(&sostorch->mutex_ops);
	//wakeup
	device_init_wakeup(&pdev->dev,1);


	//dev
	ret = sostorch_chrdev_reg();
	if (ret){
		pr_err(" device chrdev file level fail %d\n",ret);
		goto err_free;
	}
	//create sysfs
	ret =  device_create_file(&pdev->dev, &dev_attr_level);
	if (ret){
		pr_err(" device_create_file level fail %d\n",ret);
		goto err_devfile;
	}
	ret =  device_create_file(&pdev->dev, &dev_attr_mode);
	if (ret){
		pr_err(" device_create_file mode fail %d\n",ret);
		goto err_devfile;
	}
	ret =  device_create_file(&pdev->dev, &dev_attr_enable);
	if (ret){
		pr_err(" device_create_file enable fail %d\n",ret);
		goto err_devfile;
	}

	pr_debug("Probe done.\n");

	return 0;
err_devfile:
	device_remove_file(&pdev->dev,&dev_attr_level);
	device_remove_file(&pdev->dev,&dev_attr_mode);
	device_remove_file(&pdev->dev,&dev_attr_enable);
err_free:
	devm_kfree(&pdev->dev,sostorch);
	return ret;
}

static int sostorch_remove(struct platform_device *pdev)
{
	struct sostorch_data *sostorch = dev_get_platdata(&pdev->dev);

	pr_debug("Remove start.\n");

	sostorch_chrdev_unreg();
	device_remove_file(&pdev->dev, &dev_attr_level);
	device_remove_file(&pdev->dev, &dev_attr_mode);
	device_remove_file(&pdev->dev, &dev_attr_enable);

	sostorch_chrdev_unreg();
	
	sostorch->ops.exit(pdev);
	devm_kfree(&pdev->dev,sostorch);

	pr_debug("Remove done.\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sostorch_of_match[] = {
	{.compatible = "prize,sostorch"},
	{},
};
MODULE_DEVICE_TABLE(of, sostorch_of_match);
#else
static struct platform_device sostorch_platform_device[] = {
	{
		.name = SOSTORCH_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, sostorch_platform_device);
#endif

static struct platform_driver sostorch_platform_driver = {
	.probe = sostorch_probe,
	.remove = sostorch_remove,
	.driver = {
		.name = SOSTORCH_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = sostorch_of_match,
#endif
	},
};

static int __init sostorch_init(void)
{
	int ret;

	pr_debug("Init start.\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&sostorch_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&sostorch_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done.\n");

	return 0;
}

static void __exit sostorch_exit(void)
{
	pr_debug("Exit start.\n");

	platform_driver_unregister(&sostorch_platform_driver);

	pr_debug("Exit done.\n");
}

module_init(sostorch_init);
module_exit(sostorch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HH@PRIZE");

