#include "sostorch.h"
#include <asm/errno.h>
#include "mt-plat/mtk_pwm.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>

struct aw3641_data {
	int flash_gpio;
	int en_gpio;
	int power_gpio;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pwm_state;
	struct pwm_spec_config pwm_a_config;
	int level;
	enum sostorch_mode mode_running;
	struct hrtimer timer_xhz;
	struct hrtimer timer_gap;
	uint8_t state_en;
	uint8_t state_in_gap;
	uint8_t state_freq;
	uint8_t state_sos_short;
	uint8_t state_sos_cnt;
	struct hrtimer timer_sos;
};

static int aw3641_exit(struct platform_device *pdev);

struct aw3641_data *aw3641;


/******************************************************************************
 * aw36402 operations
 *****************************************************************************/
static int pwm_on(int level){
	
	if (level == 0){
		mt_pwm_disable(aw3641->pwm_a_config.pwm_no,
						aw3641->pwm_a_config.pmic_pad);
		return 0;
	}
	
	aw3641->pwm_a_config.pwm_no = PWM1;
    aw3641->pwm_a_config.mode = PWM_MODE_OLD;
    aw3641->pwm_a_config.clk_src = PWM_CLK_OLD_MODE_BLOCK;//clksrc,PWM_CLK_OLD_MODE_BLOCK(52M),PWM_CLK_OLD_MODE_32K(32k)
    aw3641->pwm_a_config.clk_div = CLK_DIV4;//CLK_DIV1,CLK_DIV128
    aw3641->pwm_a_config.pmic_pad = false;
    aw3641->pwm_a_config.PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
    aw3641->pwm_a_config.PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
    aw3641->pwm_a_config.PWM_MODE_OLD_REGS.GDURATION = 0;
    aw3641->pwm_a_config.PWM_MODE_OLD_REGS.WAVE_NUM = 0;
    aw3641->pwm_a_config.PWM_MODE_OLD_REGS.DATA_WIDTH = 130;//16250;
	if (level == 1){
		aw3641->pwm_a_config.PWM_MODE_OLD_REGS.THRESH = 130*15/100;	//15% 300mA
	}else if (level == 2){
		aw3641->pwm_a_config.PWM_MODE_OLD_REGS.THRESH = 130*30/100;	//30% 600mA
	}else if (level == 3){
		aw3641->pwm_a_config.PWM_MODE_OLD_REGS.THRESH = 130*55/100;	//55% 700mA
	}else{
		aw3641->pwm_a_config.PWM_MODE_OLD_REGS.THRESH = 1;
	}
	
	pwm_set_spec_config(&aw3641->pwm_a_config);
	pr_info("pwn on thres %d\n",aw3641->pwm_a_config.PWM_MODE_OLD_REGS.THRESH);
	
	return 0;
 }

/******************************************************************************
 * flash work
 *****************************************************************************/
static enum hrtimer_restart timer_func_xhz(struct hrtimer *timer){
	ktime_t kt;
	
	if (aw3641->state_en){
		aw3641->state_en = 0;
		gpio_direction_output(aw3641->en_gpio,0);
	}else{
		aw3641->state_en = 1;
		gpio_direction_output(aw3641->en_gpio,1);
	}
	
	if (aw3641->state_in_gap){
		return HRTIMER_NORESTART;
	}else{
		if (aw3641->state_freq == 6){
			kt = ktime_set(0,1000*1000*1000/6/2);
		}else{
			kt = ktime_set(0,1000*1000*1000/15/2);
		}
		hrtimer_forward_now(timer, kt);
		return HRTIMER_RESTART;
	}
}
static enum hrtimer_restart timer_func_gap(struct hrtimer *timer){
	ktime_t kt;
	
	if (aw3641->state_in_gap){
		aw3641->state_in_gap = 0;
		kt = ktime_set(1,500*1000*1000);
		hrtimer_forward_now(timer, kt);
		
		//start flash
		if (aw3641->state_freq == 6){
			aw3641->state_freq = 15;
		}else{
			aw3641->state_freq = 6;
		}
		hrtimer_start(&aw3641->timer_xhz,ktime_set(0,1000*100),HRTIMER_MODE_REL);
	}else{
		aw3641->state_in_gap = 1;
		kt = ktime_set(0,500*1000*1000);
		hrtimer_cancel(&aw3641->timer_xhz);
		gpio_direction_output(aw3641->en_gpio,0);
		hrtimer_forward_now(timer, kt);
	}
	//pr_info("aw3641 %s: running at %dhz\n",__func__,aw3641->state_freq);
	return HRTIMER_RESTART;
}
static int start_flash_mode(){

	pr_info("%s+\n",__func__);
	aw3641->state_en = 0;
	aw3641->state_in_gap = 1;
	aw3641->state_freq = 6;
	
	hrtimer_start(&aw3641->timer_gap,ktime_set(0,1000*100),HRTIMER_MODE_REL);
	
	return 0;
}
static int stop_flash_mode(){
	
	pr_info("%s+\n",__func__);
	hrtimer_cancel(&aw3641->timer_gap);
	hrtimer_cancel(&aw3641->timer_xhz);
	aw3641->state_en = 1;
	aw3641->state_in_gap = 1;
	
	gpio_direction_output(aw3641->en_gpio,0);
	
	return 0;
}

/******************************************************************************
 * torch work
 *****************************************************************************/
static int start_torch_mode(){
	gpio_direction_output(aw3641->en_gpio,1);
	mdelay(2);
	return 0;
}
static int stop_torch_mode(){
	gpio_direction_output(aw3641->en_gpio,0);
	return 0;
}
/******************************************************************************
 * sos work
 *****************************************************************************/
 static enum hrtimer_restart timer_func_sos(struct hrtimer *timer){
	ktime_t kt;
	
	//printk("%s short %d,cnt %d\n",__func__,aw3641->state_sos_short,aw3641->state_sos_cnt);
	//state
	if (aw3641->state_sos_short){
		if (aw3641->state_sos_cnt >= 2*2-1){
			aw3641->state_en = 0;
			aw3641->state_sos_cnt = 0;
			aw3641->state_sos_short = 0;
		}else{
			aw3641->state_sos_cnt++;
		}
		kt = ktime_set(0,500*1000*1000);//500ms
	}else{
		if (aw3641->state_sos_cnt >= 3*2-1){
			aw3641->state_en = 0;
			aw3641->state_sos_cnt = 0;
			aw3641->state_sos_short = 1;
		}else{
			aw3641->state_sos_cnt++;
		}
		kt = ktime_set(1,500*1000*1000);//1500ms
	}
	//ctl gpio
	if (aw3641->state_en){
		aw3641->state_en = 0;
		gpio_direction_output(aw3641->en_gpio,1);
	}else{
		aw3641->state_en = 1;
		gpio_direction_output(aw3641->en_gpio,0);
	}
	//
	hrtimer_forward_now(timer, kt);
	return HRTIMER_RESTART;
}
 static int start_sos_mode(){

	pr_info("%s+\n",__func__);
	
	aw3641->state_en = 0;
	aw3641->state_sos_cnt = 0;
	aw3641->state_sos_short = 0;
	
	hrtimer_start(&aw3641->timer_sos,ktime_set(0,1000*500),HRTIMER_MODE_REL);
	
	return 0;
}
static int stop_sos_mode(){
	
	pr_info("%s+\n",__func__);
	
	hrtimer_cancel(&aw3641->timer_sos);
	gpio_direction_output(aw3641->en_gpio,0);
	return 0;
}

/* flashlight enable function */
static int aw3641_enable(struct sostorch_data *sostorch,int enable){
	int level = 0;
	enum sostorch_mode mode = 0;
	
	level = atomic_read(&sostorch->level);
	mode = atomic_read(&sostorch->mode);
	if (enable){
		if (aw3641->mode_running != MODE_NONE){
			switch (aw3641->mode_running) {
				case MODE_FLASH:
					stop_flash_mode();
					break;
				case MODE_TORCH:
					stop_torch_mode();
					break;
				case MODE_SOS:
					stop_sos_mode();
					break;
				default:
					pr_err("Unsupport mode\n");
			}
		
			gpio_direction_output(aw3641->en_gpio,0);
			gpio_direction_output(aw3641->power_gpio,0);
			pwm_on(0);
		}
		gpio_direction_output(aw3641->power_gpio,1);
		switch (mode) {
			case MODE_FLASH:
				start_flash_mode();
				break;
			case MODE_TORCH:
				start_torch_mode();
				break;
			case MODE_SOS:
				start_sos_mode();
				break;
			default:
				pr_err("Unsupport mode\n");
		}
		pwm_on(level);
		aw3641->mode_running = mode;
	}else{
		switch (mode) {
			case MODE_FLASH:
				stop_flash_mode();
				break;
			case MODE_TORCH:
				stop_torch_mode();
				break;
			case MODE_SOS:
				stop_sos_mode();
				break;
			default:
				pr_err("Unsupport mode\n");
		}
	
		gpio_direction_output(aw3641->en_gpio,0);
		gpio_direction_output(aw3641->power_gpio,0);
		pwm_on(0);
		aw3641->mode_running = MODE_NONE;
	}
	pr_info("mode %d,level %d,enable %d,running %d\n",mode,level,enable,aw3641->mode_running);
	return 0;
}


static int aw3641_parse_dt(struct platform_device *pdev){
	struct device_node *np = pdev->dev.of_node;
	struct sostorch_data *sostorch = dev_get_platdata(&pdev->dev);
	//struct aw3641_data *aw3641 = NULL;
	int ret = -ENOMEM;
	
	/* get pinctrl */
	aw3641->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(aw3641->pinctrl)) {
		pr_err("Failed to get flashlight pinctrl.\n");
		ret = PTR_ERR(aw3641->pinctrl);
		return ret;
	}
	aw3641->pwm_state = pinctrl_lookup_state(
			aw3641->pinctrl, "flash_pwm_a");
	if (IS_ERR(aw3641->pwm_state)) {
		pr_err("Failed to init (%s)\n", "flash_pwm_a");
		ret = PTR_ERR(aw3641->pwm_state);
	}
	
	aw3641->flash_gpio = of_get_named_gpio(np, "aw3641,flash-gpio", 0);
    if (aw3641->flash_gpio < 0) {
        pr_err("Unable to get \"aw3641,flash-gpio\"\n");
		return aw3641->flash_gpio;
    }else {
        pr_info("flash_gpio = %d\n", aw3641->flash_gpio);

        if (gpio_is_valid(aw3641->flash_gpio)) {
            if (0 == gpio_request(aw3641->flash_gpio, "aw3641_flash")) {
                //gpio_direction_output(dvdd_1v2_gpio, GPIO_HIGH);
            }else {
                pr_err("aw3641->flash_gpio: gpio_request fail\n");
            }
        }else {
            pr_err("aw3641->flash_gpio is invalid\n");
        }
    }
	
	aw3641->en_gpio = of_get_named_gpio(np, "aw3641,en-gpio", 0);
    if (aw3641->en_gpio < 0) {
        pr_err("Unable to get \"aw3641,en-gpio\"\n");
		return aw3641->en_gpio;
    }else {
        pr_info("en_gpio = %d\n", aw3641->en_gpio);

        if (gpio_is_valid(aw3641->en_gpio)) {
            if (0 == gpio_request(aw3641->en_gpio, "aw3641_en")) {
                //gpio_direction_output(dvdd_1v2_gpio, GPIO_HIGH);
            }else {
                pr_err("aw3641->en_gpio: gpio_request fail\n");
            }
        }else {
            pr_err("aw3641->en_gpio is invalid\n");
        }
    }
	
	aw3641->power_gpio = of_get_named_gpio(np, "aw3641,power-gpio", 0);
    if (aw3641->power_gpio < 0) {
        pr_err("Unable to get \"aw3641,power-gpio\"\n");
		return aw3641->power_gpio;
    }else {
        pr_info("power_gpio = %d\n", aw3641->power_gpio);

        if (gpio_is_valid(aw3641->power_gpio)) {
            if (0 == gpio_request(aw3641->power_gpio, "aw3641_power")) {
                //gpio_direction_output(dvdd_1v2_gpio, GPIO_HIGH);
            }else {
                pr_err("aw3641->power_gpio: gpio_request fail\n");
            }
        }else {
            pr_err("aw3641->power_gpio is invalid\n");
        }
    }
	
	sostorch->fl_data = aw3641;


	return 0;
}

int aw3641_init(struct platform_device *pdev){
	struct sostorch_data *sostorch = dev_get_platdata(&pdev->dev);
	int ret = -ENOMEM;
	struct pwm_spec_config *config;
	
	pr_info("aw3641 inti +\n");
	
	aw3641 = devm_kzalloc(&pdev->dev, sizeof(*aw3641), GFP_KERNEL);
	if (!aw3641) {
		ret = -ENOMEM;
		return ret;
	}
	
	ret = aw3641_parse_dt(pdev);
	if (ret){
		pr_err("aw3641 parse dt fail\n");
		goto err_free;
	}
	
	aw3641->level = 0;
	aw3641->mode_running = MODE_NONE;
	//pwm config
	config = &aw3641->pwm_a_config;
	config->pwm_no = PWM1;
    config->mode = PWM_MODE_OLD;
    config->clk_src = PWM_CLK_OLD_MODE_BLOCK;//clksrc,PWM_CLK_OLD_MODE_BLOCK(52M),PWM_CLK_OLD_MODE_32K(32k)
    config->clk_div = CLK_DIV4;//CLK_DIV1,CLK_DIV128
    config->pmic_pad = false;
	
    config->PWM_MODE_OLD_REGS.IDLE_VALUE = 0;
    config->PWM_MODE_OLD_REGS.GUARD_VALUE = 0;
    config->PWM_MODE_OLD_REGS.GDURATION = 0;
    config->PWM_MODE_OLD_REGS.WAVE_NUM = 0;
    config->PWM_MODE_OLD_REGS.DATA_WIDTH = 16250;//52m/64/16250=50k
    config->PWM_MODE_OLD_REGS.THRESH = 1; //high clk
	
	pinctrl_select_state(aw3641->pinctrl,aw3641->pwm_state);
	
	//timer
	hrtimer_init(&aw3641->timer_xhz, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3641->timer_xhz.function = timer_func_xhz;
	hrtimer_init(&aw3641->timer_gap, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3641->timer_gap.function = timer_func_gap;
	hrtimer_init(&aw3641->timer_sos, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw3641->timer_sos.function = timer_func_sos;
	
	//ops
	sostorch->ops.enable = aw3641_enable;
	sostorch->ops.exit = aw3641_exit;
	return ret;
err_free:
	devm_kfree(&pdev->dev,aw3641);
	return ret;
}

static int aw3641_exit(struct platform_device *pdev){
	return 0;
}
