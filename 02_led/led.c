#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>

#define LED_MAJOR  200
#define LED_NAME   "led"

#define LEDOFF  0
#define LEDON   1

/* 寄存器物理地址 */
#define PERIPH_BASE     		     	(0x40000000)
#define MPU_AHB4_PERIPH_BASE			(PERIPH_BASE + 0x10000000)
#define RCC_BASE        		    	(MPU_AHB4_PERIPH_BASE + 0x0000)	
#define RCC_MP_AHB4ENSETR				(RCC_BASE + 0XA28)

#define GPIOI_BASE						(MPU_AHB4_PERIPH_BASE + 0xA000)	
#define GPIOI_MODER      			    (GPIOI_BASE + 0x0000)	
#define GPIOI_OTYPER      			    (GPIOI_BASE + 0x0004)	
#define GPIOI_OSPEEDR      			    (GPIOI_BASE + 0x0008)	
#define GPIOI_PUPDR      			    (GPIOI_BASE + 0x000C)	
#define GPIOI_BSRR      			    (GPIOI_BASE + 0x0018)


/* 映射后的寄存器虚拟地址指针 */
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;
static void __iomem *GPIOI_MODER_PI;
static void __iomem *GPIOI_OTYPER_PI;
static void __iomem *GPIOI_OSPEEDR_PI;
static void __iomem *GPIOI_PUPDR_PI;
static void __iomem *GPIOI_BSRR_PI;


/* 地址映射 */
static void led_ioremap(void)
{
    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4);
    GPIOI_MODER_PI = ioremap(GPIOI_MODER, 4);
    GPIOI_OTYPER_PI = ioremap(GPIOI_OTYPER, 4);
    GPIOI_OSPEEDR_PI = ioremap(GPIOI_OSPEEDR, 4);
    GPIOI_PUPDR_PI = ioremap(GPIOI_PUPDR, 4);
    GPIOI_BSRR_PI = ioremap(GPIOI_BSRR, 4);
}

/* 取消映射 */
static void led_iounmap(void)
{
    iounmap(MPU_AHB4_PERIPH_RCC_PI);
    iounmap(GPIOI_MODER_PI);
    iounmap(GPIOI_OTYPER_PI);
    iounmap(GPIOI_OSPEEDR_PI);
    iounmap(GPIOI_PUPDR_PI);
    iounmap(GPIOI_BSRR_PI);
}

/* 打开或关闭LED */
void led_switch(u8 sta)
{
    u32 val = 0;

    if(sta == LEDON) {
        val = readl(GPIOI_BSRR_PI); 
        val &= ~(0x1 << 16); /* 将bit16零 */
        val |= (0x1 << 16);  /* 将bit16设置为1 */
        writel(val, GPIOI_BSRR_PI); 
    } else if(sta == LEDOFF) {
        val = readl(GPIOI_BSRR_PI); 
        val &= ~(0x1 << 0); /* 将bit0清零 */
        val |= (0x1 << 0);  /* 将bit0设置为1 */
        writel(val, GPIOI_BSRR_PI); 
    }
}

static int led_open(struct inode *inode, struct file *filp)
{
    int ret = 0;

    //printk("chrdevbase_open\r\n");
    return ret;    
}


static int led_release(struct inode *inode, struct file *filp)
{
    int ret = 0;

    //printk("chrdevbase_release\r\n");
    return ret;    
}

/*
 * @description		: 向设备写数据 
 * @param - filp 	: 设备文件，表示打开的文件描述符
 * @param - buf 	: 要写给设备写入的数据
 * @param - cnt 	: 要写入的数据长度
 * @param - offt 	: 相对于文件首地址的偏移
 * @return 			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf,
			  size_t cnt, loff_t *offt)
{
    int ret = 0;
    unsigned char databuf[1];
    unsigned char ledstat;

    ret = copy_from_user(databuf, buf, cnt);
    if(ret < 0) {
        printk("kernel write failed!\r\n");
        ret = -EFAULT;
    }
  
    ledstat = databuf[0]; /* 获取到应用传递进来的开关灯状态 */

    if(ledstat == LEDON) { /* 开灯 */
        led_switch(LEDON);
    } else if(ledstat == LEDOFF) { /* 关灯 */
        led_switch(LEDOFF);
    }
    
    return ret;
}


const struct file_operations led_fops = {
	.owner =   THIS_MODULE,
	.open =    led_open,
	.release = led_release,
	.write =   led_write,
};


/* 入口函数 */
static int __init led_init(void)
{
    int ret = 0;
    u32 val = 0;

    /* 1、寄存器地址映射 */
    led_ioremap();

    /* 2、是能GPIOI时钟*/
    val = readl(MPU_AHB4_PERIPH_RCC_PI); 
    val &= ~(0x1 << 8);  /* 清除bit8以前的设置 */
    val |= (0x1 << 8); /* 将bit8设置为1 */
    writel(val, MPU_AHB4_PERIPH_RCC_PI);    

    /* 3、将GPIOI_0设置为输出引脚 */ 
    val = readl(GPIOI_MODER_PI); 
    val &= ~(0x3 << 0); /* 将bit1和bit0清零 */
    val |= (0x1 << 0);  /* 将bit1和bit0设置为01 */
    writel(val, GPIOI_MODER_PI); 

    /* 4、 将GPIOI_0设置为推挽输出*/
    val = readl(GPIOI_OTYPER_PI); 
    val &= ~(0x1 << 0);  /* bit0清零 */
    writel(val, GPIOI_OTYPER_PI); 

    /* 5、 将GPIOI_0设置为超高速 */
    val = readl(GPIOI_OSPEEDR_PI); 
    val &= ~(0x3 << 0); /* 将bit1和bit0清零 */
    val |= (0x3 << 0);  /* 将bit1和bit0设置为11 */
    writel(val, GPIOI_OSPEEDR_PI); 

    /* 6、 将GPIOI_0设置为上拉 */
    val = readl(GPIOI_PUPDR_PI); 
    val &= ~(0x3 << 0); /* 将bit1和bit0清零 */
    val |= (0x1 << 0);  /* 将bit1和bit0设置为10 */
    writel(val, GPIOI_PUPDR_PI); 

    /* 7、将GPIOI_0默认输出高电平，关闭LED灯 */
    val = readl(GPIOI_BSRR_PI); 
    val &= ~(0x1 << 0); /* 将bit0清零 */
    val |= (0x1 << 0);  /* 将bit0设置为1 */
    writel(val, GPIOI_BSRR_PI); 










    /* 注册字符设备 */
    printk("led_init\r\n");

    /* 向内核注册字符设备 */
    ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if (ret < 0) {
		printk("chrdevbase driver register failed!\r\n");
        goto faile_register;
	}

    return 0;

faile_register:
    return -EIO;
}

/* 出口函数 */
static void __exit led_exit(void)
{

    /* 1、取消地址映射 */
    led_iounmap();

    printk("led_exit\r\n");  

    /* 注销字符设备 */
    unregister_chrdev(LED_MAJOR, LED_NAME);  
}

/* 驱动模块的注册与卸载 */
module_init(led_init);
module_exit(led_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("ALIENTEK");
MODULE_INFO(intree, "Y");