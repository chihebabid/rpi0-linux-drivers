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
#include <linux/math64.h>

#define TIMEOUT 220*60

struct myhc04_misc_t {
	struct miscdevice mymisc_device;
	const char *device_name;
	struct gpio_desc * gpio_trigger;
	struct gpio_desc * gpio_echo;
	u8 value;
};


static int pf_probe(struct platform_device *pdev);
static void pf_remove(struct platform_device *pdev);
static ssize_t myhc04_read(struct file *flip, char __user *buf, size_t len, loff_t *off);
static int measure(struct myhc04_misc_t *);

static const struct file_operations myhc04_fops = {
	.owner = THIS_MODULE,
	.read  = myhc04_read,
};

static const struct of_device_id match_array_hc04[] = {
	{.compatible = "training,hc-sr04"},
	{}
};
MODULE_DEVICE_TABLE(of, match_array_hc04);

static struct platform_driver pf_driver = {
	.probe  = pf_probe,
	.remove = pf_remove,
	.driver = {
		.name = "hc_sr04_driver",
		.of_match_table = match_array_hc04,
		.owner = THIS_MODULE,
	}
};

static int pf_probe(struct platform_device *pdev) {
	int ret_val;
	struct myhc04_misc_t *mydev;
	dev_info(&pdev->dev,"pf_probe() function is called.\n");
	mydev = devm_kzalloc(&pdev->dev, sizeof(struct myhc04_misc_t), GFP_KERNEL);
	if (!mydev) {
		return -ENOMEM;
	}
	of_property_read_string(pdev->dev.of_node, "label", &mydev->device_name);

	mydev->mymisc_device.minor = MISC_DYNAMIC_MINOR;
	mydev->mymisc_device.name = mydev->device_name;
	mydev->mymisc_device.fops = &myhc04_fops;
	mydev->mymisc_device.mode=0666;
	mydev->gpio_trigger=devm_gpiod_get(&pdev->dev,"trigger",GPIOD_OUT_LOW);
	if (IS_ERR(mydev->gpio_trigger)) {
		dev_err(&pdev->dev,"Failed to get DATA GPIO from device tree.\n");
		return PTR_ERR(mydev->gpio_trigger);
	}
	mydev->gpio_echo=devm_gpiod_get(&pdev->dev,"echo",GPIOD_IN);
	if (IS_ERR(mydev->gpio_echo)) {
		dev_err(&pdev->dev,"Failed to get CLK GPIO from device tree.\n");
		return PTR_ERR(mydev->gpio_echo);
	}


	ret_val = misc_register(&mydev->mymisc_device);
	if (ret_val) {
		dev_err(&pdev->dev,"Failed to register misc device.\n");
		return ret_val; /* misc_register returns 0 if success */
	}

	platform_set_drvdata(pdev, mydev);
	return 0;
}

static void pf_remove(struct platform_device *pdev) {
	dev_info(&pdev->dev,"pf_remove() called.\n");
	struct myhc04_misc_t  *misc_device = platform_get_drvdata(pdev);

	misc_deregister(&misc_device->mymisc_device);
}


static int measure(struct myhc04_misc_t  *myhc04_dev) {
    u64 distance;
    ktime_t start_time, end_time, duration;
    s64 microseconds;
    // Trigger
    gpiod_set_value(myhc04_dev->gpio_trigger,1);
    udelay(10);
    gpiod_set_value(myhc04_dev->gpio_trigger,0);

    start_time = ktime_get();
    while (gpiod_get_value(myhc04_dev->gpio_echo) != 1) {
        end_time=ktime_get();
        duration = ktime_sub(end_time, start_time);
        microseconds = ktime_to_us(duration);
        if (microseconds > TIMEOUT) return -1;
    }
    start_time = ktime_get();
    while (gpiod_get_value(myhc04_dev->gpio_echo) == 1) {
        end_time=ktime_get();
        duration = ktime_sub(end_time, start_time);
        microseconds = ktime_to_us(duration);
        if (microseconds > TIMEOUT) return -1;
    }
    distance = (microseconds * 340 *50) ;
    distance=div_u64(distance, 100000);
    return distance;

}

static ssize_t myhc04_read(struct file *flip, char *buf, size_t len, loff_t *off) {
    char kbuf[250];
    int distance;
    if (*off==0) {
        (*off)++;
        struct myhc04_misc_t  *myhc04_dev = container_of(flip->private_data, struct myhc04_misc_t, mymisc_device);
        pr_info("Start measure...\n");
        distance=measure(myhc04_dev);
        sprintf(kbuf,"Distance : %d.%d\n",distance/10,distance%10);
        if (!copy_to_user(buf,kbuf,strlen(kbuf))) return strlen(kbuf);
        pr_err("Error copying to user space.\n");
        return 0;
    }
    return 0;
}

static int __init pf_init(void) {
    return platform_driver_register(&pf_driver);
}

static void __exit pf_exit(void) {
    platform_driver_unregister(&pf_driver);
}

module_init(pf_init);
module_exit(pf_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Chiheb Ameur ABID");
MODULE_DESCRIPTION("Linux kernel driver for HC-SR04 ultrasonic distance sensor (compatible with kernel 6.12+)");
MODULE_VERSION("1.0");
