#ifndef __PRIZE_DUAL_CAM__
#define __PRIZE_DUAL_CAM__


#define HP_CMD_SOFT_RST	0x06
#define HP_CMD_READ_PT	0x10
#define HP_CMD_READ_P	0x30
#define HP_CMD_READ_T	0x32
#define HP_CMD_ANA_CAL	0x28

#define HP_CMD_ADC		0x40
#define HP_OSR_4096		0x00	//T65.6 PT131.1ms
#define HP_OSR_2048		0x04
#define HP_OSR_1024		0x08	
#define HP_OSR_512		0x0C	//T8.2	PT16.4ms
#define HP_OSR_256		0x10
#define HP_OSR_128		0x14	//T2.1	PT4.1ms
#define HP_CHNL_PT		0x00
#define HP_CHNL_T		0x02

struct hoperf_data{
	int dvdd28_gpio;
	int irq_gpio;
	int irq;
	struct device *dev;
};

#endif