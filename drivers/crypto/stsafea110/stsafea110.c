#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/cdev.h>


/* 定义 stsafea110 设备*/
struct stsafea110_device {
    dev_t stsafea110_devno;
    int major;
    int minor;
    struct cdev stsafea110_cdev;
    struct class *stsafea110_class;
    struct device *stsafea110_device;
    struct i2c_client *stsafea110_client;
    int fd;
};

/* stsafea110设备结构体 */
struct stsafea110_device stsafea110_dev;

/* test buffer */

static u8 receive_buf[12];


/* 将i2c读写写入到自定义的读写函数中, 调用write和read函数的时候再去调用这几个函数 */
/* 标准文件操作函数中的write和read主要负责传参 */
static __maybe_unused int stsafea110_write(void /*struct i2c_client *client, u8 *buf, u16 len */)
{

    int ret;
    printk("this is i2c_write_stsafea110!\r\n");
    // ret = i2c_master_send(stsafea110_dev.stsafea110_client, test_buf, 4);
    if(ret < 0) {
        printk("i2c_master_send failed!\r\n");
        return ret;
    }
    return 0;
}

/* 主要负责接受文件操作中read传来的参数 */
static __maybe_unused int stsafea110_read(void/* u8 *buf, u16 len */)
{
    int ret;
    size_t i;
    printk("this is i2c_read_stsafea110!\r\n");
    ret = i2c_master_recv(stsafea110_dev.stsafea110_client, receive_buf, 12);
    // 将这个receive_buf 打印出来, 就可以测试是否可以使用
    if(ret < 0) {
        printk("i2c_master_recv failed!\r\n");
        return ret;
    }
    for (i = 0; i < 12; i++) {
        printk(KERN_INFO "receive_buf[%zu] = 0x%02x\n", i, receive_buf[i]);
    }


    return 0;
}




/* character device operation set, open function implementation */
static int stsafea110_open_operation(struct inode *inode, struct file *filp)
{
    printk("this is stsafea110_open_operatioin!\r\n");
    filp->private_data = &stsafea110_dev;
    return 0;
}

/* character device operation set, read function implementation */
static ssize_t stsafea110_read_operation(struct file *flip, char __user *buf, size_t count, loff_t *loff)
{

    int ret;
    int i;
    struct stsafea110_device *dev = (struct stsafea110_device *)flip->private_data; 
    struct i2c_client *client = dev->stsafea110_client;
    char *kbuf;

    printk("the parameter count is %zu\r\n", count);
    /* 1. allocating kernel buffer */
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        printk("kmalloc failed !\r\n");
        return -ENOMEM;
    }

    /* 2. use i2c_master_recv to receive data from i2c slave device */
    ret = i2c_master_recv(client, kbuf, count);
    if (ret < 0) {
        kfree(kbuf);
        printk("i2c_master_recv failed !\r\n");
        return ret;
    }
    for (i = 0; i < count; i++) {
        printk(KERN_INFO "kbuf[%d] = 0x%02x\n", i, kbuf[i]);
    }


    /* 3. copy kbuf to user buf */
    if (copy_to_user(buf, kbuf, count)) {
        kfree(kbuf);
        printk("copy_to_user failed !\r\n");
        return -EFAULT;
    }

    /* 4. free the kernel buffer */
    kfree(kbuf);

    /* 5. return the number of bytes actually read */
    return ret;
}

/* character device operation set, write function implementation */
/**
 * @brief 向 STS AE FA110 设备写入数据的操作函数
 *
 * 该函数负责将用户空间的数据写入到 STS AE FA110 设备中。
 *
 * @param filp 文件指针，指向操作的文件对象
 * @param buf 用户空间的数据缓冲区指针
 * @param count 要写入的数据字节数
 * @param loff 文件偏移量指针，用于指定写入位置
 *
 * @return 返回实际写入的字节数，如果写入失败则返回负值
 */
static ssize_t stsaefa110_write_operation(struct file * filp, const char __user *buf, size_t count, loff_t *loff)
{
    int ret;
    /* extract the i2c_client pointer from filp */
    struct stsafea110_device *dev = (struct stsafea110_device *)filp->private_data;
    struct i2c_client *client = dev->stsafea110_client;
    char *kbuf;

    /* 1. allocating kernel buffers */
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        printk("kmalloc failed !\r\n");
        return -ENOMEM;
    }
    
    /* 2. copy data from user space to kernel space */
    ret = copy_from_user(kbuf, buf, count);
    if (ret != 0) {
        kfree(kbuf);
        printk("copy_from_user failed !\r\n");
        return -EFAULT;
    }

    /* 3. call i2c_master_send to send data to the i2c salve device */
    ret = i2c_master_send(client, kbuf, count);
    if (ret < 0) {
        kfree(kbuf);
        printk("i2c_master_send failed !\r\n");
        return ret;
    }

    /* 4. free the kernel buffer */
    kfree(kbuf);

    /* 5. returns the number of bytes atually sent */
    return ret;


}

/* character device operation set, release function implementation */
static int stsafea110_release_operation(struct inode *node, struct file *flip)
{
    printk("this is stsafea110_release_operation !\r\n");

    return 0;
}

/* character device operation set, 使用系统调用打开文件和进行读写的时候调用 */
static __maybe_unused struct file_operations stsafea110_chr_dev_fops = {
    .owner = THIS_MODULE,
    .open = stsafea110_open_operation,
    .read = stsafea110_read_operation,
    .write = stsaefa110_write_operation,
    .release = stsafea110_release_operation,
};


/* probe function, use to initial the device */
static int stsafea110_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    printk(KERN_INFO "This is stsafea110_probe !\n");
    printk(KERN_INFO "the stsafea110_addr is 0x%02x\n", client->addr);
    stsafea110_dev.stsafea110_client = client;
    /* 1. create device number */
    ret = alloc_chrdev_region(&stsafea110_dev.stsafea110_devno, 0, 1, "stsafea110");   
    if (ret < 0) {
        printk("alloc_chrdev_region failed !\r\n");
        return -1;
    } 

    printk("alloc_chrdev_region success !\r\n");

    stsafea110_dev.major = MAJOR(stsafea110_dev.stsafea110_devno); /* get major devnumber */
    stsafea110_dev.minor = MINOR(stsafea110_dev.stsafea110_devno); /* get minor dev_number */

    printk("major = %d, minor = %d\r\n", stsafea110_dev.major, stsafea110_dev.minor);

    stsafea110_dev.stsafea110_cdev.owner = THIS_MODULE;
    cdev_init(&stsafea110_dev.stsafea110_cdev, &stsafea110_chr_dev_fops);

    /* 3. add the cdev, and complete the character device registration to the kernle  */
    ret = cdev_add(&stsafea110_dev.stsafea110_cdev, stsafea110_dev.stsafea110_devno, 1);
    if (ret < 0) {
        printk("cdev_add failed !\r\n");
        return -1;
    }

    stsafea110_dev.stsafea110_class = class_create(THIS_MODULE, "stsafea110_class");
    if (IS_ERR(stsafea110_dev.stsafea110_class)) {
        ret = PTR_ERR(stsafea110_dev.stsafea110_class);
        printk("class_create failed !\r\n");
        return ret;
    }

    stsafea110_dev.stsafea110_device = device_create(stsafea110_dev.stsafea110_class, NULL, stsafea110_dev.stsafea110_devno, NULL, "stsafea110_device");
    if (IS_ERR(stsafea110_dev.stsafea110_device)) {
        ret = PTR_ERR(stsafea110_dev.stsafea110_device);
        printk("device_create failed !\r\n");
        return ret;
    }


    return 0;
}


static int stsafea110_remove(struct i2c_client *client)
{
    printk("this is stsafea110_remove !\r\n");
    device_destroy(stsafea110_dev.stsafea110_class, stsafea110_dev.stsafea110_devno);
    cdev_del(&stsafea110_dev.stsafea110_cdev);
    class_destroy(stsafea110_dev.stsafea110_class);
    unregister_chrdev_region(stsafea110_dev.stsafea110_devno, 1);

    return 0;
}

static const struct of_device_id stsafea110_of_match[] = {
    { .compatible = "rk3588, stsafea110"},
    {},
};


/* i2c驱动结构体, 检测到设备的时候可以自动执行probe和remove函数 */
struct i2c_driver stsafea110_driver = {
    .probe = stsafea110_probe,
    .remove = stsafea110_remove,
    .driver = {
        .name = "i2c_driver_stsafea110",
        .of_match_table = stsafea110_of_match,
    },
};


static int __init stsafea110_init(void)
{
    int ret = 0;
    ret = i2c_add_driver(&stsafea110_driver);

    if (ret < 0) {
        printk("stsafea110_init is failed!\r\n");
        return ret;
    }
    return 0;
}

static void  stsafea110_exit(void)
{
    i2c_del_driver(&stsafea110_driver);

}

/* 模块初始化和退出函数 */
module_init(stsafea110_init);
module_exit(stsafea110_exit);

MODULE_LICENSE("GPL");
