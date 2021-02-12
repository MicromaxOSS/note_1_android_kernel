
//#include <linux/pm_wakeup.h>


//#ifdef CONFIG_PM_WAKELOCKS
//struct wakeup_source kpd_suspend_lock;
//#else
//struct wake_lock kpd_suspend_lock;
//#endif
//#ifdef CONFIG_PM_WAKELOCKS
//        __pm_wakeup_event(&kpd_suspend_lock, 500);                                                                                                                                                            
//#else
//        wake_lock_timeout(&kpd_suspend_lock, HZ / 2);
//#endif
//#ifdef CONFIG_PM_WAKELOCKS
//	wakeup_source_init(&kpd_suspend_lock, "kpd wakelock");
//#else
//	wake_lock_init(&kpd_suspend_lock, WAKE_LOCK_SUSPEND, "kpd wakelock");
//#endif
//
////linux/pm_wakeup.h
//pm_stay_awake(&tcm_hcd->pdev->dev);
//pm_relax(&tcm_hcd->pdev->dev);
//device_init_wakeup(&pdev->dev, 1);
//=====================================
//struct wakeup_source *acer_suspend_lock;
//__pm_wakeup_event(acer_suspend_lock, 2 * HZ);
//wakeup_source_init(acer_suspend_lock, "acer wakelock");

#ifndef __SOSTORCH_H__
#define __SOSTORCH_H__

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/string.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/regmap.h>
#include <linux/ioctl.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/mutex.h>


struct sostorch_data;

enum sostorch_mode {
	MODE_NONE = 0,
	MODE_TORCH,
	MODE_FLASH,
	MODE_SOS,
	MODE_MAX,
};

struct fl_dev {
	int (*enable)(struct sostorch_data *,int);
	int (*parse_dt)(struct platform_device *);
	int (*exit)(struct platform_device *);
};

struct sostorch_data {
	atomic_t level;
	atomic_t mode;
	atomic_t on_state;
	struct platform_device *pdev;
	struct fl_dev ops;
	void *fl_data;
	struct mutex mutex_ops;
};

#define IOCTL_SET_LEVEL		_IOW('P',21,unsigned int)
#define IOCTL_SET_MODE		_IOW('P',22,unsigned int)
#define IOCTL_SET_ENABLE	_IOW('P',23,unsigned int)
#define IOCTL_GET_LEVEL		_IOWR('P',24,unsigned int)
#if defined(CONFIG_COMPAT)
#define IOCTL_COMPAT_SET_LEVEL		_IOW('P',21,compat_uint_t)
#define IOCTL_COMPAT_SET_MODE		_IOW('P',22,compat_uint_t)
#define IOCTL_COMPAT_SET_ENABLE		_IOW('P',23,compat_uint_t)
#define IOCTL_COMPAT_GET_LEVEL		_IOWR('P',24,compat_uint_t)
#endif


#endif
