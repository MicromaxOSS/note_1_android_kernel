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

#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include "debussy_intf.h"
#include "debussy.h"
#include "debussy_snd_ctrl.h"
#include "debussy_customer.h"

#define IGO_TIMEOUT_TICKS (2 * HZ)      // sec
#define ENABLE_MASK_CONFIG_CMD
#define ENABLE_CMD_BATCH_MODE_BUFFERED
//#define ENABLE_REFERENCE_COUNT

#define HW_RESET_INSTEAD_OF_STANDBY

#if (DEBUSSY_ERR_RECOVER_LV == 2)
extern DEBUSSY_CMD_BACKUP_t  debussyCmdBackup;
#endif

#ifdef ENABLE_CMD_BATCH_MODE_BUFFERED
#define MAX_BATCH_MODE_CMD_NUM          (32)
static atomic_t batchMode = ATOMIC_INIT(0);
static atomic_t batchCmdNum = ATOMIC_INIT(0);
static unsigned int batch_cmd_buffer[64];
#endif

int igo_ch_chk_done(struct device* dev)
{
    struct i2c_client* client;
    unsigned int status = IGO_CH_STATUS_CMD_RDY;
    unsigned long start_time = jiffies;
    client = i2c_verify_client(dev);

    while (status == IGO_CH_STATUS_NOP || status == IGO_CH_STATUS_BUSY || status == IGO_CH_STATUS_CMD_RDY || status > IGO_CH_STATUS_MAX) {
        usleep_range(50, 50);
        igo_i2c_read(client, IGO_CH_STATUS_ADDR, &status);

        if (jiffies - start_time >= IGO_TIMEOUT_TICKS) {
            dev_err(dev, "igo cmd timeout\n");
            return IGO_CH_STATUS_TIMEOUT;
        }
    }

    return status;
}

int igo_ch_write_wait(struct device* dev, unsigned int reg, unsigned int data, unsigned int wait_time)
{
    struct i2c_client* client;
    unsigned int cmd[2];
    unsigned int status = IGO_CH_STATUS_CMD_RDY;

    client = i2c_verify_client(dev);
    igo_i2c_write(client, IGO_CH_BUF_ADDR, data);
    cmd[0] = IGO_CH_STATUS_CMD_RDY;
    cmd[1] = (IGO_CH_ACTION_WRITE << IGO_CH_ACTION_OFFSET) + reg;
    igo_i2c_write(client, IGO_CH_CMD_ADDR, cmd[1]);
    igo_i2c_write(client, IGO_CH_STATUS_ADDR, cmd[0]);

    if (wait_time) {
        if (wait_time < 20) {
            wait_time *= 1000;
            usleep_range(wait_time - 1, wait_time);
        }
        else {
            msleep(wait_time);
        }
    }

    status = igo_ch_chk_done(dev);

    if (status != IGO_CH_STATUS_DONE) {
        dev_err(dev, "igo cmd write 0x%08x : 0x%08x fail, error no : %d\n", reg, data, status);

#if (DEBUSSY_ERR_RECOVER_LV == 2)
	if (status == IGO_CH_STATUS_TIMEOUT) {
		dev_info(dev, "%s: cmd not done!! trigger check. \n", __func__);
		debussyCmdBackup.bitmap |= 0x80;
	}
#endif
    }

    return status;
}

int igo_ch_write(struct device* dev, unsigned int reg, unsigned int data)
{
    struct debussy_priv *debussy = i2c_get_clientdata(i2c_verify_client(dev));
    unsigned int noDelayAfterWrite = 0; 
    int status = 0;
#if (SEND_VAD_CMD_IN_DRIVER==1)
    unsigned int vadModeStage = atomic_read(&debussy->vad_mode_stage);
#endif

#if (DEBUSSY_ERR_RECOVER_LV == 2)
    if ((debussyCmdBackup.bitmap&0x40) == 0)
    {    
        static unsigned int prev_cmd = 0xFF, prev_val = 0xFF;
		static DEBUSSY_IGO_CMD_t   backup[32];
		static unsigned char in_idx = 0, bitmap = 0;

        if (reg == IGO_CH_POWER_MODE_ADDR)
        {
            if (data == POWER_MODE_STANDBY && bitmap == 0)
            {
                prev_cmd = reg;
                prev_val = data;
	            //dev_info(debussy->dev, "~~~%s - find bomb !!\n", __func__);
            }
            else if (data == POWER_MODE_WORKING && prev_val == POWER_MODE_STANDBY)
            {
                backup[0].cmd = prev_cmd;
                backup[0].val = prev_val;
                backup[1].cmd = reg;
                backup[1].val = data;
                in_idx = 2;
                bitmap |= 0x03;
				//if (debussyCmdBackup.active == SYS_GUARD_ALIVE_HB)
	            //    dev_info(debussy->dev, "~~~%s - trigger backup !!\n", __func__);
            }
        }
        else if (prev_val == POWER_MODE_STANDBY && bitmap == 0x03)
        {
            backup[in_idx].cmd = reg;
            backup[in_idx].val = data;

            //dev_info(debussy->dev, "~~~%s - backup[%d] : cmd=0x%x, val=0x%x !!\n", __func__, in_idx,
            //                backup[in_idx].cmd, backup[in_idx].val);
            in_idx++;   

            if (reg == IGO_CH_OP_MODE_ADDR)
            {
                prev_cmd = prev_val = 0xFF;
                bitmap |= 0x04;
            }

			if ((bitmap&0x07) == 0x07)
			{
				bitmap = 0;
				if ((debussyCmdBackup.bitmap&0x08) == 0)
				{
					memcpy(debussyCmdBackup.backup[!debussyCmdBackup.re_send_idx], backup, sizeof(DEBUSSY_IGO_CMD_t) * in_idx);
					debussyCmdBackup.in_idx[!debussyCmdBackup.re_send_idx] = in_idx;
                    debussyCmdBackup.re_send_idx = !debussyCmdBackup.re_send_idx;
					debussyCmdBackup.bitmap |= 0x07;
					atomic_set(&debussy->reset_stage, RESET_STAGE_EXEC_RESET);
					dev_info(debussy->dev, "%s - backup[%d] complete !! switch op mode, reset_stage = %d", __func__, 
                                debussyCmdBackup.re_send_idx, atomic_read(&debussy->reset_stage));
				}
				else
				{
					dev_info(debussy->dev, "%s - backup give up !! backup cmd is re-sending, reset_stage = %d", __func__, atomic_read(&debussy->reset_stage));
				}
			}
        }      
    }
#endif  /* end of DEBUSSY_ERR_RECOVER_LV */

    switch (reg) {
    case IGO_CH_HIF_CALI_EN_ADDR:
        return igo_ch_write_wait(dev, reg, data, noDelayAfterWrite);
        break;

    case IGO_CH_POWER_MODE_ADDR:
		if (POWER_MODE_WORKING == data && atomic_read(&debussy->reset_stage) == RESET_STAGE_SKIP_RESET)
		{
			atomic_set(&debussy->reset_stage, RESET_STAGE_EXEC_RESET);
			dev_info(debussy->dev, "%s - switch to working mode, reset_stage = %d", __func__, atomic_read(&debussy->reset_stage));
		}
#ifdef ENABLE_MASK_CONFIG_CMD
        if (POWER_MODE_WORKING == data) {
            atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_KEEP_ALIVE);
        }
        else {
            dev_info(dev, "%s: POWER_MODE_STANDBY: Enable mask configure setting\n", __func__);
            dev_info(dev, "%s: POWER_MODE_STANDBY: ref = %d\n", __func__, atomic_read(&debussy->referenceCount));

    #ifdef ENABLE_REFERENCE_COUNT
            if (atomic_read(&debussy->referenceCount)) {
                return IGO_CH_STATUS_DONE;
            }
    #endif
        }
#endif

#ifdef ENABLE_CMD_BATCH_MODE_BUFFERED
        if (IGO_CH_POWER_MODE_ADDR == reg) {
            atomic_set(&batchCmdNum, 0);

            if (POWER_MODE_WORKING == data) {
                atomic_set(&batchMode, 1);
                dev_info(dev, "%s: Enable batch CMD mode\n", __func__);
            }
            else {
                // POWER_MODE_STANDBY
                atomic_set(&batchMode, 0);
                dev_info(dev, "%s: Disable batch CMD mode\n", __func__);
            }
        }
#endif
        break;

    //case IGO_CH_OP_MODE_ADDR:
    default:
#ifdef ENABLE_MASK_CONFIG_CMD
        //if (atomic_read(&debussy->maskConfigCmd)) {
        //    return IGO_CH_STATUS_DONE;
        //}

        if (IGO_CH_OP_MODE_ADDR == reg) {
            if (OP_MODE_CONFIG == data) {
                // debussy_shutdown is ignored, change to Standby CMD
                reg = IGO_CH_POWER_MODE_ADDR;
                data = POWER_MODE_STANDBY;
                break;
            }
        }
#endif

#ifdef ENABLE_CMD_BATCH_MODE_BUFFERED
        if (atomic_read(&batchMode)) {
            int status = IGO_CH_STATUS_DONE;
            unsigned int index = atomic_read(&batchCmdNum) << 1;

            batch_cmd_buffer[index] = reg;
            batch_cmd_buffer[index + 1] = data;
            atomic_add(1, &batchCmdNum);

            if ((atomic_read(&batchCmdNum) >= MAX_BATCH_MODE_CMD_NUM) || (IGO_CH_OP_MODE_ADDR == reg)) {
                igo_ch_batch_write(dev, 0, batch_cmd_buffer, atomic_read(&batchCmdNum) << 1);
                status = igo_ch_batch_finish_write(dev, atomic_read(&batchCmdNum));
                atomic_set(&batchCmdNum, 0);
                dev_info(dev, "%s: igo_ch_batch_finish_write ret = %d\n", __func__,status);
#if (SEND_VAD_CMD_IN_DRIVER==1)
                if((IGO_CH_OP_MODE_ADDR == reg)&&(IGO_CH_STATUS_DONE == status))
                {
                    atomic_set(&debussy->curr_op_mode, data);   
                    dev_info(dev, "%s: prev vadModeStage = %d, data=%d\n", __func__,vadModeStage, data);
                    if (OP_MODE_VAD == data || OP_MODE_LPVAD == data)
                    {
                        atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_ENABLE);                    
                        vadModeStage = VAD_MODE_ST_ENABLE;
                    }
                    else if (OP_MODE_BYPASS == data)
                    {
                        if (debussy->voice_mode == InGoogleVoice)
                        {
                            atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_BYPASS);                    
                            vadModeStage = VAD_MODE_ST_BYPASS;
                        }
                        else
                        {
                            atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_OTHER_MODE);
                            vadModeStage = VAD_MODE_ST_OTHER_MODE;
                        }
                    }
                    else
                    {
                        atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_KEEP_ALIVE);
                        atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_OTHER_MODE);
                        vadModeStage = VAD_MODE_ST_OTHER_MODE;
                    }
                    dev_info(dev, "%s:vadModeStage = %d, reg= %d ,data = %d ,status = %d \n", __func__,
                        vadModeStage, reg,data,status);
                }
#endif
                if (IGO_CH_OP_MODE_ADDR == reg) {
                    atomic_set(&batchMode, 0);
                    dev_info(dev, "%s: IGO_CH_OP_MODE_ADDR: Disable batch mode\n", __func__);
                }
            }
            else {
                //dev_info(dev, "%s: BatchMode without write \n", __func__);
            }

#if (DEBUSSY_ERR_RECOVER_LV == 2)
			if (status == IGO_CH_STATUS_TIMEOUT)
			{
				dev_info(dev, "%s: batch write not complete!! trigger check. \n", __func__);
				debussyCmdBackup.bitmap |= 0x80;
			}
#endif
            
            return status;
        }
#endif
        break;
    }

    if (IGO_CH_POWER_MODE_ADDR == reg) {
        if (POWER_MODE_STANDBY == data) {
#ifdef HW_RESET_INSTEAD_OF_STANDBY
            if (atomic_read(&debussy->pull_down_state) == PULL_DOWN_ST_PULL_DOWN)
            {
                debussy->chip_pull_up(debussy->dev);
            }
            else
            {
                dev_info(debussy->dev, "%s:reset_stage=%d\n", __func__, atomic_read(&debussy->reset_stage));
                debussy->reset_chip(dev, 1);
                atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_CNT_DOWN);
            }
#if (SEND_VAD_CMD_IN_DRIVER==1)
            dev_info(dev, "%s:POWER_MODE_STANDBY : vadModeStage=%d\n", __func__, vadModeStage); 
            if (VAD_MODE_ST_OTHER_MODE == vadModeStage)
            {
                atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_DISABLE);    
                vadModeStage = VAD_MODE_ST_DISABLE;
            }
#endif
            return IGO_CH_STATUS_DONE;
#endif
        }

        noDelayAfterWrite = 20;
        dev_info(dev, "%s: POWER_MODE_WORKING: set noDelayAfterWrite = %d \n", __func__,noDelayAfterWrite);
    }

    status = igo_ch_write_wait(dev, reg, data, noDelayAfterWrite);
#if (SEND_VAD_CMD_IN_DRIVER==1)
    if((IGO_CH_OP_MODE_ADDR == reg)&&(IGO_CH_STATUS_DONE == status))
    {
        atomic_set(&debussy->curr_op_mode, data);   
        dev_info(dev, "%s: prev vadModeStage = %d, data=%d\n", __func__,vadModeStage, data);
        if (OP_MODE_VAD == data || OP_MODE_LPVAD == data)
        {
            atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_ENABLE);   
            vadModeStage = VAD_MODE_ST_ENABLE;
        }
        else if (OP_MODE_BYPASS == data)
        {
            if (VAD_MODE_ST_ENABLE == vadModeStage)
            {
                atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_BYPASS);                    
                vadModeStage = VAD_MODE_ST_BYPASS;
            }
            else
            {
                atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_OTHER_MODE);
                vadModeStage = VAD_MODE_ST_OTHER_MODE;
            }
        }
        else
        {
            atomic_set(&debussy->pull_down_state, PULL_DOWN_ST_KEEP_ALIVE);
            atomic_set(&debussy->vad_mode_stage, VAD_MODE_ST_OTHER_MODE);
            vadModeStage = VAD_MODE_ST_OTHER_MODE;
        }
        dev_info(dev, "%s:vadModeStage = %d, reg= %d ,data = %d ,status = %d \n", __func__,
            vadModeStage, reg,data,status);
    }
#endif
    
    return status;
}

int igo_ch_read(struct device* dev, unsigned int reg, unsigned int* data)
{
    struct i2c_client* client;
    unsigned int cmd[2];
    unsigned int status = IGO_CH_STATUS_CMD_RDY;
    int retval = 0;

    client = i2c_verify_client(dev);

    cmd[0] = status;
    cmd[1] = (IGO_CH_ACTION_READ << IGO_CH_ACTION_OFFSET) + reg;
    retval |= igo_i2c_write(client, IGO_CH_CMD_ADDR, cmd[1]);
    retval |= igo_i2c_write(client, IGO_CH_STATUS_ADDR, cmd[0]);

    if (0 == retval) {
        status = igo_ch_chk_done(dev);
    
        igo_i2c_read(client, IGO_CH_BUF_ADDR, data);
    
        if (status != IGO_CH_STATUS_DONE) {
            dev_err(dev, "igo cmd read 0x%08x fail, error no : %d\n", reg, status);
            *data = 0;
#if (DEBUSSY_ERR_RECOVER_LV == 2)
            if (status == IGO_CH_STATUS_TIMEOUT)
            {
                dev_info(dev, "%s:  trigger check. \n", __func__);
                debussyCmdBackup.bitmap |= 0x80;
            }
#endif
        }
    }
    else {
        dev_err(dev, "igo cmd I2C Write fail, error no : %d\n", reg);
        status = IGO_CH_STATUS_BUSY;
        *data = 0;
    }
         
    return status;
}

#ifdef ENABLE_CMD_BATCH_MODE_BUFFERED
int igo_ch_batch_write(struct device* dev, unsigned int cmd_index, unsigned int *data, unsigned int data_length)
{
    struct i2c_client* client;

    client = i2c_verify_client(dev);
    return igo_i2c_write_buffer(client, IGO_CH_BUF_ADDR + (cmd_index << 3), data, data_length);
}

int igo_ch_batch_finish_write(struct device* dev, unsigned int num_of_cmd)
{
    struct i2c_client* client;
    unsigned int cmd[2];
    unsigned int status = IGO_CH_STATUS_CMD_RDY;

    client = i2c_verify_client(dev);
    cmd[0] = IGO_CH_STATUS_CMD_RDY;
    cmd[1] = (IGO_CH_ACTION_BATCH << IGO_CH_ACTION_OFFSET) + num_of_cmd;
    igo_i2c_write(client, IGO_CH_CMD_ADDR, cmd[1]);
    igo_i2c_write(client, IGO_CH_STATUS_ADDR, cmd[0]);

    status = igo_ch_chk_done(dev);

    if (status != IGO_CH_STATUS_DONE) {
        dev_err(dev, "igo batch cmd write fail, error no : %d\n", status);
        igo_i2c_read(client, IGO_CH_RSV_ADDR, &cmd[0]);
        dev_err(dev, "igo cmd batch write fail: error bit 0x%08X\n", cmd[0]);

#if (DEBUSSY_ERR_RECOVER_LV == 2)
		if (status == IGO_CH_STATUS_TIMEOUT)
		{
			dev_info(dev, "%s: cmd not done!! trigger check. \n", __func__);
			debussyCmdBackup.bitmap |= 0x80;
		}
#endif
    }

    return status;
}
#endif

int igo_ch_buf_write(struct device* dev, unsigned int addr, unsigned int *data, unsigned int word_len) {
    struct i2c_client* client;
    unsigned int cmd[2];
    unsigned int status = IGO_CH_STATUS_CMD_RDY;
    unsigned int writeSize, writeSize_bytes, offset = 0;

    client = i2c_verify_client(dev);

    while (word_len) {
        writeSize = (word_len >= (IG_BUF_RW_LEN >> 2)) ? (IG_BUF_RW_LEN >> 2) : word_len;
        writeSize_bytes = writeSize << 2;
        igo_i2c_write_buffer(client, IGO_CH_BUF_ADDR, &data[offset], writeSize);

        cmd[0] = IGO_CH_STATUS_CMD_RDY;
        cmd[1] = (IGO_CH_ACTION_WRITE << IGO_CH_ACTION_OFFSET) + addr;
        igo_i2c_write(client, IGO_CH_CMD_ADDR, cmd[1]);
        igo_i2c_write(client, IGO_CH_OPT_ADDR, writeSize_bytes);
        igo_i2c_write(client, IGO_CH_STATUS_ADDR, cmd[0]);

        status = igo_ch_chk_done(dev);

        if (status != IGO_CH_STATUS_DONE) {
            dev_err(dev, "igo buffer write fail, error no : %d\n", status);

#if (DEBUSSY_ERR_RECOVER_LV == 2)
			if (status == IGO_CH_STATUS_TIMEOUT)
			{
				dev_info(dev, "%s: cmd not done!! trigger check. \n", __func__);
				debussyCmdBackup.bitmap |= 0x80;
			}
#endif

            return status;
        }

        addr += writeSize << 2;
        word_len -= writeSize;
        offset += writeSize;
    }

    return status;
}

int igo_ch_buf_read(struct device* dev, unsigned int addr, unsigned int *data, unsigned int word_len) {
    struct i2c_client* client = i2c_verify_client(dev);
    unsigned int cmd[2];
    unsigned int status = IGO_CH_STATUS_CMD_RDY;
    unsigned int read_size, read_size_byte, offset = 0;
    struct debussy_priv* debussy = i2c_get_clientdata(client);

    while (word_len) {
        read_size = (word_len >= (IG_BUF_RW_LEN >> 2)) ? (IG_BUF_RW_LEN >> 2) : word_len;
        read_size_byte = read_size << 2;

        cmd[0] = IGO_CH_STATUS_CMD_RDY;
        cmd[1] = (IGO_CH_ACTION_READ << IGO_CH_ACTION_OFFSET) + addr;

        if (debussy->spi_dev) {
            if (0 != igo_spi_write(IGO_CH_CMD_ADDR, cmd[1])) {
                igo_spi_intf_enable(1);
                igo_spi_write(IGO_CH_CMD_ADDR, cmd[1]);
                igo_spi_write(IGO_CH_OPT_ADDR, read_size_byte);
                igo_spi_write(IGO_CH_STATUS_ADDR, cmd[0]);
            }
            else {
                igo_spi_write(IGO_CH_CMD_ADDR, cmd[1]);
                igo_spi_write(IGO_CH_OPT_ADDR, read_size_byte);
                igo_spi_write(IGO_CH_STATUS_ADDR, cmd[0]);
            }
        }
        else {
            igo_i2c_write(client, IGO_CH_CMD_ADDR, cmd[1]);
            igo_i2c_write(client, IGO_CH_OPT_ADDR, read_size_byte);
            igo_i2c_write(client, IGO_CH_STATUS_ADDR, cmd[0]);
        }

        status = igo_ch_chk_done(dev);

        if (status != IGO_CH_STATUS_DONE) {
            dev_err(dev, "igo buffer read fail, error no : %d\n", status);

#if (DEBUSSY_ERR_RECOVER_LV == 2)
			if (status == IGO_CH_STATUS_TIMEOUT)
			{
				dev_info(dev, "%s: cmd not done!! trigger check. \n", __func__);
				debussyCmdBackup.bitmap |= 0x80;
			}
#endif

            return status;
        }

        if (debussy->spi_dev) {
            if(igo_spi_read_buffer(IGO_CH_BUF_ADDR, &data[offset], read_size)!=0){
              pr_err("debussy: %s: call igo_spi_read_buffer() error!\n", __func__);
			}
        }
        else {
            if(igo_i2c_read_buffer(client, IGO_CH_BUF_ADDR, &data[offset], read_size)!=0){
              pr_err("debussy: %s: call igo_i2c_read_buffer() error!\n", __func__);
			}
        }

        word_len -= read_size;
        offset += read_size;
        addr += read_size << 2;
    }

    return status;
}
