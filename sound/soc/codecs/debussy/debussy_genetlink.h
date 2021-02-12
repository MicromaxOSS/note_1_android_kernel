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

#ifndef _DEBUSSY_GENETLINK_H
#define _DEBUSSY_GENETLINK_H

#define DEBUSSY_GENL_NAME               "IG_DRIVER_CTRL"
#define DEBUSSY_GENL_VERSION            (0x1)

/*
 * Commands sent from userspace
 * Not versioned. New commands should only be inserted at the enum's end
 * prior to __DEMO_CMD_MAX
 */

enum {
    DEBUSSY_CMD_UNSPEC = 0,         /* Reserved */
    DEBUSSY_CMD_ECHO,               /* user->kernel request/get-response */
    DEBUSSY_CMD_REPLY,              /* kernel->user event */
    DEBUSSY_CMD_NOTIFY,             /* kernel->user event */
    DEBUSSY_CMD_ENROLL_MOD_PUT,
    DEBUSSY_CMD_ENROLL_MOD_PUT_REPLY,
    DEBUSSY_CMD_ENROLL_MOD_GET,
    DEBUSSY_CMD_ENROLL_MOD_GET_REPLY,
    DEBUSSY_CMD_VAD_BUF_GET,
    DEBUSSY_CMD_VAD_BUF_GET_REPLY,

    DEBUSSY_CMD_MAX
};
#define DEBUSSY_CMD_MAX                 (DEBUSSY_CMD_MAX - 1)

enum {
    DEBUSSY_CMD_ATTR_UNSPEC = 0,
    DEBUSSY_CMD_ATTR_MESG,
    DEBUSSY_CMD_ATTR_DATA,
    DEBUSSY_CMD_ATTR_NOTIFY,
    DEBUSSY_CMD_ATTR_BINARY,
    DEBUSSY_CMD_ATTR_NESTED,

    DEBUSSY_CMD_ATTR_MAX
};

#define DEBUSSY_GENL_CMD_ATTR_MAX       (DEBUSSY_CMD_ATTR_MAX - 1)

#define REPLY_MSG_FROM_KERNEL           "Hello from kernel space!!!"
#define KWS_TRIGGERED_MESSAGE           "KWS Triggered"

//#define FOR_GENETLINK_MODULE_TEST

#ifndef FOR_GENETLINK_MODULE_TEST
extern int debussy_genetlink_init(void *prv_data);
extern void debussy_genetlink_exit(void);
#endif

extern int debussy_genetlink_multicast(char *msg, unsigned int count);

#endif /* _DEBUSSY_GENETLINK_H */
