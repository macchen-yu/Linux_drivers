#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/***************************************************************
 * Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
 * 文件名 : newchrled.c
 * 作者 : 正点原子
 * 版本 : V1.0
 * 描述 : LED驅動文件。
 * 其他 : 無
 * 论坛 : www.openedv.com
 * 日志 : 初版V1.0 2020/11/24 正点原子团队创建
 ***************************************************************/

#define NEWCHRLED_CNT 1       /* 設備號個數 */
#define NEWCHRLED_NAME "newchrled" /* 名字 */
#define LEDOFF 0              /* 關燈 */
#define LEDON 1               /* 開燈 */

/* 寄存器物理地址 */
#define PERIPH_BASE (0x40000000)
#define MPU_AHB4_PERIPH_BASE (PERIPH_BASE + 0x10000000)
#define RCC_BASE (MPU_AHB4_PERIPH_BASE + 0x0000)
#define RCC_MP_AHB4ENSETR (RCC_BASE + 0xA28)
#define GPIOI_BASE (MPU_AHB4_PERIPH_BASE + 0xA000)
#define GPIOI_MODER (GPIOI_BASE + 0x0000)
#define GPIOI_OTYPER (GPIOI_BASE + 0x0004)
#define GPIOI_OSPEEDR (GPIOI_BASE + 0x0008)
#define GPIOI_PUPDR (GPIOI_BASE + 0x000C)
#define GPIOI_BSRR (GPIOI_BASE + 0x0018)

/* 映射後的寄存器虛擬地址指針 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;

/* newchrled設備結構體 */
struct newchrled_dev {
    dev_t devid;             /* 設備號 */
    struct cdev cdev;        /* cdev */
    struct class *class;     /* 類 */
    struct device *device;   /* 設備 */
    int major;               /* 主設備號 */
    int minor;               /* 次設備號 */
};

struct newchrled_dev newchrled; /* led設備 */

/*
 * @description : LED開啟/關閉
 * @param - sta : LEDON(0) 開啟LED，LEDOFF(1) 關閉LED
 * @return : 無
 */
void led_switch(u8 sta) {
    u32 val = 0;
    if (sta == LEDON) {
        val = readl(GPIOI_BSRR_PI);
        val |= (1 << 16);
        writel(val, GPIOI_BSRR_PI);
    } else if (sta == LEDOFF) {
        val = readl(GPIOI_BSRR_PI);
        val |= (1 << 0);
        writel(val, GPIOI_BSRR_PI);
    }
}

/*
 * @description : 取消映射
 * @return : 無
 */
void led_unmap(void) {
    /* 取消映射 */
    iounmap(MPU_AHB4_PERIPH_RCC_PI);
    iounmap(GPIOI_MODER_PI);
    iounmap(GPIOI_OTYPER_PI);
    iounmap(GPIOI_OSPEEDR_PI);
    iounmap(GPIOI_PUPDR_PI);
    iounmap(GPIOI_BSRR_PI);
}

/*
 * @description : 開啟設備
 * @param – inode : 傳遞給驅動的inode
 * @param - filp : 設備文件，file結構體有個叫做private_data的成員變量
 * 一般在open的時候將private_data指向設備結構體。
 * @return : 0 成功;其他 失敗
 */
static int led_open(struct inode *inode, struct file *filp) {
    filp->private_data = &newchrled; /* 設置私有數據 */
    return 0;
}

/*
 * @description : 從設備讀取數據
 * @param - filp : 要開啟的設備文件(文件描述符)
 * @param - buf : 返回給用戶空間的數據緩衝區
 * @param - cnt : 要讀取的數據長度
 * @param - offt : 相對於文件首地址的偏移
 * @return : 讀取的字節數，如果為負值，表示讀取失敗
 */
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt) {
    return 0;
}

/*
 * @description : 向設備寫數據
 * @param – filp : 設備文件，表示開啟的文件描述符
 * @param - buf : 要寫給設備寫入的數據
 * @param - cnt : 要寫入的數據長度
 * @param - offt : 相對於文件首地址的偏移
 * @return : 寫入的字節數，如果為負值，表示寫入失敗
 */
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt) {
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;

    retvalue = copy_from_user(databuf, buf, cnt);
    if (retvalue < 0) {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0]; /* 獲取狀態值 */

    if (ledstat == LEDON) {
        led_switch(LEDON); /* 開啟LED燈 */
    } else if (ledstat == LEDOFF) {
        led_switch(LEDOFF); /* 關閉LED燈 */
    }
    return 0;
}

/*
 * @description : 關閉/釋放設備
 * @param - filp : 要關閉的設備文件(文件描述符)
 * @return : 0 成功;其他 失敗
 */
static int led_release(struct inode *inode, struct file *filp) {
    return 0;
}

/* 設備操作函數 */
static struct file_operations newchrled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

/*
 * @description : 驅動出口函數
 * @param : 無
 * @return : 無
 */
static int __init led_init(void) {
    u32 val = 0;
    int ret;

    /* 初始化LED */
    /* 1、寄存器地址映射 */
    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4);
    GPIOI_MODER_PI = ioremap(GPIOI_MODER, 4);
    GPIOI_OTYPER_PI = ioremap(GPIOI_OTYPER, 4);
    GPIOI_OSPEEDR_PI = ioremap(GPIOI_OSPEEDR, 4);
    GPIOI_PUPDR_PI = ioremap(GPIOI_PUPDR, 4);
    GPIOI_BSRR_PI = ioremap(GPIOI_BSRR, 4);

    /* 2、使能PI時鐘 */
    val = readl(MPU_AHB4_PERIPH_RCC_PI);
    val &= ~(0X1 << 8); /* 清除以前的設置 */
    val |= (0X1 << 8); /* 設置新值 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);

    /* 3、設置PI0通用的輸出模式。*/
    val = readl(GPIOI_MODER_PI);
    val &= ~(0X3 << 0); /* bit0:1清零 */
    val |= (0X1 << 0); /* bit0:1設置01 */
    writel(val, GPIOI_MODER_PI);

    /* 3、設置PI0為推挽模式。*/
    val = readl(GPIOI_OTYPER_PI);
    val &= ~(0X1 << 0); /* bit0清零，設置為上拉 */
    writel(val, GPIOI_OTYPER_PI);

    /* 4、設置PI0為高速。*/
    val = readl(GPIOI_OSPEEDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零 */
    val |= (0x2 << 0); /* bit0:1 設置為10 */
    writel(val, GPIOI_OSPEEDR_PI);

    /* 5、設置PI0為上拉。*/
    val = readl(GPIOI_PUPDR_PI);
    val &= ~(0X3 << 0); /* bit0:1 清零 */
    val |= (0x1 << 0); /* bit0:1 設置為01 */
    writel(val, GPIOI_PUPDR_PI);

    /* 6、默認關閉LED */
    val = readl(GPIOI_BSRR_PI);
    val |= (0x1 << 0);
    writel(val, GPIOI_BSRR_PI);

    /* 註冊字符設備驅動 */
    /* 1、創建設備號 */
    if (newchrled.major) { /* 定義了設備號 */
        newchrled.devid = MKDEV(newchrled.major, 0);
        ret = register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
        if (ret < 0) {
            pr_err("cannot register %s char driver [ret=%d]\n", NEWCHRLED_NAME, NEWCHRLED_CNT);
            goto fail_map;
        }
    } else { /* 沒有定義設備號 */
        ret = alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME); /* 申請設備號 */
        if (ret < 0) {
            pr_err("%s Couldn't alloc_chrdev_region, ret=%d\r\n", NEWCHRLED_NAME, ret);
            goto fail_map;
        }
        newchrled.major = MAJOR(newchrled.devid); /* 獲取主設備號 */
        newchrled.minor = MINOR(newchrled.devid); /* 獲取次設備號 */
    }
    printk("newchrled major=%d, minor=%d\r\n", newchrled.major, newchrled.minor);

    /* 2、初始化cdev */
    cdev_init(&newchrled.cdev, &newchrled_fops);
    newchrled.cdev.owner = THIS_MODULE;

    /* 3、添加cdev */
    ret = cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);
    if (ret) {
        printk("cdev add failed!\r\n");
        goto fail_cdev;
    }

    /* 創建類和設備 */
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.class)) {
        ret = PTR_ERR(newchrled.class);
        goto fail_class;
    }

    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.device)) {
        ret = PTR_ERR(newchrled.device);
        goto fail_device;
    }

    return 0;

fail_device:
    class_destroy(newchrled.class);
fail_class:
    cdev_del(&newchrled.cdev);
fail_cdev:
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);
fail_map:
    led_unmap();
    return ret;
}

/*
 * @description : 驅動出口函數
 * @param : 無
 * @return : 無
 */
static void __exit led_exit(void) {
    led_unmap(); /* 取消映射 */

    /* 注銷字符設備驅動 */
    cdev_del(&newchrled.cdev); /* 刪除cdev */
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);

    /* 銷毀設備和類 */
    device_destroy(newchrled.class, newchrled.devid);
    class_destroy(newchrled.class);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");
