/**
 * Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
 * 2018-10-17 File created.
 */
#include "fsm_public.h"
#include <linux/regmap.h>
#include <linux/i2c.h>


int fsm_regmap_write(struct regmap *map, uint8_t reg, uint16_t val)
{
	int retries = 0;
	int ret;

	if(map == NULL) {
		return -EINVAL;
	}

	do {
		ret = regmap_write(map, reg, val);
		if (ret < 0) {
			fsm_delay_ms(5);
			retries++;
		}
	} while(ret < 0 && retries < FSM_I2C_RETRY);
	if (ret < 0) {
		pr_err("write %02x i2c error: %d, retry: %d", reg, ret, retries);
	}
	//pr_debug("%02x<-%04x\n", reg, val);

	return ret;
}

int fsm_regmap_bulkwrite(struct regmap *map, uint8_t reg, uint8_t *pval, uint32_t len)
{
	int ret = 0;

	if(map == NULL) {
		return -EINVAL;
	}

	ret = regmap_bulk_write(map, reg, pval, len);

	return ret;
}

int fsm_regmap_read(struct regmap *map, uint8_t reg, uint16_t *pval)
{
	int retries = 0;
	unsigned int val;
	int ret;

	if(map == NULL) {
		return -EINVAL;
	}

	do {
		ret = regmap_read(map, reg, &val);
		if (ret < 0) {
			fsm_delay_ms(5);
			retries++;
		}
	} while(ret < 0 && retries < FSM_I2C_RETRY);
	if (ret < 0) {
		pr_err("read %02x i2c error: %d, retry: %d", reg, ret, retries);
		return ret;
	}
	if (pval) {
		*pval = (uint16_t)val;
	}
	//pr_debug("%02x->%04x\n", reg, val);

	return ret;
}

int fsm_regmap_bulkread(struct regmap *map, uint8_t reg, uint8_t *pval, uint32_t len)
{
	if(map == NULL) {
		return -EINVAL;
	}

	return regmap_bulk_read(map, reg, pval, len);
}

int fsm_regmap_update_bits(struct regmap *map, uint8_t reg, uint8_t mask, uint8_t val)
{
	if(map == NULL) {
		return -EINVAL;
	}

	return regmap_update_bits(map, reg, mask, val);
}

static bool fsm_writeable_register(struct device *dev, uint32_t reg)
{
	return 1;
}

static bool fsm_readable_register(struct device *dev, uint32_t reg)
{
	return 1;
}

static bool fsm_volatile_register(struct device *dev, uint32_t reg)
{
	return 1;
}

static const struct regmap_config fsm_i2c_regmap =
{
	.reg_bits = 8,
	.val_bits = 16,
	.max_register = 0xff,
	.writeable_reg = fsm_writeable_register,
	.readable_reg = fsm_readable_register,
	.volatile_reg = fsm_volatile_register,
	.cache_type = REGCACHE_NONE,
};

struct regmap *fsm_regmap_i2c_init(struct i2c_client *i2c)
{
	struct regmap *regmap;
	int ret;

	regmap = devm_regmap_init_i2c(i2c, &fsm_i2c_regmap);
	if(IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n", ret);
		return NULL;
	}

	return regmap;
}

int fsm_regmap_i2c_deinit(struct regmap *regmap)
{
	pr_debug("enter");
	//if(regmap) {
	//	regmap_exit(regmap);
	//}
	return 0;
}

