#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

static struct gpio_desc *dht11_gpiod;
int TemperatureHigh, HumidityHigh;

void SendStartSignal(void) {
    gpiod_direction_output(dht11_gpiod, 0);
    msleep(20);
    gpiod_set_value(dht11_gpiod, 1);
    gpiod_direction_input(dht11_gpiod);
}

int WaitForLow(void) {
    ktime_t start_time, end_time;
    s64 microseconds;

    start_time = ktime_get();
    while (gpiod_get_value(dht11_gpiod)) {
        end_time = ktime_get();
        microseconds = ktime_to_us(ktime_sub(end_time, start_time));
        if (microseconds > 1000) {
            pr_err("Time out WaitForLow!\n");
            break;
        }
    }
    end_time = ktime_get();
    return ktime_to_us(ktime_sub(end_time, start_time));
}

int WaitForHigh(void) {
    ktime_t start_time, end_time;
    s64 microseconds;

    start_time = ktime_get();
    while (!gpiod_get_value(dht11_gpiod)) {
        end_time = ktime_get();
        microseconds = ktime_to_us(ktime_sub(end_time, start_time));
        if (microseconds > 100) {
            pr_err("Time out WaitForHigh!\n");
            break;
        }
    }
    end_time = ktime_get();
    return ktime_to_us(ktime_sub(end_time, start_time));
}

u8 CalculateParity(u8 HumidityHigh, u8 HumidityLow, u8 TemperatureHigh, u8 TemperatureLow) {
    return (HumidityHigh + HumidityLow + TemperatureHigh + TemperatureLow);
}

void ProcessData(u64 data) {
    u8 HumidityLow, TemperatureLow, parity;
    HumidityHigh = (data >> 32) & 0xFF;
    HumidityLow = (data >> 24) & 0xFF;
    TemperatureHigh = (data >> 16) & 0xFF;
    TemperatureLow = (data >> 8) & 0xFF;
    parity = data & 0xFF;

    if (parity != CalculateParity(HumidityHigh, HumidityLow, TemperatureHigh, TemperatureLow))
        pr_err("Parity checksum failed.\n");
}

void Measure(void) {
    u64 data = 0;
    int LowTime, HighTime;

    SendStartSignal();
    WaitForLow();
    WaitForHigh();
    WaitForLow();

    for (int i = 0; i < 40; i++) {
        data <<= 1;
        LowTime = WaitForHigh();
        HighTime = WaitForLow();
        if (LowTime < HighTime)
            data |= 0x1;
    }

    WaitForHigh();
    gpiod_direction_output(dht11_gpiod, 1);
    ProcessData(data);
}

static ssize_t dht11_read(struct file *flip, char __user *buf, size_t len, loff_t *off) {
    char kbuf[100];

    if (*off != 0)
        return 0;

    pr_info("Start measure...\n");
    Measure();
    *off = 1;

    snprintf(kbuf, sizeof(kbuf), "%d", TemperatureHigh);
    if (copy_to_user(buf, kbuf, strlen(kbuf)))
        return -EFAULT;

    return strlen(kbuf);
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read  = dht11_read,
};

static struct miscdevice misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "dht11",
    .fops  = &fops,
    .mode  = 0666,
};

static int dht11_probe(struct platform_device *pdev) {
    int ret;

    pr_info("dht11_probe() is called.\n");

    dht11_gpiod = devm_gpiod_get(&pdev->dev, "data", GPIOD_OUT_HIGH);
    if (IS_ERR(dht11_gpiod)) {
        pr_err("Failed to get DHT11 GPIO from device tree.\n");
        return PTR_ERR(dht11_gpiod);
    }

    pr_info("GPIO acquired successfully.\n");

    ret = misc_register(&misc);
    if (ret) {
        pr_err("Failed to register misc device.\n");
        return ret;
    }

    return 0;
}

static void dht11_remove(struct platform_device *pdev) {
    misc_deregister(&misc);
    pr_info("dht11_remove() called.\n");
}

static const struct of_device_id ids[] = {
    {.compatible = "training,dht11"},
    {}
};
MODULE_DEVICE_TABLE(of, ids);

static struct platform_driver dht11_driver = {
    .probe  = dht11_probe,
    .remove = dht11_remove,
    .driver = {
        .name = "dht11_driver",
        .of_match_table = ids,
        .owner = THIS_MODULE,
    }
};

static int __init dht11_init(void) {
    return platform_driver_register(&dht11_driver);
}

static void __exit dht11_exit(void) {
    platform_driver_unregister(&dht11_driver);
}

module_init(dht11_init);
module_exit(dht11_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chiheb");
MODULE_DESCRIPTION("DHT11 Driver using gpiod for Linux 6.12+");
