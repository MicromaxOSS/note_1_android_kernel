/**
 * Copyright (c)Foursemi Co., Ltd. 2018-2019. All rights reserved.
 * Description: Function Defination .
 * Author: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */
#ifndef __FSM_PUBLIC_H__
#define __FSM_PUBLIC_H__

#include "fsm_dev.h"

#define MAX_PATH_NUM     4
#define PRESET_NAME      ".fsm"
#define COEF_NAME        ".coef"
#define DEAULT_FILE      "_DEFAULT"
#define VENDOR_NAME      "foursemi/"
#define PRESET_NAME_SIZE 256

enum test_type {
    FSM_UNKNOW_TEST = -1,
    FSM_MMI_TEST = 0,
    FSM_RT_TEST = 1,
};

enum coef_data {
    COEF_DATA0 = 0,
    COEF_DATA1,
    COEF_DATA2,
    COEF_DATA3,
    COEF_DATA4,
    COEF_DATA5,
};

int fsm_hal_open(void);
int fsm_hal_get_device(void);
int fsm_hal_set_srate(int srate);
int fsm_hal_set_bclk(int bclk);
int fsm_hal_set_scene(uint16_t scene);
int fsm_hal_init(int force);
int fsm_hal_calibrate(int force);
int fsm_hal_get_r25(uint32_t *re25_list);
int fsm_hal_spk_on(void);
int fsm_hal_f0_test(int mode);
int fsm_hal_spk_off(void);
int fsm_hal_get_f0(uint32_t *f0_list);
void fsm_hal_close(void);
int fsm_hal_check_calib_state(void);
int fsm_hal_dump_reg(void);
int fsm_hal_clear_calib_data(void);
int fsm_hal_select_device(uint8_t dev_mask);
int fsm_hal_set_bypass_flag(void);
char *fsm_hal_parse_file_name(char *platform_file_name, char *file_type);
int fsm_hal_get_file(char *component_name, char *file_type, int len);
char *fsm_hal_set_get_file_name(char *platform_file_name, char *file_type);
int fsm_hal_get_reg_value(uint16_t reg);
int fsm_hal_get_r0(uint32_t *r0_list);
#endif
