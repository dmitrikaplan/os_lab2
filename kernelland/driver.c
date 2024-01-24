#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <uapi/linux/bpf.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/device/class.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/part_stat.h>
#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/kallsyms.h>
#include <linux/ctype.h>
#include <linux/device-mapper.h>
#include "kallsyms.h"

#include "../common.h"

dev_t dev = 0;
static struct class *dev_class;
static struct cdev iostat_cdev;

static int      iostat_open(struct inode *inode, struct file *file);
static int      iostat_release(struct inode *inode, struct file *file);
static long     iostat_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = iostat_open,
    .unlocked_ioctl = iostat_ioctl,
    .release        = iostat_release,
};

static int iostat_open(struct inode *inode, struct file *file) {
    pr_info("Device File Opened...!!!\n");
    return 0;
}
static int iostat_release(struct inode *inode, struct file *file) {
    pr_info("Device File Closed...!!!\n");
    return 0;
}

//украдено из genhd.c
// общий драйвер для блочных устройств
// Проходит по всем ядрам и собирает статистику устройств
static void part_stat_read_all(struct block_device *part,
                struct disk_stats *stat) {
        int cpu;
        memset(stat, 0, sizeof(struct disk_stats));
        for_each_possible_cpu(cpu) {
                struct disk_stats *ptr = per_cpu_ptr(part->bd_stats, cpu);
                int group;

                for (group = 0; group < NR_STAT_GROUPS; group++) {
                        stat->nsecs[group] += ptr->nsecs[group];
                        stat->sectors[group] += ptr->sectors[group];
                        stat->ios[group] += ptr->ios[group];
                        stat->merges[group] += ptr->merges[group];
                }
                stat->io_ticks += ptr->io_ticks;
        }
}

// out_iter - сколько уже вывели
// output - указатель на буфер данных
struct CallbackContext {
    uint64_t out_iter;
    struct Message __user *output;
};

int iter_callback(struct device*dev, void* context_void) {
    struct CallbackContext *context = context_void;
    if (context->out_iter >= MESSAGE_STORAGE_SIZE){
        return 1; // останавливает итерацию
    }

    // для того, чтобы получить название диска
    struct gendisk *gp = dev_to_disk(dev);
    // для получения статистики
    struct block_device *hd = dev_to_bdev(dev);
    // пропускаем разделы дисков
    if (hd != NULL && gp != NULL) {
        if (bdev_is_partition(hd) || !bdev_nr_sectors(hd)) {
            return 0; //  продолжает итерации
        }
        // структура для статистики
        struct disk_stats stat;
        part_stat_read_all(hd, &stat);

        struct Message msg = {0};
        memmove(msg.name, gp->disk_name, min(sizeof(gp->disk_name), sizeof(msg.name)));
        msg.name[sizeof(msg.name) - 1] = '\0';
        msg.rd = stat.ios[STAT_READ];
        msg.rd_sectors = stat.sectors[STAT_READ];
        msg.wr = stat.ios[STAT_WRITE];
        msg.wr_sectors = stat.sectors[STAT_WRITE];
        if (copy_to_user(&context->output[context->out_iter++], &msg, sizeof(msg))) {
            pr_err("copy_to_user() failure within iostat_ioctl");
        }
    }
    return 0;
}

long iostat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct CallbackContext context = {0, (void*)arg};
    class_for_each_device(BLOCK_CLASS, 0, &context, iter_callback);
    return context.out_iter;
}

static int __init iostat_driver_init(void) {
    /*выделение типа устройства*/
    if((alloc_chrdev_region(&dev, 0, 1, "iostat_Dev")) <0){
            pr_err("Не получилось выделить чардев :(\n");
            return -1;
    }
    pr_info("Major = %d Minor = %d \n",MAJOR(dev), MINOR(dev));

    /*Ассоциирование функций и устройства*/
    cdev_init(&iostat_cdev,&fops);

    /*Добавление устройства в систему*/
    if((cdev_add(&iostat_cdev,dev,1)) < 0){
        pr_err("Не получилось добавить устройство :(\n");
        goto r_class;
    }

    /*Выделение класса под наш драйвер*/
    if(IS_ERR(dev_class = class_create("iostat_class"))){
        pr_err("Не получилось создать класс :(\n");
        goto r_class;
    }

    /*Создание файла в /proc*/
    if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"iostat_device"))){
        pr_err("Файл не создался :(\n");
        goto r_device;
    }
    pr_info("Iostat driver installed\n");
    return 0;

r_device:
        class_destroy(dev_class);
r_class:
        unregister_chrdev_region(dev,1);
        return -1;
}
static void __exit iostat_driver_exit(void) {
        device_destroy(dev_class,dev);
        class_destroy(dev_class);
        cdev_del(&iostat_cdev);
        unregister_chrdev_region(dev, 1);
        pr_info("Iostat driver uninstalled\n");
}
 
module_init(iostat_driver_init); // insmod
module_exit(iostat_driver_exit); // rmmod
MODULE_LICENSE("GPL");