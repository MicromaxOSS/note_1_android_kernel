#include <linux/string.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <asm-generic/gpio.h>


#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#endif


//extern int mt_dsi_get_gpio_info(struct platform_device *pdev);

struct pinctrl *prize_typec_pinctrl;
struct pinctrl_state *lp_sel_set_high,*prize_typec_default;
struct pinctrl_state *lp_sel_set_low,*mic_high,*mic_low;

static int prize_typec_probe(struct platform_device *pdev)
{
		int ret = 0;

	printk("prize_typec_probe begin\n");
	prize_typec_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(prize_typec_pinctrl)) {
		printk("Cannot find prize_typec_pinctrl!\n");
		ret = PTR_ERR(prize_typec_pinctrl);
	}
	//hall eint pin initialization
	prize_typec_default= pinctrl_lookup_state(prize_typec_pinctrl, "default");
	if (IS_ERR(prize_typec_default)) {
		ret = PTR_ERR(prize_typec_default);
		printk("%s : init err, prize_typec_default\n", __func__);
	}

	lp_sel_set_high = pinctrl_lookup_state(prize_typec_pinctrl, "lp_sel_set_high");
	if (IS_ERR(lp_sel_set_high)) {
		ret = PTR_ERR(lp_sel_set_high);
		printk("%s : init err, lp_sel_set_high\n", __func__);
	}
	else
			printk("%s : init ok, lp_sel_set_high\n", __func__);
	lp_sel_set_low = pinctrl_lookup_state(prize_typec_pinctrl, "lp_sel_set_low");
	if (IS_ERR(lp_sel_set_low)) {
		ret = PTR_ERR(lp_sel_set_low);
		printk("%s : init err, lp_sel_set_low\n", __func__);
	}
	else
		printk("%s : init ok, lp_sel_set_low \n", __func__);
	//else

#ifdef	CONFIG_TCPC_CLASS
    if(!IS_ERR(lp_sel_set_low))
 	    pinctrl_select_state(prize_typec_pinctrl,lp_sel_set_low);
#endif
	mic_high = pinctrl_lookup_state(prize_typec_pinctrl, "mic_high");
	if (IS_ERR(mic_high))
		pr_info("Can *NOT* find mic_high\n");
	else
		pr_info("Find mic_high\n");

	pr_info("Finish parsing pinctrl\n");

	mic_low = pinctrl_lookup_state(prize_typec_pinctrl, "mic_low");
	if (IS_ERR(mic_low))
		pr_info("Can *NOT* find mic_low\n");
	else
		pr_info("Find mic_low\n");

	if (!IS_ERR(mic_low))
		pinctrl_select_state(prize_typec_pinctrl,mic_low);

	mic_high = pinctrl_lookup_state(prize_typec_pinctrl, "mic_high");
	if (IS_ERR(mic_high))
		pr_info("Can *NOT* find mic_high\n");
	else
		pr_info("Find mic_high\n");

	return 0;
}

void typec_pinctrl_sel(int state)
{
			int ret = 0;
    if(prize_typec_pinctrl == NULL)
        return;
	if (state)
	{
			printk("hsl typec_pinctrl_sel lp_sel_set_high\n");
				lp_sel_set_high = pinctrl_lookup_state(prize_typec_pinctrl, "lp_sel_set_high");
					if (IS_ERR(lp_sel_set_high)) {
							ret = PTR_ERR(lp_sel_set_high);
							printk("%s : init err, lp_sel_set_high\n", __func__);
                            return;
					}
			pinctrl_select_state(prize_typec_pinctrl,lp_sel_set_high);
	}
	else
	{
			if (!IS_ERR(lp_sel_set_low))
			{
				printk("hsl typec_pinctrl_sel lp_sel_set_low\n");
				pinctrl_select_state(prize_typec_pinctrl,lp_sel_set_low);
		 }
		 else
		 {
		 		printk("hsl typec_pinctrl_sel lp_sel_set_low fail\n");
		 }
	}
}

void typec_pinctrl_mic(int state)
{
			int ret = 0;
    if(prize_typec_pinctrl == NULL)
        return;
	if (state)
	{
			printk("hsl typec_pinctrl_mic lp_mic_set_high\n");
			mic_high = pinctrl_lookup_state(prize_typec_pinctrl, "mic_high");
					if (IS_ERR(mic_high)) {
							ret = PTR_ERR(mic_high);
							printk("%s : init err, mic_high\n", __func__);
                            return;
					}
			if (!IS_ERR(mic_high)) {
				pinctrl_select_state(prize_typec_pinctrl,mic_high);
			}
	}
	else
	{
			if (!IS_ERR(mic_low))
			{
				printk("hsl typec_pinctrl_sel mic_low\n");
				pinctrl_select_state(prize_typec_pinctrl,mic_low);
		 }
		 else
		 {
		 		printk("hsl typec_pinctrl_sel mic_low fail\n");
		 }
	}
}

#if defined(CONFIG_PRIZE_SWITCH_SGM3798_SUPPORT)
void typec_pinctrl_mic_reverse(void){
	static int sel_pin_state = 0;
    if(prize_typec_pinctrl == NULL)
        return;
	if (sel_pin_state){
		if (!IS_ERR(mic_low)) {
			pinctrl_select_state(prize_typec_pinctrl,mic_low);
			sel_pin_state = 0;
		}
	}else{
		if (!IS_ERR(mic_high)){
			pinctrl_select_state(prize_typec_pinctrl,mic_high);
			sel_pin_state = 1;
		}
	}
}
#endif

static const struct of_device_id prize_typec_of_ids[] = {
	{.compatible = "mediatek,prize_typec",},
	{}
};

static struct platform_driver prize_typec_driver = {
	.driver = {
		   .name = "prize_typec",
	#ifdef CONFIG_OF
		   .of_match_table = prize_typec_of_ids,
	#endif
		   },
	.probe = prize_typec_probe,
};


static int __init prize_typec_init(void)
{
	printk("hushilun prize_typec init\n");
	if (platform_driver_register(&prize_typec_driver) != 0) {
		printk("unable to register prize_typec_driver.\n");
		return -1;
	}
	else
	{
				printk("register prize_typec_driver ok.\n");
	}
	return 0;
}

static void __exit prize_typec_exit(void)
{
	platform_driver_unregister(&prize_typec_driver);

}
module_init(prize_typec_init);
module_exit(prize_typec_exit);

MODULE_LICENSE("PRIZE_TYPEC");
MODULE_DESCRIPTION("prize typec driver");
MODULE_AUTHOR("hushilun<hushilun@szprize.com>");
