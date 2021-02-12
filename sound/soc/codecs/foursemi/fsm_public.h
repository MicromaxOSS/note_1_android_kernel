/**
 * Copyright (c)Foursemi Co., Ltd. 2018-2019. All rights reserved.
 * Description: Public Defination .
 * Author: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */
#include "fsm_dev.h"
#include <linux/firmware.h>

/*
 * module: fsm_regmap
 */
#if defined(CONFIG_FSM_REGMAP)
#include <linux/regmap.h>
int fsm_regmap_write(struct regmap *map, uint8_t reg, uint16_t val);
int fsm_regmap_bulkwrite(struct regmap *map, uint8_t reg, uint8_t *pval, uint32_t len);
int fsm_regmap_read(struct regmap *map, uint8_t reg, uint16_t *pval);
int fsm_regmap_bulkread(struct regmap *map, uint8_t reg, uint8_t *pval, uint32_t len);
int fsm_regmap_update_bits(struct regmap *map, uint8_t reg, uint8_t mask, uint8_t val);
struct regmap *fsm_regmap_i2c_init(struct i2c_client *i2c);
int fsm_regmap_i2c_deinit(struct regmap *map);
#endif
/*
 * module: fsm_i2c
 */
#if defined(CONFIG_FSM_I2C)
#include <linux/i2c.h>
void fsm_mutex_lock(void);
void fsm_mutex_unlock(void);
int fsm_check_device(void);
int fsm_i2c_cmd(int cmd, int argv);
int fsm_i2c_event(int event, int argv);
int fsm_dump_info(char *buf);
int fsm_get_zmdata_str(char *buf, int max_size);
int fsm_get_r25_str(char *buf, int max_size);
fsm_dev_t *fsm_get_device(void);
#if !defined(CONFIG_FSM_REGMAP)
int fsm_i2c_reg_read(struct i2c_client *i2c, uint8_t reg, uint16_t *pVal);
int fsm_i2c_reg_write(struct i2c_client *i2c, uint8_t reg, uint16_t val);
int fsm_i2c_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *val, uint32_t len);
#endif
struct i2c_client *fsm_get_i2c(uint8_t addr);
#endif

/*
 * module: fsm_misc
 */
#if defined(CONFIG_FSM_MISC)
int fsm_misc_init(void);
void fsm_misc_deinit(void);
#endif

/*
 * module: fsm_proc
 */
#if defined(CONFIG_FSM_PROC)
int fsm_proc_init(void);
int fsm_proc_deinit(void);
#endif

/*
 * module: fsm_firmware
 */
int fsm_init_firmware(fsm_pdata_t *pdata, int force);
int fsm_init_firmware_sync(fsm_pdata_t *pdata, int force);

/*
 * module: fsm_codec
 */
int fsm_codec_register(struct i2c_client *i2c);
int fsm_codec_unregister(struct i2c_client *i2c);

/*
 * module: fsm_core
 */
extern struct preset_file *g_presets_file;
extern uint8_t *fw_cont;
extern uint32_t fw_size;
fsm_config_t *fsm_get_config(void);
void fsm_delay_ms(uint32_t delay_ms);
void *fsm_alloc_mem(int size);
void fsm_free_mem(void *buf);
int fsm_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val);
int fsm_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pval);
int fsm_reg_wmask(fsm_dev_t *fsm_dev, struct reg_unit *reg);
int fsm_burst_write(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *val, int len);
int fsm_access_key(fsm_dev_t *fsm_dev, int access);
int fsm_init_dev_list(fsm_dev_t *fsm_dev, struct preset_file *pfile);
int fsm_parse_firmware(fsm_dev_t *fsm_dev, uint8_t *pdata, uint32_t size);
uint8_t count_bit0_number(uint8_t byte);
int fsm_probe(fsm_dev_t *fsm_dev);
void fsm_remove(fsm_dev_t *fsm_dev);
int fsm_calib_zmdata(fsm_dev_t *fsm_dev);
uint16_t fsm_get_zmdata(fsm_dev_t *fsm_dev);
int fsm_set_cmd(fsm_dev_t *fsm_dev, int cmd, int argv);
int fsm_init(fsm_dev_t *fsm_dev, int force);
void fsm_hal_clear_calib_data(fsm_dev_t *fsm_dev);

/*
 * module: fsm_hal
 */
int fsm_hal_open(void);
int fsm_hal_reg_read(fsm_dev_t *fsm_dev, uint16_t reg, uint16_t *val);
int fsm_hal_reg_write(fsm_dev_t *fsm_dev, uint16_t reg, uint16_t val);
int fsm_hal_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg, uint8_t *val, uint32_t len);
int fsm_hal_init_preset(fsm_dev_t *fsm_dev, int force);
void fsm_hal_close(void);

/*
* module: fsm i2c and fsm codec
*/
int fsm_sysfs_init(struct i2c_client *client);
int fsm_calibrate(int force);
void fs1603_ops(fsm_dev_t *fsm_dev);
void fsm_hal_clear_calib_data(fsm_dev_t *fsm_dev);
int fs1603_status_check(fsm_dev_t *fsm_dev);

