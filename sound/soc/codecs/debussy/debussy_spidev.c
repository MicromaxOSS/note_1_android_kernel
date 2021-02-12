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

#include "debussy_config.h"

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/uaccess.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/input.h>        /* BUS_SPI */
#include <linux/regmap.h>

#include "debussy.h"
#ifdef ENABLE_SPI_INTF
#include "debussy_intf.h"

//#define ENABLE_SPI_DRIVER_DEBUG

#define DEF_REG_ADDR        (0x2A000000)
#define DEF_SPI_MAX_SPEED   (24000000)

static void _endian_swap(unsigned int *target, unsigned int *source, unsigned int word_len) {
    unsigned int i;
    unsigned int temp;

    for (i = 0; i < word_len; i++) {
        temp =  (source[i] >> 24) & 0x000000FF;
        temp += (source[i] >>  8) & 0x0000FF00;
        temp += (source[i] <<  8) & 0x00FF0000;
        temp += (source[i] << 24) & 0xFF000000;
        target[i] = temp;
    }
}

#ifdef ENABLE_DEBUSSY_SPI_REGMAP
static bool debussy_readable(struct device *dev, unsigned int reg)
{
    return true;
}

static bool debussy_writeable(struct device *dev, unsigned int reg)
{
    return true;
}

static bool debussy_volatile(struct device *dev, unsigned int reg)
{
    return true;
}

static bool debussy_precious(struct device *dev, unsigned int reg)
{
    return false;
}

const struct regmap_config debussy_spi_regmap = {
    .reg_bits = 32,
    .reg_stride = 4,
    .val_bits = 32,

    .cache_type = REGCACHE_NONE,
    .reg_format_endian = REGMAP_ENDIAN_BIG,
    #if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    .val_format_endian = REGMAP_ENDIAN_NATIVE,
    #else
    .val_format_endian = REGMAP_ENDIAN_LITTLE,
    #endif

    .max_register = 0xFFFFFFFF,
    .readable_reg = debussy_readable,
    .writeable_reg = debussy_writeable,
    .volatile_reg = debussy_volatile,
    .precious_reg = debussy_precious,
};
#endif

struct debussy_spi_priv {
    struct regmap           *spi_regmap;
    dev_t                   devt;
    spinlock_t              spi_lock;
    struct spi_device       *spi;
    struct list_head        device_entry;

    /* TX/RX buffers are NULL unless this device is open (users > 0) */
    struct mutex            buf_lock;
    unsigned int                users;
    u8                      *tx_buffer;
    u8                      *rx_buffer;
    u32                     speed_hz;

    uint32_t                reg_address;
    struct debussy_priv     *debussy;
    uint8_t                 isLittleEndian;
    uint32_t                enable_spi;
};

/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
static int SPIDEV_MAJOR = 0;
#define N_SPI_MINORS                    32      /* ... up to 256 */

static DECLARE_BITMAP(minors, N_SPI_MINORS);

/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *      is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
// For Android 5.1
#define SPI_MODE_MASK           (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                                | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
                                | SPI_NO_CS | SPI_READY)
#else
#define SPI_MODE_MASK           (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                                | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
                                | SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
                                | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)
#endif

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned int TX_BUF_SIZE = (4 * 1024);
module_param(TX_BUF_SIZE, uint, S_IRUGO);
MODULE_PARM_DESC(TX_BUF_SIZE, "data bytes in biggest supported SPI message");
static unsigned int RX_BUF_SIZE = (4 * 1024);
module_param(RX_BUF_SIZE, uint, S_IRUGO);
MODULE_PARM_DESC(RX_BUF_SIZE, "data bytes in biggest supported SPI message");

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    #define PTR_ERR_OR_ZERO         PTR_RET
#endif

#define SPI_CLOCK_SPEED             (24 * 1000000)      // 24MHz
//#define SPI_CLOCK_SPEED             (32 * 100000)       // 3.2MHz

static struct debussy_spi_priv      *p_debussy_spi_priv = NULL;

static ssize_t _debussy_spi_read(struct device *dev, uint32_t address, uint32_t *retData, size_t word_len);
static ssize_t _debussy_spi_write(struct device *dev, uint32_t address, uint32_t *data, size_t word_len);
static ssize_t _spidev_sync(struct device *dev, struct spi_message *message);
static inline ssize_t _spidev_sync_read(struct device *dev, size_t read_len, uint32_t write_len);

/*-------------------------------------------------------------------------*/
static int _check_spi_tx_rx_buffer(struct debussy_spi_priv *spidev_priv) {
    if (!spidev_priv->tx_buffer) {
        spidev_priv->tx_buffer = devm_kzalloc(&spidev_priv->spi->dev, TX_BUF_SIZE + 8, GFP_KERNEL);
        if (!spidev_priv->tx_buffer) {
            dev_err(&spidev_priv->spi->dev, "open/ENOMEM\n");

            return -ENOMEM;
        }
    }

    if (!spidev_priv->rx_buffer) {
        spidev_priv->rx_buffer = devm_kzalloc(&spidev_priv->spi->dev, RX_BUF_SIZE, GFP_KERNEL);
        if (!spidev_priv->rx_buffer) {
            dev_err(&spidev_priv->spi->dev, "open/ENOMEM\n");

            return -ENOMEM;
        }
    }

    return 0;
}

static ssize_t _debussy_spi_get_chip_id(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_spi_get_chip_id_fops = {
    .open = simple_open,
    .write = _debussy_spi_get_chip_id,
};

static ssize_t _debussy_spi_reg_get(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_spi_reg_get_fops = {
    .open = simple_open,
    .write = _debussy_spi_reg_get,
};

static ssize_t _debussy_spi_reg_put(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos);
static const struct file_operations debussy_spi_reg_put_fops = {
    .open = simple_open,
    .write = _debussy_spi_reg_put,
};

/*-------------------------------------------------------------------------*/
static ssize_t _debussy_spi_get_chip_id(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_spi_priv* spidev_priv = file->private_data;
    uint32_t chip_id;

    mutex_lock(&spidev_priv->debussy->igo_ch_lock);
    _debussy_spi_read(&spidev_priv->spi->dev, 0x2A000000, &chip_id, 1);
    dev_info(&spidev_priv->spi->dev, "CHIP ID: 0x%08X\n", chip_id);
    mutex_unlock(&spidev_priv->debussy->igo_ch_lock);

    return count;
}

// Usage: echo address > /d/debussy/spi_reg_get
static ssize_t _debussy_spi_reg_get(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_spi_priv* spidev_priv = file->private_data;
    uint32_t reg, data, position;
    char *input_data = NULL;

    if ((input_data = devm_kzalloc(&spidev_priv->spi->dev, count + 1, GFP_KERNEL)) == NULL) {
        dev_err(&spidev_priv->spi->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(&spidev_priv->spi->dev, input_data);
        dev_err(&spidev_priv->spi->dev, "%s: copy_from_user fail\n", __func__);

        return -EFAULT;
    }

    position = count;
    dev_info(&spidev_priv->spi->dev, "%s: input_data = %s, %zu\n", __func__, input_data, count);
    position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
    reg = simple_strtoul(&input_data[position], NULL, 16);
    dev_info(&spidev_priv->spi->dev, "%s: position = %d, reg = 0x%X\n", __func__, position, reg);
    devm_kfree(&spidev_priv->spi->dev, input_data);

    mutex_lock(&spidev_priv->debussy->igo_ch_lock);
    _debussy_spi_read(&spidev_priv->spi->dev, reg, &data, 1);
    dev_info(&spidev_priv->spi->dev, "%s: Reg 0x%X = 0x%X\n\n", __func__, reg, data);
    mutex_unlock(&spidev_priv->debussy->igo_ch_lock);

    return count;
}

// Usage: echo address, data > /d/debussy/spi_reg_put
static ssize_t _debussy_spi_reg_put(struct file* file,
    const char __user* user_buf,
    size_t count, loff_t* ppos)
{
    struct debussy_spi_priv* spidev_priv = file->private_data;
    unsigned int reg, data;
    uint32_t position;
    char *input_data, *next_data;

    if ((input_data = devm_kzalloc(&spidev_priv->spi->dev, count + 1, GFP_KERNEL)) == NULL) {
        dev_err(&spidev_priv->spi->dev, "%s: alloc fail\n", __func__);
        return -EFAULT;
    }

    memset(input_data, 0, count + 1);
    if (copy_from_user(input_data, user_buf, count)) {
        devm_kfree(&spidev_priv->spi->dev, input_data);
        dev_err(&spidev_priv->spi->dev, "%s: copy_from_user fail\n", __func__);

        return -EFAULT;
    }

    position = count;
    dev_info(&spidev_priv->spi->dev, "%s: input_data = %s, %zu\n", __func__, input_data, count);

    position = strcspn(input_data, "1234567890abcdefABCDEF");               // find first number
    reg = simple_strtoul(&input_data[position], &next_data, 16);
    position = strcspn(next_data, "1234567890abcdefABCDEF");                // find next number
    data = simple_strtoul(&next_data[position], NULL, 16);
    devm_kfree(&spidev_priv->spi->dev, input_data);

    mutex_lock(&spidev_priv->debussy->igo_ch_lock);
    _debussy_spi_write(&spidev_priv->spi->dev, reg, &data, 1);
    dev_info(&spidev_priv->spi->dev, "%s: Reg 0x%X = 0x%X\n\n", __func__, reg, data);
    mutex_unlock(&spidev_priv->debussy->igo_ch_lock);

    return count;
}

static int _check_spi_intf(struct debussy_spi_priv* spidev_priv) {
    if (NULL == spidev_priv) {
        pr_err("debussy: %s: debussy_spi_priv is null!\n", __func__);
        return -EFAULT;
    }
    else if (0 == spidev_priv->enable_spi) {
        dev_err(&spidev_priv->spi->dev, "debussy: %s: SPI is not enabled\n", __func__);
        return -EFAULT;
    }

    return 0;
}

static int _debussy_debufs_init(struct debussy_spi_priv* spidev_priv)
{
    int ret;
    struct dentry* dir;

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

#if 0
    dir = debugfs_create_dir("debussy", NULL);
    if (IS_ERR_OR_NULL(dir)) {
        dev_err(&spidev_priv->spi->dev, "%s: Failed to create debugfs node %s - folder was existed\n",
            __func__, "debussy");
        dir = spidev_priv->debussy->dir;
    }
#else
    dir = spidev_priv->debussy->dir;
#endif

    if (!debugfs_create_file("spi_get_chip_id", S_IWUSR,
            dir, spidev_priv, &debussy_spi_get_chip_id_fops)) {
        dev_err(&spidev_priv->spi->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "spi_get_chip_id");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("spi_reg_get", S_IWUSR,
            dir, spidev_priv, &debussy_spi_reg_get_fops)) {
        dev_err(&spidev_priv->spi->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "spi_reg_get");
        ret = -ENODEV;
        goto err_create_entry;
    }

    if (!debugfs_create_file("spi_reg_put", S_IWUSR,
            dir, spidev_priv, &debussy_spi_reg_put_fops)) {
        dev_err(&spidev_priv->spi->dev, "%s: Failed to create debugfs node %s\n",
            __func__, "spi_reg_put");
        ret = -ENODEV;
        goto err_create_entry;
    }

err_create_entry:
    return ret;
}

/*-------------------------------------------------------------------------*/
static int
_debussy_enable_spi_intf(struct debussy_spi_priv* spidev_priv, uint32_t enable)
{
    uint32_t data;

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return 1;
    }

    igo_i2c_read(i2c_verify_client(spidev_priv->debussy->dev), 0x2A012020, (unsigned int *) &data);

    if (enable) {
        if (0x83000000 == (data & 0xFF000000)) {
            #ifdef ENABLE_SPI_DRIVER_DEBUG
            dev_info(&spidev_priv->spi->dev, "SPI interface was enabled.\n");
            #endif

            return 0;
        }
    }
    else {
        if (0x03000000 == (data & 0xFF000000)) {
            #ifdef ENABLE_SPI_DRIVER_DEBUG
            dev_info(&spidev_priv->spi->dev, "SPI interface was disabled.\n");
            #endif

            return 0;
        }
    }

    data &= 0x00FFFFFF;
    (enable) ? (data |= 0x83000000) : (data |= 0x03000000);
    igo_i2c_write(i2c_verify_client(spidev_priv->debussy->dev), 0x2A012020, data);
    igo_i2c_read(i2c_verify_client(spidev_priv->debussy->dev), 0x2A0110B8, (unsigned int *) &data);
    data &= 0xF0FFFFFF;
    (enable) ? (data |= 0x01000000) : (data &= 0xF0FFFFFF);
    igo_i2c_write(i2c_verify_client(spidev_priv->debussy->dev), 0x2A0110B8, data);

#if 0
    igo_i2c_read(i2c_verify_client(spidev_priv->debussy->dev), 0x2A012020, &data);
    dev_info(&spidev_priv->spi->dev, "0x2A012020 = 0x%X\n", data);
    igo_i2c_read(i2c_verify_client(spidev_priv->debussy->dev), 0x2A0110B8, &data);
    dev_info(&spidev_priv->spi->dev, "0x2A0110B8 = 0x%X\n", data);
#endif

    dev_info(&spidev_priv->spi->dev, "SPI interface is %s.\n", enable ? "enabled" : "disabled");

    return 0;
}

static ssize_t
_debussy_spi_read(struct device* dev,
    uint32_t address, uint32_t *retData, size_t word_len)
{
    struct debussy_spi_priv* spidev_priv = dev_get_drvdata(dev);

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    mutex_lock(&spidev_priv->buf_lock);

    if (0 != _check_spi_tx_rx_buffer(spidev_priv)) {
        mutex_unlock(&spidev_priv->buf_lock);

        return -ENOMEM;
    }

#ifdef ENABLE_DEBUSSY_SPI_REGMAP
    if (spidev_priv->spi_regmap) {
        if (0 != regmap_raw_read(spidev_priv->spi_regmap, address, retData, word_len << 2)) {
            word_len = 0;
        }

        mutex_unlock(&spidev_priv->buf_lock);

        return word_len;
    }
#endif

    spidev_priv->tx_buffer[0] = 0x2B;
    spidev_priv->tx_buffer[1] = (address >> 16) & 0xFF;
    spidev_priv->tx_buffer[2] = (address >> 8) & 0xFF;
    spidev_priv->tx_buffer[3] = address & 0xFF;
    spidev_priv->tx_buffer[4] = 0;
    spidev_priv->tx_buffer[5] = 0;
    spidev_priv->tx_buffer[6] = 0;

    _spidev_sync_read(dev, word_len << 2, 7);

    if (retData) {
        if (spidev_priv->isLittleEndian) {
            memcpy(retData, spidev_priv->rx_buffer, word_len << 2);
        }
        else {
            _endian_swap(retData, (uint32_t *) spidev_priv->rx_buffer, word_len);
        }
    }

    mutex_unlock(&spidev_priv->buf_lock);

    return word_len;
}

static ssize_t
_debussy_spi_write(struct device *dev,
    uint32_t address, uint32_t *data, size_t word_len)
{
    struct spi_message message;
    struct spi_transfer xfer;

    struct debussy_spi_priv* spidev_priv = dev_get_drvdata(dev);

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    /* chipselect only toggles at start or end of operation */
    if ((word_len << 2) > TX_BUF_SIZE) {
        dev_err(dev, "%s: Write size is exceed to %dKB\n", __func__, TX_BUF_SIZE >> 10);
        return -EMSGSIZE;
    }

    mutex_lock(&spidev_priv->buf_lock);
    //dev_info(dev, "%s -\n", __func__);
    spi_message_init(&message);
    memset((void *) &xfer, 0, sizeof(xfer));

    if (0 != _check_spi_tx_rx_buffer(spidev_priv)) {
        dev_err(dev, "open/ENOMEM\n");
        mutex_unlock(&spidev_priv->buf_lock);

        return -ENOMEM;
    }

#ifdef ENABLE_DEBUSSY_SPI_REGMAP
    if (spidev_priv->spi_regmap) {
        if (0 != regmap_raw_write(spidev_priv->spi_regmap, address, data, word_len << 2)) {
            word_len = 0;
        }

        mutex_unlock(&spidev_priv->buf_lock);

        return word_len;
    }
#endif

    spidev_priv->tx_buffer[0] = 0x42;
    spidev_priv->tx_buffer[1] = (address >> 16) & 0xFF;
    spidev_priv->tx_buffer[2] = (address >> 8) & 0xFF;
    spidev_priv->tx_buffer[3] = address & 0xFF;

    if (spidev_priv->isLittleEndian) {
        memcpy((void *) &spidev_priv->tx_buffer[4], (void *) data, word_len << 2);
    }
    else {
        _endian_swap((unsigned int *) spidev_priv->tx_buffer, data, word_len);
    }

    xfer.tx_buf = spidev_priv->tx_buffer;
    xfer.len = (word_len << 2) + 4;
    spi_message_add_tail(&xfer, &message);
    _spidev_sync(dev, &message);

    //debussy_enable_spi_intf(spidev_priv, 0);
    mutex_unlock(&spidev_priv->buf_lock);

    return word_len;
}

static ssize_t
_spidev_sync(struct device *dev, struct spi_message *message)
{
    DECLARE_COMPLETION_ONSTACK(done);
    int status;
    struct spi_device *spi;
    struct debussy_spi_priv* spidev_priv = dev_get_drvdata(dev);

    spin_lock_irq(&spidev_priv->spi_lock);
    spi = spidev_priv->spi;
    spin_unlock_irq(&spidev_priv->spi_lock);

    if (spi == NULL) {
        status = -ESHUTDOWN;
        dev_err(dev, "%s - spi is null\n", __func__);
    }
    else {
        status = spi_sync(spi, message);
    }

    if (status == 0) {
        status = message->actual_length;
        #ifdef ENABLE_SPI_DRIVER_DEBUG
        dev_info(dev, "%s - message->actual_length = %d\n", __func__, message->actual_length);
        #endif
    }

    return status;
}

static inline ssize_t
_spidev_sync_write(struct device *dev, size_t len)
{
    struct debussy_spi_priv* spidev_priv = dev_get_drvdata(dev);
    struct spi_transfer t = {
        .tx_buf         = spidev_priv->tx_buffer,
        .len            = len,
        .speed_hz       = spidev_priv->speed_hz,
    };
    struct spi_message m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return _spidev_sync(dev, &m);
}

static inline ssize_t
_spidev_sync_read(struct device *dev, size_t read_len, uint32_t write_len)
{
    struct spi_transfer t[2];
    struct spi_message  m;
    struct debussy_spi_priv* spidev_priv = dev_get_drvdata(dev);

    spi_message_init(&m);
    memset(t, 0, sizeof(t));

    if (write_len) {
        t[0].tx_buf = spidev_priv->tx_buffer;
        t[0].len = write_len;
        t[0].speed_hz = spidev_priv->speed_hz;
        spi_message_add_tail(&t[0], &m);
    }

    t[1].rx_buf = spidev_priv->rx_buffer;
    t[1].len = read_len;
    t[1].speed_hz = spidev_priv->speed_hz;
    spi_message_add_tail(&t[1], &m);

    return _spidev_sync(dev, &m);
}

/*-------------------------------------------------------------------------*/
static loff_t
_spidev_seek(struct file *file, loff_t p, int orig)
{
    struct debussy_spi_priv* spidev_priv = file->private_data;

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    mutex_lock(&spidev_priv->buf_lock);
    spidev_priv->reg_address = p & 0xFFFFFFFC;
    #ifdef ENABLE_SPI_DRIVER_DEBUG
    dev_info(&spidev_priv->spi->dev, "%s - seek to 0x%X\n", __func__, spidev_priv->reg_address);
    #endif
    mutex_unlock(&spidev_priv->buf_lock);

    return p;
}

/* Read-only message with current device setup */
static ssize_t
_spidev_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct debussy_spi_priv *spidev_priv;
    ssize_t                 status = 0;

    spidev_priv = filp->private_data;
    dev_info(&spidev_priv->spi->dev, "%s\n", __func__);

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    /* chipselect only toggles at start or end of operation */
    if (count > RX_BUF_SIZE) {
        dev_err(&spidev_priv->spi->dev, "%s: Read size is exceed to %dKB\n", __func__, RX_BUF_SIZE >> 10);
        return -EMSGSIZE;
    }

    mutex_lock(&spidev_priv->debussy->igo_ch_lock);
    mutex_lock(&spidev_priv->buf_lock);
    _debussy_enable_spi_intf(spidev_priv, 1);

    spidev_priv->tx_buffer[0] = 0x2B;
    spidev_priv->tx_buffer[1] = (spidev_priv->reg_address >> 16) & 0xFF;
    spidev_priv->tx_buffer[2] = (spidev_priv->reg_address >> 8) & 0xFF;
    spidev_priv->tx_buffer[3] = spidev_priv->reg_address & 0xFF;
    spidev_priv->tx_buffer[4] = 0;
    spidev_priv->tx_buffer[5] = 0;
    spidev_priv->tx_buffer[6] = 0;

    status = _spidev_sync_read(&spidev_priv->spi->dev, count, 7);
    if (status > 0) {
        unsigned long   missing;

        missing = copy_to_user(buf, spidev_priv->rx_buffer, status);
        if (missing == status)
            status = -EFAULT;
        else
            status = status - missing;
    }

    mutex_unlock(&spidev_priv->buf_lock);
    mutex_unlock(&spidev_priv->debussy->igo_ch_lock);

    return status;
}

/* Write-only message with current device setup */
static ssize_t
_spidev_write(struct file *filp, const char __user *buf,
        size_t count, loff_t *f_pos)
{
    struct debussy_spi_priv *spidev_priv;
    ssize_t                 status = 0;
    unsigned long           missing;

    spidev_priv = filp->private_data;

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    /* chipselect only toggles at start or end of operation */
    if (count > TX_BUF_SIZE) {
        dev_err(&spidev_priv->spi->dev, "%s: Write size is exceed to %dKB\n", __func__, TX_BUF_SIZE >> 10);
        return -EMSGSIZE;
    }

    mutex_lock(&spidev_priv->debussy->igo_ch_lock);
    mutex_lock(&spidev_priv->buf_lock);
    _debussy_enable_spi_intf(spidev_priv, 1);

    spidev_priv->tx_buffer[0] = 0x42;
    spidev_priv->tx_buffer[1] = (spidev_priv->reg_address >> 16) & 0xFF;
    spidev_priv->tx_buffer[2] = (spidev_priv->reg_address >> 8) & 0xFF;
    spidev_priv->tx_buffer[3] = spidev_priv->reg_address & 0xFF;

    missing = copy_from_user(&spidev_priv->tx_buffer[4], buf, count);
    if (missing == 0) {
        _spidev_sync_write(&spidev_priv->spi->dev, count + 4);
    }
    else {
        status = -EFAULT;
    }

    //debussy_enable_spi_intf(spidev_priv, 0);
    mutex_unlock(&spidev_priv->buf_lock);
    mutex_unlock(&spidev_priv->debussy->igo_ch_lock);

    return status;
}

#if 0
static int
_spidev_message(struct debussy_spi_priv *spidev_priv,
        struct spi_ioc_transfer *u_xfers, unsigned n_xfers)
{
    struct spi_message      msg;
    struct spi_transfer     *k_xfers;
    struct spi_transfer     *k_tmp;
    struct spi_ioc_transfer *u_tmp;
    unsigned                n, total, tx_total, rx_total;
    u8                      *tx_buf, *rx_buf;
    int                     status = -EFAULT;

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    spi_message_init(&msg);
    k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
    if (k_xfers == NULL)
        return -ENOMEM;

    /* Construct spi_message, copying any tx data to bounce buffer.
     * We walk the array of user-provided transfers, using each one
     * to initialize a kernel version of the same transfer.
     */
    tx_buf = spidev_priv->tx_buffer;
    rx_buf = spidev_priv->rx_buffer;
    total = 0;
    tx_total = 0;
    rx_total = 0;
    for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
            n;
            n--, k_tmp++, u_tmp++) {
        k_tmp->len = u_tmp->len;

        total += k_tmp->len;
        /* Since the function returns the total length of transfers
         * on success, restrict the total to positive int values to
         * avoid the return value looking like an error.  Also check
         * each transfer length to avoid arithmetic overflow.
         */
        if (total > INT_MAX || k_tmp->len > INT_MAX) {
            status = -EMSGSIZE;
            goto done;
        }

        if (u_tmp->rx_buf) {
            /* this transfer needs space in RX bounce buffer */
            rx_total += k_tmp->len;
            if (rx_total > RX_BUF_SIZE) {
                status = -EMSGSIZE;
                goto done;
            }
            k_tmp->rx_buf = rx_buf;
            if (!access_ok(VERIFY_WRITE, (u8 __user *)
                    (uintptr_t) u_tmp->rx_buf,
                    u_tmp->len))
                goto done;
            rx_buf += k_tmp->len;
        }
        if (u_tmp->tx_buf) {
            /* this transfer needs space in TX bounce buffer */
            tx_total += k_tmp->len;
            if (tx_total > TX_BUF_SIZE) {
                status = -EMSGSIZE;
                goto done;
            }
            k_tmp->tx_buf = tx_buf;
            if (copy_from_user(tx_buf, (const u8 __user *)
                        (uintptr_t) u_tmp->tx_buf,
                        u_tmp->len))
                goto done;
            tx_buf += k_tmp->len;
        }

        k_tmp->cs_change = !!u_tmp->cs_change;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,11)
        // For Android 5.1
        k_tmp->tx_nbits = u_tmp->tx_nbits;
        k_tmp->rx_nbits = u_tmp->rx_nbits;
#endif
        k_tmp->bits_per_word = u_tmp->bits_per_word;
        k_tmp->delay_usecs = u_tmp->delay_usecs;
        k_tmp->speed_hz = u_tmp->speed_hz;
        if (!k_tmp->speed_hz)
            k_tmp->speed_hz = spidev_priv->speed_hz;
#ifdef VERBOSE
        dev_dbg(&spidev_priv->spi->dev,
                "  xfer len %u %s%s%s%dbits %u usec %uHz\n",
                u_tmp->len,
                u_tmp->rx_buf ? "rx " : "",
                u_tmp->tx_buf ? "tx " : "",
                u_tmp->cs_change ? "cs " : "",
                u_tmp->bits_per_word ? : spidev_priv->spi->bits_per_word,
                u_tmp->delay_usecs,
                u_tmp->speed_hz ? : spidev_priv->spi->max_speed_hz);
#endif
        spi_message_add_tail(k_tmp, &msg);
    }

    status = _spidev_sync(&spidev_priv->spi->dev, &msg);
    if (status < 0)
        goto done;

    /* copy any rx data out of bounce buffer */
    rx_buf = spidev_priv->rx_buffer;
    for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
        if (u_tmp->rx_buf) {
            if (copy_to_user((u8 __user *)
                    (uintptr_t) u_tmp->rx_buf, rx_buf,
                    u_tmp->len)) {
                status = -EFAULT;
                goto done;
            }
            rx_buf += u_tmp->len;
        }
    }
    status = total;

done:
    devm_kfree(&spidev_priv->spi->dev, k_xfers);
    return status;
}

static struct spi_ioc_transfer *
spidev_get_ioc_message(unsigned int cmd, struct spi_ioc_transfer __user *u_ioc,
        unsigned *n_ioc)
{
    struct spi_ioc_transfer *ioc;
    u32     tmp;

    /* Check type, command number and direction */
    if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC
            || _IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
            || _IOC_DIR(cmd) != _IOC_WRITE)
        return ERR_PTR(-ENOTTY);

    tmp = _IOC_SIZE(cmd);
    if ((tmp % sizeof(struct spi_ioc_transfer)) != 0)
        return ERR_PTR(-EINVAL);
    *n_ioc = tmp / sizeof(struct spi_ioc_transfer);
    if (*n_ioc == 0)
        return NULL;

    /* copy into scratch area */
    ioc = devm_kzalloc(&spidev_priv->spi->dev, tmp, GFP_KERNEL);
    if (!ioc)
        return ERR_PTR(-ENOMEM);
    if (__copy_from_user(ioc, u_ioc, tmp)) {
        devm_kfree(&spidev_priv->spi->dev, ioc);
        return ERR_PTR(-EFAULT);
    }
    return ioc;
}

static long
spidev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int                     err = 0;
    int                     retval = 0;
    struct debussy_spi_priv *spidev_priv;
    struct spi_device       *spi;
    u32                     tmp;
    unsigned                n_ioc;
    struct spi_ioc_transfer *ioc;

    /* Check type and command number */
    if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
        return -ENOTTY;

    /* Check access direction once here; don't repeat below.
     * IOC_DIR is from the user perspective, while access_ok is
     * from the kernel perspective; so they look reversed.
     */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE,
                        (void __user *)arg, _IOC_SIZE(cmd));
    if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
        err = !access_ok(VERIFY_READ,
                        (void __user *)arg, _IOC_SIZE(cmd));
    if (err)
        return -EFAULT;

    /* guard against device removal before, or while,
     * we issue this ioctl.
     */
    spidev_priv = filp->private_data;
    spin_lock_irq(&spidev_priv->spi_lock);
    spi = spi_dev_get(spidev_priv->spi);
    spin_unlock_irq(&spidev_priv->spi_lock);

    if (spi == NULL)
        return -ESHUTDOWN;

    /* use the buffer lock here for triple duty:
     *  - prevent I/O (from us) so calling spi_setup() is safe;
     *  - prevent concurrent SPI_IOC_WR_* from morphing
     *    data fields while SPI_IOC_RD_* reads them;
     *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
     */
    mutex_lock(&spidev_priv->buf_lock);

    switch (cmd) {
    /* read requests */
    case SPI_IOC_RD_MODE:
        retval = __put_user(spi->mode & SPI_MODE_MASK,
                                (__u8 __user *)arg);
        break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,11)
    // For Android 5.1
    case SPI_IOC_RD_MODE32:
        retval = __put_user(spi->mode & SPI_MODE_MASK,
                                (__u32 __user *)arg);
        break;
#endif
    case SPI_IOC_RD_LSB_FIRST:
        retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
                                (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_BITS_PER_WORD:
        retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
        break;
    case SPI_IOC_RD_MAX_SPEED_HZ:
        retval = __put_user(spidev_priv->speed_hz, (__u32 __user *)arg);
        break;

    /* write requests */
    case SPI_IOC_WR_MODE:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,11)
// For Android 5.1
    case SPI_IOC_WR_MODE32:
#endif
        if (cmd == SPI_IOC_WR_MODE)
            retval = __get_user(tmp, (u8 __user *)arg);
        else
            retval = __get_user(tmp, (u32 __user *)arg);
        if (retval == 0) {
            u32     save = spi->mode;

            if (tmp & ~SPI_MODE_MASK) {
                retval = -EINVAL;
                break;
            }

            tmp |= spi->mode & ~SPI_MODE_MASK;
            spi->mode = (u16)tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->mode = save;
            else
                dev_dbg(&spi->dev, "spi mode %x\n", tmp);
        }
        break;
    case SPI_IOC_WR_LSB_FIRST:
        retval = __get_user(tmp, (__u8 __user *)arg);
        if (retval == 0) {
            u32     save = spi->mode;

            if (tmp)
                spi->mode |= SPI_LSB_FIRST;
            else
                spi->mode &= ~SPI_LSB_FIRST;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->mode = save;
            else
                dev_dbg(&spi->dev, "%csb first\n",
                        tmp ? 'l' : 'm');
        }
        break;
    case SPI_IOC_WR_BITS_PER_WORD:
        retval = __get_user(tmp, (__u8 __user *)arg);
        if (retval == 0) {
            u8      save = spi->bits_per_word;

            spi->bits_per_word = tmp;
            retval = spi_setup(spi);
            if (retval < 0)
                spi->bits_per_word = save;
            else
                dev_dbg(&spi->dev, "%d bits per word\n", tmp);
        }
        break;
    case SPI_IOC_WR_MAX_SPEED_HZ:
        retval = __get_user(tmp, (__u32 __user *)arg);
        if (retval == 0) {
            u32     save = spi->max_speed_hz;

            spi->max_speed_hz = tmp;
            retval = spi_setup(spi);
            if (retval >= 0)
                spidev_priv->speed_hz = tmp;
            else
                dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
            spi->max_speed_hz = save;
        }
        break;

    default:
        /* segmented and/or full-duplex I/O request */
        /* Check message and copy into scratch area */
        ioc = spidev_get_ioc_message(cmd,
                            (struct spi_ioc_transfer __user *)arg, &n_ioc);
        if (IS_ERR(ioc)) {
            retval = PTR_ERR(ioc);
            break;
        }
        if (!ioc)
            break;  /* n_ioc is also 0 */

        /* translate to spi_message, execute */
        retval = _spidev_message(spidev_priv, ioc, n_ioc);
        devm_kfree(&spidev_priv->spi->dev, ioc);
        break;
    }

    mutex_unlock(&spidev_priv->buf_lock);
    spi_dev_put(spi);
    return retval;
}
#endif

static int _spidev_open(struct inode *inode, struct file *filp)
{
    struct debussy_spi_priv *spidev_priv;
    int                     status = -ENXIO;

    mutex_lock(&device_list_lock);

    list_for_each_entry(spidev_priv, &device_list, device_entry) {
        if (spidev_priv->devt == inode->i_rdev) {
            status = 0;
            break;
        }
    }

    dev_info(&spidev_priv->spi->dev, "%s\n", __func__);

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    mutex_lock(&spidev_priv->buf_lock);

    if (status) {
        dev_err(&spidev_priv->spi->dev, "spidev_priv: nothing for minor %d\n", iminor(inode));
        goto err_find_dev;
    }

    if (0 != _check_spi_tx_rx_buffer(spidev_priv)) {
        dev_err(&spidev_priv->spi->dev, "open/ENOMEM\n");
        status = -ENOMEM;
        goto err_find_dev;
    }

    spidev_priv->users++;
    filp->private_data = spidev_priv;
    nonseekable_open(inode, filp);

    _debussy_enable_spi_intf(spidev_priv, 1);

    mutex_unlock(&spidev_priv->buf_lock);
    mutex_unlock(&device_list_lock);

    return 0;

err_find_dev:
    mutex_unlock(&spidev_priv->buf_lock);
    mutex_unlock(&device_list_lock);

    return status;
}

static int _spidev_release(struct inode *inode, struct file *filp)
{
    struct debussy_spi_priv *spidev_priv;

    mutex_lock(&device_list_lock);

    spidev_priv = filp->private_data;
    filp->private_data = NULL;

    if (0 != _check_spi_intf(spidev_priv)) {
        pr_err("debussy: %s: SPI Intf error!\n", __func__);
        return -EFAULT;
    }

    mutex_lock(&spidev_priv->buf_lock);

    dev_info(&spidev_priv->spi->dev, "%s\n", __func__);

    /* last close? */
    spidev_priv->users--;
    if (!spidev_priv->users) {
        int dofree;

        devm_kfree(&spidev_priv->spi->dev, spidev_priv->tx_buffer);
        spidev_priv->tx_buffer = NULL;

        devm_kfree(&spidev_priv->spi->dev, spidev_priv->rx_buffer);
        spidev_priv->rx_buffer = NULL;

        spin_lock_irq(&spidev_priv->spi_lock);
        if (spidev_priv->spi)
            spidev_priv->speed_hz = spidev_priv->spi->max_speed_hz;

        /* ... after we unbound from the underlying device? */
        dofree = (spidev_priv->spi == NULL);
        spin_unlock_irq(&spidev_priv->spi_lock);

        if (dofree) {
            devm_kfree(&spidev_priv->spi->dev, spidev_priv);
        }
    }

    mutex_unlock(&spidev_priv->buf_lock);
    mutex_unlock(&device_list_lock);

    return 0;
}

/*-------------------------------------------------------------------------*/

static const struct file_operations spidev_fops = {
    .owner =        THIS_MODULE,
    /* REVISIT switch to aio primitives, so that userspace
     * gets more complete API coverage.  It'll simplify things
     * too, except for the locking.
     */
    .write =        _spidev_write,
    .read =         _spidev_read,
    //.unlocked_ioctl = _spidev_ioctl,
    //.compat_ioctl = _spidev_compat_ioctl,
    .open =         _spidev_open,
    .release =      _spidev_release,
    .llseek =       _spidev_seek,
};

/*-------------------------------------------------------------------------*/

/* The main reason to have this class is to make mdev/udev create the
 * /dev/spidevB.C character device nodes exposing our userspace API.
 * It also simplifies memory management.
 */

static struct class *spidev_class;

#if DTS_SUPPORT == 1
static const struct of_device_id spidev_dt_ids[] = {
    { .compatible = "intelligo,debussy" },
    {},
};
MODULE_DEVICE_TABLE(of, spidev_dt_ids);
#endif  /* end of DTS_SUPPORT */
/*-------------------------------------------------------------------------*/

static int _spidev_drv_probe(struct spi_device *spi)
{
    struct debussy_spi_priv *spidev_priv;
    int                     status;
    unsigned long           minor;

    dev_info(&spi->dev, "%s\n", __func__);

    /* Allocate driver data */
    spidev_priv = devm_kzalloc(&spi->dev, sizeof(struct debussy_spi_priv), GFP_KERNEL);
    if (!spidev_priv) {
        pr_err("debussy: %s - devm_kzalloc fail\n", __func__);
        return -ENOMEM;
    }

    /////////////////////////////////////////////////////////////////////////
    /* Initialize the driver data */
    memset(spidev_priv, 0, sizeof(*spidev_priv));
    p_debussy_spi_priv = spidev_priv;
    spidev_priv->debussy = p_debussy_priv;

#if DTS_SUPPORT
    if (of_property_read_u32(spi->dev.of_node, "ig,enable-spi", &spidev_priv->enable_spi)) {
        dev_err(&spi->dev, "Unable to get \"ig,enable-spi\"\n");
        spidev_priv->enable_spi = 0;

        return -EPERM;
    }
#else
    spidev_priv->enable_spi = debussyDtsReplace.enable_spi;
#endif  /* end of DTS_SUPPORT */

    spidev_priv->debussy->spi_dev = &spi->dev;
    spidev_priv->spi = spi;

    #ifdef ENABLE_DEBUSSY_SPI_REGMAP
    spidev_priv->spi_regmap = devm_regmap_init_spi(spi, &debussy_spi_regmap);
    if (IS_ERR(spidev_priv->spi_regmap)) {
        dev_err(&spidev_priv->spi->dev, "Failed to allocate SPI regmap!\n");
        //PTR_ERR(spidev_priv->spi_regmap);
        spidev_priv->spi_regmap = NULL;
    }
    else {
        regcache_cache_bypass(spidev_priv->spi_regmap, 1);
    }
    #endif

    spin_lock_init(&spidev_priv->spi_lock);
    mutex_init(&spidev_priv->buf_lock);

    INIT_LIST_HEAD(&spidev_priv->device_entry);

    /* If we can allocate a minor number, hook up this device.
     * Reusing minors is fine so long as udev or mdev is working.
     */
    mutex_lock(&device_list_lock);
    minor = find_first_zero_bit(minors, N_SPI_MINORS);

    if (minor < N_SPI_MINORS) {
        struct device *dev;

        spidev_priv->devt = MKDEV(SPIDEV_MAJOR, minor);
        dev = device_create(spidev_class, &spi->dev, spidev_priv->devt,
                            spidev_priv, "debussy_spi%d.%d",
                            spi->master->bus_num, spi->chip_select);
        status = PTR_ERR_OR_ZERO(dev);
    } else {
        dev_err(&spi->dev, "no minor number available!\n");
        status = -ENODEV;
    }

    if (status == 0) {
        set_bit(minor, minors);
        list_add(&spidev_priv->device_entry, &device_list);
    }

    mutex_unlock(&device_list_lock);

    /////////////////////////////////////////////////////////////////////////
#if DTS_SUPPORT
    if (of_property_read_u32(spi->dev.of_node, "ig,spi-reg-address", &spidev_priv->reg_address)) {
        dev_err(&spi->dev, "Unable to get \"ig,spi-reg-addr, using DEF_REG_ADDR\"\n");
        spidev_priv->reg_address = DEF_REG_ADDR;
    }

    dev_info(&spi->dev, "spidev_priv->reg_address = 0x%X\n", spidev_priv->reg_address);

    /////////////////////////////////////////////////////////////////////////
    if (of_property_read_u32(spi->dev.of_node, "ig,spi-max-speed", &spidev_priv->spi->max_speed_hz)) {
        dev_err(&spi->dev, "Unable to get \"ig,spi-max-speed, using DEF_SPI_MAX_SPEED\"\n");
        spidev_priv->spi->max_speed_hz = DEF_SPI_MAX_SPEED;
    }

    dev_info(&spi->dev, "spidev_priv->spi->max_speed_hz = %dHz\n", spidev_priv->spi->max_speed_hz);
    spidev_priv->speed_hz = spi->max_speed_hz;
#else
    spidev_priv->reg_address = debussyDtsReplace.spi_reg_address;
    dev_info(&spi->dev, "spidev_priv->reg_address = 0x%X\n", spidev_priv->reg_address);

    spidev_priv->spi->max_speed_hz = debussyDtsReplace.spi_max_speed;
    dev_info(&spi->dev, "spidev_priv->spi->max_speed_hz = %dHz\n", spidev_priv->spi->max_speed_hz);
    spidev_priv->speed_hz = spidev_priv->spi->max_speed_hz;
#endif  /* end of DTS_SUPPORT */

    if (status == 0) {
        spi_set_drvdata(spi, spidev_priv);
        _debussy_debufs_init(spidev_priv);
    }
    else {
        devm_kfree(&spi->dev, spidev_priv);
    }

    {
        uint32_t t = 0x12345678;
        uint8_t *p = (uint8_t *) &t;

        if (*p == 0x78) {
            spidev_priv->isLittleEndian = 1;
        }
        else {
            spidev_priv->isLittleEndian = 0;
        }
    }

    dev_info(&spi->dev, "%s - Done\n", __func__);

    return status;
}

static int _spidev_drv_remove(struct spi_device *spi)
{
    struct debussy_spi_priv *spidev_priv = spi_get_drvdata(spi);

    dev_info(&spi->dev, "%s\n", __func__);

    /* make sure ops on existing fds can abort cleanly */
    spin_lock_irq(&spidev_priv->spi_lock);
    spidev_priv->spi = NULL;
    spin_unlock_irq(&spidev_priv->spi_lock);

    /* prevent new opens */
    mutex_lock(&device_list_lock);
    list_del(&spidev_priv->device_entry);
    device_destroy(spidev_class, spidev_priv->devt);
    clear_bit(MINOR(spidev_priv->devt), minors);
    if (spidev_priv->users == 0) {
        devm_kfree(&spi->dev, spidev_priv);
    }
    mutex_unlock(&device_list_lock);

    return 0;
}

static struct spi_driver spidev_spi_driver = {
    .driver = {
        .name = "debussy",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(spidev_dt_ids),
    },
    .probe =        _spidev_drv_probe,
    .remove =       _spidev_drv_remove,

    /* NOTE:  suspend/resume methods are not necessary here.
     * We don't do anything except pass the requests to/from
     * the underlying controller.  The refrigerator handles
     * most issues; the controller driver handles the rest.
     */
};

/*-------------------------------------------------------------------------*/
ssize_t debussy_spidrv_buffer_read(uint32_t address, uint32_t *data, size_t word_len) {
    return _debussy_spi_read(&p_debussy_spi_priv->spi->dev, address, data, word_len);
}

ssize_t debussy_spidrv_buffer_write(uint32_t address, uint32_t *data, size_t word_len) {
    return _debussy_spi_write(&p_debussy_spi_priv->spi->dev, address, data, word_len);
}

int debussy_spidrv_intf_enable(uint32_t enable) {
    return _debussy_enable_spi_intf(p_debussy_spi_priv, enable);
}

int debussy_spidrv_intf_check(void) {
    if (0 == _check_spi_intf(p_debussy_spi_priv)) {
        return 0;
    }

    return 1;
}

int debussy_spidev_init(void)
{
    int status;

    if (NULL == p_debussy_priv) {
        pr_err("debussy: %s - The I2C device must be installed firstly.\n", __func__);
        return -ENOMEM;
    }

    /* Claim our 256 reserved device numbers.  Then register a class
     * that will key udev/mdev to add/remove /dev nodes.  Last, register
     * the driver which manages those device numbers.
     */
    BUILD_BUG_ON(N_SPI_MINORS > 256);
    status = register_chrdev(SPIDEV_MAJOR, "spi", &spidev_fops);
    if (status < 0) {
        pr_info("debussy spi: %s - register_chrdev fail\n", __func__);
        return status;
    }

    SPIDEV_MAJOR = status;
    pr_info("debussy: SPIDEV_MAJOR - %d\n", SPIDEV_MAJOR);

    spidev_class = class_create(THIS_MODULE, "debussy_spi");
    if (IS_ERR(spidev_class)) {
        pr_info("debussy spi: %s - class_create fail\n", __func__);
        unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
        return PTR_ERR(spidev_class);
    }

    status = spi_register_driver(&spidev_spi_driver);
    if (status < 0) {
        class_destroy(spidev_class);
        unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);
        pr_info("debussy spi: %s - spi_register_driver fail\n", __func__);
        return status;
    }

    pr_info("debussy spi: %s - Done\n", __func__);

    return status;
}

void debussy_spidev_exit(void)
{
    spi_unregister_driver(&spidev_spi_driver);
    class_destroy(spidev_class);
    unregister_chrdev(SPIDEV_MAJOR, spidev_spi_driver.driver.name);

    if (p_debussy_spi_priv->tx_buffer) {
        devm_kfree(&p_debussy_spi_priv->spi->dev, p_debussy_spi_priv->tx_buffer);
    }

    if (p_debussy_spi_priv->rx_buffer) {
        devm_kfree(&p_debussy_spi_priv->spi->dev, p_debussy_spi_priv->rx_buffer);
    }

    devm_kfree(&p_debussy_spi_priv->spi->dev, p_debussy_spi_priv);
    p_debussy_spi_priv = NULL;
}
#endif  /* end of ENABLE_SPI_INTF */

