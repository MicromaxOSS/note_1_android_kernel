/**
 * Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
 * 2019-02-22 File created.
 */

#include "fsm_public.h"
#include <linux/slab.h>

uint8_t *fw_cont = NULL;
uint32_t fw_size = 0;

static void fsm_firmware_inited(const struct firmware *cont, void *context)
{
    fsm_pdata_t *pdata = context;
    fsm_dev_t *fsm_dev = &pdata->fsm_dev;
    int ret = 0;

    pr_info("%s enter\n", __func__);
    if (!cont || !context) {
        pr_err("%s failed to read %s\n", __func__, fsm_dev->preset_name);
        return;
    }

    pr_info("%s loaded %s - size: %zu\n", __func__, fsm_dev->preset_name, cont->size);

    fw_cont = (uint8_t *)cont->data;
    fw_size = (uint32_t)cont->size;
    ret |= fsm_i2c_cmd(FSM_CMD_PARSE_FIRMW, 1);
    release_firmware(cont);
    if(ret)
    {
        pr_err("%s parse firmware failed, ret: %d.\n", __func__, ret);
    }

    pr_info("%s ret: %d\n", __func__, ret);
    if(ret == 0) {
        ret |= fsm_i2c_cmd(FSM_CMD_INIT, 1);
    }
    pr_info("%s firmware init complete!\n", __func__);
}

int fsm_init_firmware(fsm_pdata_t *pdata, int force)
{
    struct preset_file *pfile = g_presets_file;
    fsm_dev_t *fsm_dev = NULL;
    int ret = 0;

    pr_info("%s enter\n", __func__);

    if(pdata == NULL)
        return -EINVAL;

    fsm_dev = &pdata->fsm_dev;
    if(!force && (fsm_dev->dev_state & STATE(FW_INITED))) {
        return 0;
    } else if (force) {
        if (g_presets_file)
        {
            fsm_free_mem(g_presets_file);
            g_presets_file = NULL;
            fsm_dev->dev_list = NULL;
        }
        fsm_dev->dev_state &= ~(STATE(FW_INITED));
    }
    if (!force && pfile)
    {
        return fsm_init_dev_list(fsm_dev, pfile);
    }

    ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
            fsm_dev->preset_name, pdata->dev, GFP_KERNEL,
            pdata, fsm_firmware_inited);

    pr_info("%s exit, ret: %d\n", __func__, ret);
    return ret;
}

int fsm_init_firmware_sync(fsm_pdata_t *pdata, int force)
{
    struct preset_file *pfile = g_presets_file;
    const struct firmware *fsm_fw = NULL;
    fsm_dev_t *fsm_dev = NULL;
    int ret = 0;

    if(pdata == NULL)
        return -EINVAL;
    fsm_dev = &pdata->fsm_dev;
    if(!force && (fsm_dev->dev_state & STATE(FW_INITED)))
        return 0;
    else if(force)
    {
        if(g_presets_file)
        {
            fsm_free_mem(g_presets_file);
            g_presets_file = NULL;
            fsm_dev->dev_list = NULL;
        }
        fsm_dev->dev_state &= ~(STATE(FW_INITED));
    }
    if (pfile)
    {
        fsm_init_dev_list(fsm_dev, pfile);
        return fsm_init(fsm_dev, 0);
    }

    ret = request_firmware(&fsm_fw, fsm_dev->preset_name, pdata->dev);
	if(!ret)
	{
		fsm_firmware_inited(fsm_fw,pdata);
	}

    return ret;
}
