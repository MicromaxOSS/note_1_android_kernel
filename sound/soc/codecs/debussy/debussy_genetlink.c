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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <net/genetlink.h>

#include "debussy.h"
#ifdef ENABLE_GENETLINK
#include "debussy_genetlink.h"

#ifdef FOR_GENETLINK_MODULE_TEST
    #define ENABLE_BC_TESET_TIMER

    #ifdef ENABLE_BC_TESET_TIMER
    #define ENABLE_BC_TESET_TIMER_PERIOD        (5000)
    static struct timer_list timer;
    #endif

#else

    #include "debussy.h"
    #include "debussy_intf.h"
    #include "debussy_snd_ctrl.h"

    static struct debussy_priv* debussy = NULL;
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
// For Android 5.1
static struct genl_multicast_group debussy_genl_mcgrp = {
    .name = "DEBUSSY_GRP"
};
#else
static struct genl_multicast_group debussy_genl_mcgrp[] = {
    {
        .name = "DEBUSSY_GRP",
    },
};
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,9,256)
    // NOP
#else
    #define GENL_ID_GENERATE  0
#endif

static struct genl_family debussy_family = {
    .id             = GENL_ID_GENERATE,
    .hdrsize        = 0,
    .name           = DEBUSSY_GENL_NAME,
    .version        = DEBUSSY_GENL_VERSION,
    .maxattr        = DEBUSSY_GENL_CMD_ATTR_MAX,
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    #else
    .mcgrps         = debussy_genl_mcgrp,
    .n_mcgrps       = ARRAY_SIZE(debussy_genl_mcgrp),
    #endif
};

static const struct nla_policy debussy_cmd_policy[DEBUSSY_GENL_CMD_ATTR_MAX + 1] = {
    [DEBUSSY_CMD_ATTR_MESG] = { .type = NLA_STRING },
    [DEBUSSY_CMD_ATTR_DATA] = { .type = NLA_S32 },
    [DEBUSSY_CMD_ATTR_NOTIFY] = { .type = NLA_STRING },
    [DEBUSSY_CMD_ATTR_BINARY] = { .type = NLA_BINARY },
    [DEBUSSY_CMD_ATTR_NESTED] = { .type = NLA_NESTED },
};

static uint32_t enroll_data_size = 1024;
static uint32_t vad_data_size = 1024;

static int debussy_prepare_reply(struct genl_info *info, u8 cmd, struct sk_buff **skbp, size_t size)
{
    struct sk_buff *skb;
    void *reply;

    if (!info)
        return -EINVAL;

    /*
     * If new attributes are added, please revisit this allocation
     */
    skb = genlmsg_new(size, GFP_ATOMIC);
    if (!skb)
        return -ENOMEM;

    /* 构建回发消息头 */
    reply = genlmsg_put_reply(skb, info, &debussy_family, 0, cmd);
    //reply = genlmsg_put(skb, 0, 0, info, &debussy_family, 0, cmd);
    if (reply == NULL) {
        nlmsg_free(skb);
        return -EINVAL;
    }

    *skbp = skb;
    return 0;
}

static int debussy_mk_reply(struct sk_buff *skb, int attrtype, void *data, int len)
{
    /* add a netlink attribute to a socket buffer */
    return nla_put(skb, attrtype, len, data);

    /*
     * nla是可以分level的(调用nla_nest_start和nla_nest_end进行)，
     * 由于本示例程序attr较少，所以就不进行分层了，分层具体可见
     * 内核中其他使用到genetlink的地方。
     */
}

static int debussy_send_reply(struct sk_buff *skb, struct genl_info *info)
{
    struct genlmsghdr *genlhdr = nlmsg_data(nlmsg_hdr(skb));
    void *reply = genlmsg_data(genlhdr);

    genlmsg_end(skb, reply);

    return genlmsg_reply(skb, info);
}

#ifdef FOR_GENETLINK_MODULE_TEST
static char recv_enroll_data[1024];
#endif

static int cmd_attr_echo_message(struct genl_info *info, u32 replyCmd)
{
    struct nlattr *na;
    char *msg, *replyMsg;
    struct sk_buff *rep_skb;
    size_t size;
    int ret;
    uint32_t count;

    /* 读取用户下发的消息 */
    na = info->attrs[DEBUSSY_CMD_ATTR_MESG];
    if (!na)
        return -EINVAL;

    /* 回发消息 */
    switch (replyCmd) {
    case DEBUSSY_CMD_ENROLL_MOD_GET_REPLY:
        pr_info("debussy: %s: DEBUSSY_CMD_ENROLL_MOD_GET_REPLY -\n", __func__);

        replyMsg = (char *) kzalloc(enroll_data_size + 1, GFP_ATOMIC);

        if (NULL == replyMsg) {
            pr_err("debussy: %s: alloc fail\n", __func__);
            return -EFAULT;
        }

#ifdef FOR_GENETLINK_MODULE_TEST
        memcpy(replyMsg, recv_enroll_data, enroll_data_size);
#else
        mutex_lock(&debussy->igo_ch_lock);
        igo_ch_buf_read(debussy->dev, IGO_CH_VAD_ENROLL_MD_ADDR, (u32 *) replyMsg, enroll_data_size >> 2);

        {
            // For Debug Only
            //u32 *buf = (u32 *) replyMsg;

            /*for (count = 0; count < enroll_data_size / 4; count++) {
                printk(KERN_INFO "debussy: [igo-mdl 0x%08X] 0x%08X\n", count << 2, buf[count]);
            }*/
        }

        mutex_unlock(&debussy->igo_ch_lock);
#endif

        size = nla_total_size(enroll_data_size + 1);
        break;

    case DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY:
        msg = (char *) nla_data(na);
        pr_info("debussy: %s: DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY -\n", __func__);

#ifdef FOR_GENETLINK_MODULE_TEST
        {
            u32 *buf = (u32 *) msg;

            memcpy(recv_enroll_data, msg, enroll_data_size);
            for (count = 0; count < (enroll_data_size >> 2); count++) {
                printk(KERN_INFO "debussy: [igo-mdl 0x%08X] 0x%08X\n", count << 2, buf[count]);
            }
        }
#else
        mutex_lock(&debussy->igo_ch_lock);
        igo_ch_write(debussy->dev, IGO_CH_VAD_ENROLL_APPLY_ADDR, VAD_ENROLL_APPLY_DISABLE);
        igo_ch_buf_write(debussy->dev, IGO_CH_VAD_ENROLL_MD_ADDR, (u32 *) msg, enroll_data_size >> 2);

        {
            // For Debug Only
            /*u32 *buf = (u32 *) msg;

            for (count = 0; count < (enroll_data_size >> 2); count++) {
                pr_info("debussy: [igo-mdl 0x%08X] 0x%08X\n", count << 2, buf[count]);
            }*/
        }

        igo_ch_write(debussy->dev, IGO_CH_VAD_ENROLL_APPLY_ADDR, VAD_ENROLL_APPLY_APLLY);
        igo_ch_read(debussy->dev, IGO_CH_VAD_ENROLL_CNT_ADDR, &count);
        pr_info("debussy: %s: IGO_CH_VAD_ENROLL_CNT_ADDR - %d\n", __func__, count);
        mutex_unlock(&debussy->igo_ch_lock);
#endif

        replyMsg = (char *) kzalloc(strlen("DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY") + 1, GFP_ATOMIC);
        memset(replyMsg, 0, strlen("DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY") + 1);
        memcpy(replyMsg, "DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY", strlen("DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY"));

        size = nla_total_size(strlen(replyMsg)+1);
        break;

    case DEBUSSY_CMD_VAD_BUF_GET_REPLY:
       // pr_info("debussy: %s: DEBUSSY_CMD_VAD_BUF_GET_REPLY -\n", __func__);

        replyMsg = (char *) kzalloc(vad_data_size + 1, GFP_ATOMIC );

        if (NULL == replyMsg) {
            pr_err("debussy: %s: alloc fail\n", __func__);
            return -EFAULT;
        }

#ifdef FOR_GENETLINK_MODULE_TEST
        // NOP
        for (count = 0; count < vad_data_size; count++) {
            replyMsg[i] = i;
        }
#else
        mutex_lock(&debussy->igo_ch_lock);

        for (count = 0; count < 4; count++) {
            igo_ch_buf_read(debussy->dev, IGO_CH_VAD_BUF_ADDR,
                            (u32 *) &replyMsg[count * 256], 256 >> 2);
        }

        {
            // For Debug Only
            // u32* buf = (u32*)replyMsg;

            // for (count = 0; count < (vad_data_size >> 2); count++) {
            // printk(KERN_INFO "debussy: [vad data 0x%08X] 0x%08X\n", count << 2, buf[count]);
            // }
        }

        mutex_unlock(&debussy->igo_ch_lock);
#endif

        size = nla_total_size(vad_data_size + 1);
		debussy->vad_buf_loaded = true;
       pr_info("debussy: %s: DEBUSSY_CMD_VAD_BUF_GET_REPLY done-\n", __func__);
        break;

    //case DEBUSSY_CMD_REPLY:
    default:
        msg = (char *)nla_data(na);
        pr_info("debussy: generic netlink receive echo mesg: %s\n", msg);

        replyMsg = (char *) kzalloc(strlen(REPLY_MSG_FROM_KERNEL) + 1, GFP_ATOMIC);
        memset(replyMsg, 0, strlen(REPLY_MSG_FROM_KERNEL) + 1);
        memcpy(replyMsg, REPLY_MSG_FROM_KERNEL, strlen(REPLY_MSG_FROM_KERNEL));

        size = nla_total_size(strlen(replyMsg) + 1);
        break;
    }

    /* 准备构建消息 */
    ret = debussy_prepare_reply(info, replyCmd, &rep_skb, size);
    if (ret < 0) {
        kfree(replyMsg);

        return ret;
    }

    /* 填充消息 */
    ret = debussy_mk_reply(rep_skb, DEBUSSY_CMD_ATTR_MESG, replyMsg, size);
    kfree(replyMsg);

    if (ret < 0)
        goto err;

    /* 完成构建并发送 */
    return debussy_send_reply(rep_skb, info);

err:
    nlmsg_free(rep_skb);
    return ret;
}

static int debussy_echo_cmd(struct sk_buff *skb, struct genl_info *info)
{
    if (info->attrs[DEBUSSY_CMD_ATTR_MESG])
        return cmd_attr_echo_message(info, DEBUSSY_CMD_REPLY);
    else
        return -EINVAL;
}

static int debussy_enroll_mod_get_cmd(struct sk_buff *skb, struct genl_info *info)
{
    if (info->attrs[DEBUSSY_CMD_ATTR_MESG])
        return cmd_attr_echo_message(info, DEBUSSY_CMD_ENROLL_MOD_GET_REPLY);
    else
        return -EINVAL;
}

static int debussy_enroll_mod_put_cmd(struct sk_buff *skb, struct genl_info *info)
{
    if (info->attrs[DEBUSSY_CMD_ATTR_MESG])
        return cmd_attr_echo_message(info, DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY);
    else
        return -EINVAL;
}

static int debussy_vad_buf_get_cmd(struct sk_buff *skb, struct genl_info *info)
{
    if (info->attrs[DEBUSSY_CMD_ATTR_MESG])
        return cmd_attr_echo_message(info, DEBUSSY_CMD_VAD_BUF_GET_REPLY);
    else
        return -EINVAL;
}

int debussy_genetlink_multicast(char *msg, unsigned int count)
{
    struct sk_buff *skb = NULL;
    void *msg_header = NULL;
    int size;
    int rc;

    /* allocate memory */
    pr_info("debussy: %s - %d, %s\n", __func__, count, msg);

    size = nla_total_size(strlen(msg) + 1) + nla_total_size(0);

    skb = genlmsg_new(size, GFP_ATOMIC);
    if (!skb)
        return -ENOMEM;

    /* add the genetlink message header */
    msg_header = genlmsg_put(skb, 0, 0, &debussy_family, 0, DEBUSSY_CMD_NOTIFY);
    if (!msg_header) {
        rc = -ENOMEM;
        goto err_out;
    }

    /* add a DEBUSSY_CMD_ATTR_NOTIFY attribute */
    //rc = nla_put_string(skb, DEBUSSY_CMD_ATTR_NOTIFY, msg);
    rc = nla_put(skb, DEBUSSY_CMD_ATTR_NOTIFY, nla_total_size(strlen(msg) + 1), msg);
    if (rc != 0)
        goto err_out;

    /* finalize the message */
    genlmsg_end(skb, msg_header);

    //multicast is send a message to a logical group
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    rc = genlmsg_multicast(skb, 0, debussy_genl_mcgrp.id, GFP_ATOMIC);
    #else
    rc = genlmsg_multicast(&debussy_family, skb, 0, 0, GFP_ATOMIC);
    #endif

    if (rc != 0 && rc != -ESRCH) {
        /* if NO one is waitting the message in user space,
         * genlmsg_multicast return -ESRCH
         */
        pr_err("debussy: genlmsg_multicast to user failed, return %d\n", rc);

        /*
         * attention:
         * If you NOT call genlmsg_unicast/genlmsg_multicast and error occurs,
         * call nlmsg_free(skb).
         * But if you call genlmsg_unicast/genlmsg_multicast, NO need to call
         * nlmsg_free(skb). If NOT, kernel crash.
         */
        return rc;
    }

    pr_info("genlmsg_multicast Success");

    /*
     * Attention:
     * Should NOT call nlmsg_free(skb) here. If NOT, kernel crash!!!
     */
    return 0;

err_out:
    if (skb) {
        nlmsg_free(skb);
    }

    return rc;
}

static struct genl_ops debussy_genl_ops[] = {
    {
        .cmd            = DEBUSSY_CMD_ECHO,
        .doit           = debussy_echo_cmd,
        .policy         = debussy_cmd_policy,
        .flags          = 0,
    },
    {
        .cmd            = DEBUSSY_CMD_ENROLL_MOD_GET,
        .doit           = debussy_enroll_mod_get_cmd,
        .policy         = debussy_cmd_policy,
        .flags          = 0,
    },
    {
        .cmd            = DEBUSSY_CMD_ENROLL_MOD_PUT,
        .doit           = debussy_enroll_mod_put_cmd,
        .policy         = debussy_cmd_policy,
        .flags          = 0,
    },
    {
        .cmd            = DEBUSSY_CMD_VAD_BUF_GET,
        .doit           = debussy_vad_buf_get_cmd,
        .policy         = debussy_cmd_policy,
        .flags          = 0,
    },
};

#ifdef ENABLE_BC_TESET_TIMER
static void test_netlink_send(unsigned long count) {
    debussy_genetlink_multicast("BC: Hello from kernel space!!!", 0);
    mod_timer(&timer, jiffies + msecs_to_jiffies(ENABLE_BC_TESET_TIMER_PERIOD));
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,00)
// NOP
#else
static inline int
genl_register_family_with_ops_grps(struct genl_family *family,
                                    const struct genl_ops *ops, size_t n_ops,
                                    const struct genl_multicast_group *mcgrps,
                                    size_t n_mcgrps)
{
        family->module = THIS_MODULE;
        family->ops = ops;
        family->n_ops = n_ops;
        family->mcgrps = mcgrps;
        family->n_mcgrps = n_mcgrps;
        return genl_register_family(family);
}

#define genl_register_family_with_ops(family, ops)                      \
        genl_register_family_with_ops_grps((family),                    \
                                            (ops), ARRAY_SIZE(ops),     \
                                            NULL, 0)
#define genl_register_family_with_ops_groups(family, ops, grps) \
        genl_register_family_with_ops_grps((family),                    \
                                            (ops), ARRAY_SIZE(ops),     \
                                            (grps), ARRAY_SIZE(grps))
#endif

#ifdef FOR_GENETLINK_MODULE_TEST
static int __init debussy_genetlink_init(void)
#else
int debussy_genetlink_init(void *prv_data)
#endif
{
    int ret;

    #if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    ret = genl_register_family_with_ops(&debussy_family, debussy_genl_ops, ARRAY_SIZE(debussy_genl_ops));
    ret = genl_register_mc_group(&debussy_family, &debussy_genl_mcgrp);
    #else
    ret = genl_register_family_with_ops_groups(&debussy_family, debussy_genl_ops, debussy_genl_mcgrp);
    //ret = genl_register_family_with_ops(&debussy_family, debussy_genl_ops);
    #endif

    if (ret != 0) {
        pr_err("debussy: failed to init generic netlink example module\n");
        return ret;
    }

#ifndef FOR_GENETLINK_MODULE_TEST
    debussy = (struct debussy_priv *) prv_data;
#endif

    pr_info("debussy: generic netlink module init success\n");

#ifdef ENABLE_BC_TESET_TIMER
    init_timer(&timer);
    timer.function = test_netlink_send;
    timer.expires = 0;
    timer.data = 0;

    mod_timer(&timer, jiffies + msecs_to_jiffies(ENABLE_BC_TESET_TIMER_PERIOD));
#endif

    return 0;
}

#ifdef FOR_GENETLINK_MODULE_TEST
static void __exit debussy_genetlink_exit(void)
#else
void debussy_genetlink_exit(void)
#endif
{
    #ifdef ENABLE_BC_TESET_TIMER
    del_timer_sync(&timer);
    #endif

    #if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    genl_unregister_mc_group(&debussy_family, &debussy_genl_mcgrp);
    #endif
    genl_unregister_family(&debussy_family);
#ifndef FOR_GENETLINK_MODULE_TEST
    debussy = NULL;
#endif

    pr_info("debussy: test generic netlink module exit successful\n");
}

#ifdef FOR_GENETLINK_MODULE_TEST
module_init(debussy_genetlink_init);
module_exit(debussy_genetlink_exit);

MODULE_AUTHOR("debussy");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("generic netlink test module");
#endif

#endif  /* end of ENABLE_GENETLINK */

