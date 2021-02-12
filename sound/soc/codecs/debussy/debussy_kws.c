/*
 * Copyright (C) 2017-2019 Intelligo Technology Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "debussy_config.h"

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#if DTS_SUPPORT
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#endif  /* end of DTS_SUPPORT */
#include <linux/input.h> 
#include <linux/delay.h>

#include "debussy.h"
#include "debussy_intf.h"
#include "debussy_snd_ctrl.h"
#ifdef ENABLE_GENETLINK
#include "debussy_genetlink.h"
#endif
#include "debussy_customer.h"

#define KWS_WS_TIMEOUT_MS 4000
static int32_t gpio_pin, gpio_kws_deb;
//static u32 kws_irq;
static struct wakeup_source kws_ws;
extern int reset_chip_is_processing;

static const struct of_device_id kws_of_match[] = {
    { .compatible = "intelligo,debussy", },
    {},
};

static struct {
    struct debussy_priv* debussy;
} debussy_kws_priv;

static void debussy_kws_irq_work(struct work_struct* work)
{
    struct debussy_priv* debussy;
    uint32_t status = 0;

    debussy = container_of(work, struct debussy_priv, irq_work);
    dev_info(debussy->dev, "%s add wake timeout\n", __func__);
	__pm_wakeup_event(&kws_ws, KWS_WS_TIMEOUT_MS);
#if SOUND_TRIGGER_STAGE == 2    
    igo_ch_read(debussy->dev, IGO_CH_VAD_STATUS_ADDR, &status);
#endif
    if (VAD_STATUS_HW_VAD_TRIGGERED == status) {
        atomic_inc(&debussy->vad_count);
        dev_info(debussy->dev, "@@@@@@ debussy->vad_count = %d @@@@@@\n", atomic_read(&debussy->vad_count));

        return;
    }

    atomic_set(&debussy->kws_triggered, 1);
    atomic_inc(&debussy->kws_count);
    debussyInfo.s2_hit_ctr++;
    debussy->vad_buf_loaded = false;
    dev_info(debussy->dev, "####### debussy->kws_count = %d #######\n", atomic_read(&debussy->kws_count));

#ifdef ENABLE_GENETLINK
    {
        char s[80];
    
        sprintf(s,"%s%d","GENL:VAD_TRIGGERED debussy->kws_count = ",atomic_read(&debussy->kws_count));
        dev_info(debussy->dev, "Send: debussy_genetlink_multicast\n");
        debussy_genetlink_multicast(s, atomic_read(&debussy->kws_count));
#if SOUND_TRIGGER_STAGE == 2    
        queue_work(debussy->debussy_wq, &debussy->vadbuf_chk_work);
#endif
    }
#endif

    // Add Customer Code to wakeup AP
    debussy_kws_hit(debussy);
}

#if SOUND_TRIGGER_STAGE == 2    
static void debussy_vadbuf_chk_work(struct work_struct* work)
{
    struct debussy_priv* debussy;
    uint32_t status = 0;

    debussy = container_of(work, struct debussy_priv, vadbuf_chk_work);

    status = 10;
    while (--status) {
        if (debussy->vad_buf_loaded) {
            dev_info(debussy->dev, "%s: vad_buf is loaded by user, exit!\n", __func__);
            break;
        }
        msleep(50);
    }
#ifdef ENABLE_GENETLINK
    {
        char s[80];

        if ((status == 0)&& !debussy->vad_buf_loaded) {
            dev_info(debussy->dev, "%s: vad_buf is NOT loaded by user, Multicast Again!\n", __func__);
            sprintf(s,"%s%d","GENL:VAD_TRIGGERED debussy->kws_count = ",atomic_read(&debussy->kws_count));
            debussy_genetlink_multicast(s, atomic_read(&debussy->kws_count));
        }
    }
#endif
}
#endif  /* end of SOUND_TRIGGER_STAGE */

static irqreturn_t debussy_eint_handler(int irq, void *data)
{
    struct debussy_priv* debussy = debussy_kws_priv.debussy;

	if (!reset_chip_is_processing){
    dev_info(debussy->dev, "%s -\n", __func__);
    queue_work(debussy->debussy_wq, &debussy->irq_work);
		debussyInfo.s1_hit_ctr++;
	}

    return IRQ_HANDLED;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,13,11)
static int debussy_request_input_dev(struct debussy_priv* debussy)
{
    int ret = -1;
  
    dev_info(debussy->dev, "%s(+)", __func__);
  
    debussy->input_dev = input_allocate_device();
    if (debussy->input_dev == NULL)
    {
        dev_err(debussy->dev, "%s: Failed to allocate input device,ret:%d\n", __func__, ret);
        return -ENOMEM;
    }

    debussy->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) ;

    debussy->input_dev->keybit[BIT_WORD(SOUND_TRIGGER_KEY)] = BIT_MASK(SOUND_TRIGGER_KEY);

    __set_bit(INPUT_PROP_DIRECT, debussy->input_dev->propbit);


    debussy->input_dev->name = "intelligo-debussy";
    debussy->input_dev->phys = "input/debussy";
    debussy->input_dev->id.bustype = BUS_I2C;
    debussy->input_dev->id.vendor = 0xDEAD;
    debussy->input_dev->id.product = 0xBEEF;
    debussy->input_dev->id.version = 10427;
    
    ret = input_register_device(debussy->input_dev);
    if (ret)
    {
        dev_err(debussy->dev, "%s: Register %s input device failed,ret:%d\n", __func__, debussy->input_dev->name, ret);
        return -ENODEV;
    }

    return 0;
}
#endif

int debussy_kws_init(struct debussy_priv* debussy)
{
    int ret;
    unsigned long kws_irqflags = IRQF_TRIGGER_RISING;
#if DTS_SUPPORT
    struct device_node   *node = NULL;
#endif

    debussy_kws_priv.debussy = debussy;

    INIT_WORK(&debussy->irq_work, debussy_kws_irq_work);
#if SOUND_TRIGGER_STAGE == 2
    INIT_WORK(&debussy->vadbuf_chk_work, debussy_vadbuf_chk_work);
#endif

#if DTS_SUPPORT
    node = of_find_matching_node(node, kws_of_match);
    if (!node) {
        dev_err(debussy->dev, "%s: there is no this node\n", __func__);
        return -1;
    }

    gpio_pin = of_get_named_gpio(node, "ig,deb-gpios", 0);
    if (gpio_pin < 0) {
        dev_err(debussy->dev, "Unable to get \"ig,deb-gpios\"\n");
        gpio_pin = IGO_KWS_INT;
    }
    else {
        dev_info(debussy->dev, "%s: kws gpio pin found:%d\n", __func__, gpio_pin);
    }

    ret = of_property_read_u32(node, "ig,debounce", &gpio_kws_deb);
    if (ret) {
        dev_err(debussy->dev, "%s: gpio debounce not found,ret:%d\n", __func__, ret);
        gpio_kws_deb = 5;
    }
    else {
        dev_info(debussy->dev, "%s: gpio debounce found:%d\n", __func__, gpio_kws_deb);
    }
#else
    gpio_pin = debussyDtsReplace.gpio_pin;
    gpio_kws_deb = debussyDtsReplace.gpio_kws_deb;
#endif  /* end of DTS_SUPPORT */

    if (gpio_pin >= 0) {
#if XP_ENV == 1        
        printk("FRK gpio_pin = %d", gpio_pin);
        if(gpio_request(gpio_pin, "igo_kws") == 0)
            printk("FRK request OK\n");
        else
            printk("FRK request fail\n");
        gpio_direction_input(gpio_pin); 
#endif
        gpio_set_debounce(gpio_pin, gpio_kws_deb);
        debussy->kws_irq = gpio_to_irq(gpio_pin);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,13,11)
        ret = debussy_request_input_dev(debussy);
        if (ret < 0)
        {
            dev_err(debussy->dev,"%s: debussy request input dev failed, ret:%d.\n", __func__, ret);
            return ret;
        }
#endif

        ret = request_irq(debussy->kws_irq, debussy_eint_handler, kws_irqflags, "debussy-kws-eint", NULL);
        if (ret) {
            dev_err(debussy->dev, "%s: request_irq fail, ret:%d.\n", __func__, ret);
            return ret;
        }

        /*ret = fsfuncirq_set_irq_wake(kws_irq, 1);*/
        ret = enable_irq_wake(debussy->kws_irq);
        if (ret) {
            dev_err(debussy->dev, "%s: set_irq_wake fail, ret:%d.\n", __func__, ret);
            return ret;
        }
		/* ADD wake up source */
		wakeup_source_init(&kws_ws, "debussy_ksw_ws");

        dev_info(debussy->dev, "%s: set gpio EINT finished, irq=%d, gpio_headset_deb=%d\n", __func__, debussy->kws_irq, gpio_kws_deb);
	}
    return 0;
}
