/**************************************************************************
*  dts example
*	hoperf@21 {
*		compatible = "prize,hoperf_r_1";
*		reg = <0x21>;
*		pdn_pin = <&pio 20 0>;
*		sensor_type = <0x32a>;
*	};
**************************************************************************/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/compat.h>

#include "hoperf.h"

static struct hoperf_data *phoperf;

/*******************************************************************************
* device file
*******************************************************************************/
static int hoperf_convert_once_lazy(struct i2c_client *client, int *p, int *t){
	int ret = 0;
	int32_t temp = 0;
	uint8_t data_buf[6] = {0};
	
	ret = i2c_smbus_write_byte(client,HP_CMD_ADC|HP_OSR_512|HP_CHNL_PT);
	if (ret){
		dev_err(&client->dev,"% fail %d\n",__func__,ret);
	}
	msleep(100);
	ret = i2c_smbus_read_i2c_block_data(client,HP_CMD_READ_PT,6,data_buf);
	if (ret){
		dev_info(&client->dev,"%s read 0x%02x%02x%02x%02x%02x%02x, ret %d\n",__func__,
				data_buf[0],data_buf[1],data_buf[2],data_buf[3],data_buf[4],data_buf[5],ret);
	}
	
	//pressure
	temp = (data_buf[3] & 0xf)<<16 | data_buf[4]<<8 | data_buf[5];
	*p = temp;	//pa

	//temperature
	temp = (data_buf[0] & 0xf)<<16 | data_buf[1]<<8 | data_buf[2];
	if (data_buf[0] & 0x08){
		temp = temp | 0xfff00000;
	}
	*t = temp * 10;	//C degree * 1000
	
	dev_info(&client->dev,"%s read p %d,t %d\n",__func__,*p,*t);
	
	return 0;
}

/*******************************************************************************
* device file
*******************************************************************************/

static ssize_t device_temperature_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	//struct hoperf_data *hoperf = i2c_get_clientdata(client);
	int pressure = 0, temperature = 0;
	int ret = 0;
	
	ret = hoperf_convert_once_lazy(client,&pressure,&temperature);
	if (ret){
		dev_err(&client->dev,"%s convert fail %d\n",__func__,ret);
		return sprintf(buf,"error\n");
	}

	return sprintf(buf, "%d\n", temperature);
}
static DEVICE_ATTR(temperature, S_IRUGO, device_temperature_show, NULL);

static ssize_t device_pressure_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	//struct hoperf_data *hoperf = i2c_get_clientdata(client);
	int temperature = 0, pressure = 0;
	int ret = 0;
	
	ret = hoperf_convert_once_lazy(client,&pressure,&temperature);
	if (ret){
		dev_err(&client->dev,"%s convert fail %d\n",__func__,ret);
		return sprintf(buf,"error\n");
	}

	return sprintf(buf, "%d\n", pressure);
}
static DEVICE_ATTR(pressure, S_IRUGO, device_pressure_show, NULL);

static ssize_t device_enable_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	//struct i2c_client *client = to_i2c_client(dev);
	//struct hoperf_data *hoperf = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", 0);
}
 static ssize_t device_enable_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
 {
	//struct i2c_client *client = to_i2c_client(dev);
	//struct hoperf_data *hoperf = i2c_get_clientdata(dev);
	
	int enable = 0;
	
	sscanf(buf,"%d",&enable);
	if (enable){
		
	}else{
		
	}
	return size;
 }
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP, device_enable_show, device_enable_store);

/*******************************************************************************
 * ioctl
 ******************************************************************************/
 #ifdef CONFIG_COMPAT
static long hoperf_compat_ioctl(struct file *file, unsigned int cmd,
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

	//switch (cmd) {
	//	case IOCTL_COMPAT_SET_LEVEL:
	//		ret = file->f_op->unlocked_ioctl(file, IOCTL_SET_LEVEL,
	//						 (unsigned long)data);
	//		break;
	//	case IOCTL_COMPAT_SET_MODE:
	//		ret = file->f_op->unlocked_ioctl(file, IOCTL_SET_MODE,
	//						 (unsigned long)data);
	//		break;
	//	case IOCTL_COMPAT_SET_ENABLE:
	//		ret = file->f_op->unlocked_ioctl(file, IOCTL_SET_ENABLE,
	//						 (unsigned long)data);
	//		break;
	//	case IOCTL_COMPAT_GET_LEVEL:
	//		ret = file->f_op->unlocked_ioctl(file, IOCTL_GET_LEVEL,
	//						 (unsigned long)data);
	//		break;
	//	default:
	//		pr_err("unsupport cmd %d\n",cmd);
	//}
	
	ret |= get_user(d, data);
	ret |= put_user(d, data32);
	return ret;
}
#endif

static long hoperf_ioctl(struct file *file, unsigned int cmd, 
					unsigned long arg){
	long ret = 0;
	uint32_t val = 0;
	
	get_user(val,(uint32_t __user *) arg);
	pr_info("cmd %d,arg %d\n",cmd,val);
	
	//mutex_lock(&hoperf->mutex_ops);
	//switch (cmd) {
	//	case IOCTL_SET_LEVEL:
	//		atomic_set(&hoperf->level,val);
	//		break;
	//	case IOCTL_SET_MODE:
	//		atomic_set(&hoperf->mode,val);
	//		break;
	//	case IOCTL_SET_ENABLE:
	//		ret = hoperf->ops.enable(hoperf,val);
	//		break;
	//	case IOCTL_GET_LEVEL:
	//		ret = put_user(atomic_read(&hoperf->level),(unsigned long *)arg);
	//		break;
	//	default:
	//		pr_err("unsupport cmd %d\n",cmd);
	//		ret = -EINVAL;
	//}
	//mutex_unlock(&hoperf->mutex_ops);
	return ret;
}

static int hoperf_open(struct inode *a_pstInode, struct file *a_pstFile){
	int ret = 0;
	pr_debug("%s start\n", __func__);
	return ret;
}

static int hoperf_release(struct inode *inode, struct file *file){
	return 0;
}

static ssize_t hoperf_write(struct file *file, const char __user *ubuf,
			      size_t count, loff_t *ppos){
	return 0;
}

static ssize_t hoperf_read(struct file *file, char __user *ubuf,
	size_t count, loff_t *ppos){
		
	struct i2c_client *client = to_i2c_client(phoperf->dev);
	//struct hoperf_data *hoperf = i2c_get_clientdata(client);
	int temperature = 0, pressure = 0;
	int ret = 0;
	char hoperf_buf[64];
	int cnt = 0;
	
	ret = hoperf_convert_once_lazy(client,&pressure,&temperature);
	if (ret){
		dev_err(&client->dev,"%s convert fail %d\n",__func__,ret);
		cnt = sprintf(hoperf_buf,"error");
	}else{
		cnt = sprintf(hoperf_buf, "%d",pressure);
	}
	
	return simple_read_from_buffer(ubuf, count, ppos, hoperf_buf, cnt);
}

static const struct file_operations hoperf_fops = {
	.read = hoperf_read,
	.write = hoperf_write,
	.open = hoperf_open,
	.release = hoperf_release,
	.unlocked_ioctl = hoperf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hoperf_compat_ioctl,
#endif
};

static dev_t g_devNum;
static struct cdev *g_charDrv;
static struct class *g_drvClass;
static inline int hoperf_chrdev_reg(void){
	struct device *device = NULL;
	int ret = 0;

	pr_debug("%s Start\n", __func__);

	if (alloc_chrdev_region(&g_devNum, 0, 1, "hoperf")) {
		pr_debug("Allocate device no failed\n");
		return -EAGAIN;
	}

	g_charDrv = cdev_alloc();
	if (g_charDrv == NULL) {
		unregister_chrdev_region(g_devNum, 1);
		pr_debug("Allocate mem for kobject failed\n");
		return -ENOMEM;
	}

	cdev_init(g_charDrv, &hoperf_fops);
	g_charDrv->owner = THIS_MODULE;

	if (cdev_add(g_charDrv, g_devNum, 1)) {
		pr_debug("Attatch file operation failed\n");
		unregister_chrdev_region(g_devNum, 1);
		return -EAGAIN;
	}

	g_drvClass = class_create(THIS_MODULE, "hoperf");
	if (IS_ERR(g_drvClass)) {
		ret = PTR_ERR(g_drvClass);
		pr_debug("Unable to create class, err = %d\n", ret);
		return ret;
	}
	device = device_create(g_drvClass, NULL, g_devNum, NULL,"hoperf");
	pr_debug("%s End\n", __func__);

	return 0;
}

static void hoperf_chrdev_unreg(void)
{
	/*Release char driver */
	class_destroy(g_drvClass);
	device_destroy(g_drvClass, g_devNum);
	cdev_del(g_charDrv);
	unregister_chrdev_region(g_devNum, 1);
}
/*******************************************************************************
* i2c
*******************************************************************************/
static const struct of_device_id __maybe_unused hoperf_of_match[] = {
	{ .compatible = "hoperf,hp5804",},
};
MODULE_DEVICE_TABLE(of, hoperf_of_match);

static const struct i2c_device_id hoperf_id_table[] = {
	{ "hp5804",},
};
MODULE_DEVICE_TABLE(i2c, hoperf_id_table);

static int hoperf_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id){
	
	struct hoperf_data *hoperf;
	struct device_node *np = NULL;
	int ret = 0;
	
	dev_info(&client->dev,"%s+",__func__);
	ret = i2c_smbus_write_byte(client,HP_CMD_SOFT_RST);
	if (ret){
		dev_err(&client->dev,"hoperf detect device fail %d\n",ret);
		//return ret;
	}	
	
	hoperf = devm_kzalloc(&client->dev, sizeof(*hoperf), GFP_KERNEL);
	if (!hoperf) {
		dev_info(&client->dev,"alloc memory fail\n");
		return -ENOMEM;
	}
	hoperf->dev = &client->dev;
	i2c_set_clientdata(client,hoperf);
	
	np = client->dev.of_node;
	if (!IS_ERR(np)){
		hoperf->irq_gpio = of_get_named_gpio(np,"hp,irq-gpio",0);
		if (gpio_is_valid(hoperf->irq_gpio)){
			dev_info(&client->dev,"hoperf irq_gpio(%d)\n",hoperf->irq_gpio);
			gpio_request(hoperf->irq_gpio,"hoperf_irq");
			gpio_direction_input(hoperf->irq_gpio);
			hoperf->irq = gpio_to_irq(hoperf->irq_gpio);
			dev_info(&client->dev,"hoperf irq(%d)\n",hoperf->irq);
		}else{
			dev_err(&client->dev,"hoperf get \"hp,irq-gpio\" fail\n");
			ret = -EINVAL;
			goto err_mem;
		}
		
		hoperf->dvdd28_gpio = of_get_named_gpio(np,"hp,dvdd28-gpio",0);
		if (gpio_is_valid(hoperf->dvdd28_gpio)){
			dev_info(&client->dev,"hoperf dvdd28_gpio(%d)\n",hoperf->dvdd28_gpio);
			gpio_request(hoperf->dvdd28_gpio,"hoperf_dvdd28");
		}else{
			dev_err(&client->dev,"hoperf get \"hp,dvdd28-gpio\" fail\n");
		}
		
		//dev_info(&client->dev,"\n");)
		//of_property_read_u32(node,"irq",&sensor_type);
		//hoperf_data->pdn_pin = of_get_named_gpio(node,"hp5804,dvdd28-gpio",0);
		//printk("hoperf hoperf_r_pdn_pin(%d)\n",hoperf_data->pdn_pin);
		//dev_info(&client->dev,"hoperf_r_pdn_pin(%d)\n",hoperf_data->pdn_pin);
		//if (gpio_is_valid(hoperf_data->pdn_pin)) {
        //    gpio_direction_output(hoperf_data->pdn_pin, 1);
        //}
		//gpio_request(hoperf_data->pdn_pin,"hp5804_dvdd28");
		//gpio_direction_output(hoperf_data->pdn_pin,1);
	}else{
		printk("hoperf get device node fail %s\n",client->name);
		ret = -EINVAL;
		goto err_mem;
	}
	
	//dev
	ret = hoperf_chrdev_reg();
	if (ret){
		pr_err(" device chrdev file level fail %d\n",ret);
		goto err_mem;
	}
	
	//create devfile
	ret = device_create_file(&client->dev, &dev_attr_temperature);
	if (ret){
		dev_err(&client->dev,"hoperf failed to create file tempature %d\n",ret);
		goto err_devfile;
	}
	ret = device_create_file(&client->dev, &dev_attr_pressure);
	if (ret){
		dev_err(&client->dev,"hoperf failed to create file pressure %d\n",ret);
		goto err_devfile;
	}
	ret = device_create_file(&client->dev, &dev_attr_enable);
	if (ret){
		dev_err(&client->dev,"hoperf failed to create file enable %d\n",ret);
		goto err_devfile;
	}
	
	phoperf = hoperf;

	return 0;

err_devfile:
	device_remove_file(&client->dev,&dev_attr_enable);
	device_remove_file(&client->dev,&dev_attr_pressure);
	device_remove_file(&client->dev,&dev_attr_temperature);
err_mem:
	devm_kfree(&client->dev,hoperf);
	return ret;
}

static int  hoperf_i2c_remove(struct i2c_client *client){
	//struct hoperf_data *hoperf = i2c_get_clientdata(client);
	
	dev_info(&client->dev,"%s+\n",__func__);
	
	hoperf_chrdev_unreg();
	
	device_remove_file(&client->dev, &dev_attr_enable);
	device_remove_file(&client->dev, &dev_attr_pressure);
	device_remove_file(&client->dev, &dev_attr_temperature);
	
	return 0;
}

static int __maybe_unused hoperf_i2c_suspend(struct device *dev){
	return 0;
}

static int __maybe_unused hoperf_i2c_resume(struct device *dev){
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops hoperf_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(hoperf_i2c_suspend, hoperf_i2c_resume)
};
#endif

static struct i2c_driver hoperf_driver = {
	.driver = {
		.name		= "hoperf",
		.owner		= THIS_MODULE,
		//.of_match_table	= of_match_ptr(sp_cam_of_match),
		.of_match_table	= hoperf_of_match,
	#ifdef CONFIG_PM_SLEEP
		.pm		= &hoperf_pm_ops,
	#endif
	},
	.probe		= hoperf_i2c_probe,
	.remove		= hoperf_i2c_remove,
	.id_table	= hoperf_id_table,
};

module_i2c_driver(hoperf_driver);

MODULE_LICENSE("GPL");

