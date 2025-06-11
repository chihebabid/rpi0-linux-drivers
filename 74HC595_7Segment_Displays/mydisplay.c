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
#include <linux/kthread.h>

struct mydev_misc_t {
	struct miscdevice mymisc_device;
	const char *device_name;
	struct gpio_desc * gpio_data;
	struct gpio_desc * gpio_clk;
	struct gpio_desc * gpio_latch;
	struct gpio_desc * gpio_digit1;
	struct gpio_desc * gpio_digit2;
	struct task_struct *thread;
	u8 digit1,digit2;
};

static int pf_probe(struct platform_device *pdev);
static void pf_remove(struct platform_device *pdev);
static ssize_t my74595_read(struct file *flip, char __user *buf, size_t len, loff_t *off);
static ssize_t my74595_write(struct file *flip,const char *buf,size_t len,loff_t *off);
static void write8Bits(struct mydev_misc_t  *my_misc_dev,u8 value);
static void prepare(struct mydev_misc_t  *my_misc_dev);
static void update(struct mydev_misc_t  *my_misc_dev);
static int manage_digits(void *arg);
static u8 digit_to_7segments(u8 d);



static const struct file_operations mydev_fops = {
	.owner = THIS_MODULE,
	.read  = my74595_read,
	.write = my74595_write,
};



static const struct of_device_id match_array_74595[] = {
	{.compatible = "training,display"},
	{}
};
MODULE_DEVICE_TABLE(of, match_array_74595);

static struct platform_driver pf_driver = {
	.probe  = pf_probe,
	.remove = pf_remove,
	.driver = {
		.name = "74595_driver",
		.of_match_table = match_array_74595,
		.owner = THIS_MODULE,
	}
};







static int __init pf_init(void) {
    return platform_driver_register(&pf_driver);
}

static void __exit pf_exit(void) {
    platform_driver_unregister(&pf_driver);
}

module_init(pf_init);
module_exit(pf_exit);

static int pf_probe(struct platform_device *pdev) {
	int ret_val;
	struct mydev_misc_t *mydev;
	dev_info(&pdev->dev,"pf_probe() function is called.\n");
	mydev = devm_kzalloc(&pdev->dev, sizeof(struct mydev_misc_t), GFP_KERNEL);
	of_property_read_string(pdev->dev.of_node, "label", &mydev->device_name); /* Different label for each misc device*/

	mydev->mymisc_device.minor = MISC_DYNAMIC_MINOR;
	mydev->mymisc_device.name = mydev->device_name;
	mydev->mymisc_device.fops = &mydev_fops;
	mydev->mymisc_device.mode=0666;
	mydev->gpio_data=devm_gpiod_get(&pdev->dev,"data",GPIOD_OUT_LOW);
	if (IS_ERR(mydev->gpio_data)) {
		dev_err(&pdev->dev,"Failed to get DATA GPIO from device tree.\n");
		return PTR_ERR(mydev->gpio_data);
	}
	mydev->gpio_clk=devm_gpiod_get(&pdev->dev,"clk",GPIOD_OUT_LOW);
	if (IS_ERR(mydev->gpio_clk)) {
		dev_err(&pdev->dev,"Failed to get CLK GPIO from device tree.\n");
		return PTR_ERR(mydev->gpio_clk);
	}
	mydev->gpio_latch=devm_gpiod_get(&pdev->dev,"latch",GPIOD_OUT_LOW);
	if (IS_ERR(mydev->gpio_latch)) {
		dev_err(&pdev->dev,"Failed to get LATCH GPIO from device tree.\n");
		return PTR_ERR(mydev->gpio_latch);
	}

	mydev->gpio_digit1=devm_gpiod_get(&pdev->dev,"digit1",GPIOD_OUT_HIGH);
	if (IS_ERR(mydev->gpio_digit1)) {
		dev_err(&pdev->dev,"Failed to get DIGIT1 GPIO from device tree.\n");
		return PTR_ERR(mydev->gpio_digit1);
	}

	mydev->gpio_digit2=devm_gpiod_get(&pdev->dev,"digit2",GPIOD_OUT_LOW);
	if (IS_ERR(mydev->gpio_digit2)) {
		dev_err(&pdev->dev,"Failed to get DIGIT2 GPIO from device tree.\n");
		return PTR_ERR(mydev->gpio_digit2);
	}

	// Start the thread
	mydev->thread=kthread_run(manage_digits, mydev, "manage_digits_thread");
	if (IS_ERR(mydev->thread)) {
		dev_err(&pdev->dev, "Failed to create kernel thread.\n");
		return PTR_ERR(mydev->thread);
	}

	ret_val = misc_register(&mydev->mymisc_device);
	if (ret_val) {
		dev_err(&pdev->dev,"Failed to register misc device.\n");
		return ret_val; /* misc_register returns 0 if success */
	}
	mydev->digit1=0;
	mydev->digit2=0;
	platform_set_drvdata(pdev, mydev);
	return 0;
}

static void pf_remove(struct platform_device *pdev) {
	dev_info(&pdev->dev,"pf_remove() called.\n");
	struct mydev_misc_t  *misc_device = platform_get_drvdata(pdev);
	if (misc_device->thread) {
		kthread_stop(misc_device->thread);
	}
	misc_deregister(&misc_device->mymisc_device);
}


static ssize_t my74595_read(struct file *flip, char __user *buf, size_t len, loff_t *off) {
	char kbuf[100];

	if (*off != 0)
		return 0;
	*off = 1;


	return strlen(kbuf);
}

static ssize_t my74595_write(struct file *flip,const char *buf,size_t len,loff_t *off) {
	pr_info("mydev_write() is called.\n");
	struct mydev_misc_t  *my_misc_dev = container_of(flip->private_data, struct mydev_misc_t, mymisc_device);
	if (len>=2) {
		get_user(my_misc_dev->digit2, buf);
		my_misc_dev->digit2-='0';
		get_user(my_misc_dev->digit1, buf+1);
		my_misc_dev->digit1-='0';
	}
	else if (len==1) {
		my_misc_dev->digit2=0;
		get_user(my_misc_dev->digit1, buf);
		my_misc_dev->digit1-='0';
	}
	else {
		my_misc_dev->digit1=0;
		my_misc_dev->digit2=0;
	}
	pr_info("Values : %d %d\n",my_misc_dev->digit2,my_misc_dev->digit1);
	return len;
}

static void write8Bits(struct mydev_misc_t  *my_misc_dev,u8 value) {
	u8 bit,_byte=value;
	for (int i=0;i<8;++i) {
		bit=!!(_byte & 0x80);
		_byte=_byte<<1;
		gpiod_set_value(my_misc_dev->gpio_data,bit);
		ndelay(100);
		gpiod_set_value(my_misc_dev->gpio_clk,1);
		ndelay(100);
		gpiod_set_value(my_misc_dev->gpio_clk,0);
		ndelay(100);
	}
}

static void prepare(struct mydev_misc_t  *my_misc_dev) {
	gpiod_set_value(my_misc_dev->gpio_clk,0);
	gpiod_set_value(my_misc_dev->gpio_latch,0);
	ndelay(100);
}

static void update(struct mydev_misc_t  *my_misc_dev) {
	gpiod_set_value(my_misc_dev->gpio_latch,1);
	ndelay(100);
}


static int manage_digits(void *arg) {
	struct mydev_misc_t  *my_misc_dev=(struct mydev_misc_t  *)arg;
	while (!kthread_should_stop()) {
		gpiod_set_value(my_misc_dev->gpio_digit1,0);
		gpiod_set_value(my_misc_dev->gpio_digit2,0);
		prepare(my_misc_dev);
		write8Bits(my_misc_dev,digit_to_7segments(my_misc_dev->digit2));
		update(my_misc_dev);
		gpiod_set_value(my_misc_dev->gpio_digit1,0);
		gpiod_set_value(my_misc_dev->gpio_digit2,1);
		usleep_range(250, 350);
		gpiod_set_value(my_misc_dev->gpio_digit1,0);
		gpiod_set_value(my_misc_dev->gpio_digit2,0);
		prepare(my_misc_dev);
		write8Bits(my_misc_dev,digit_to_7segments(my_misc_dev->digit1));
		update(my_misc_dev);
		gpiod_set_value(my_misc_dev->gpio_digit2,0);
		gpiod_set_value(my_misc_dev->gpio_digit1,1);
		usleep_range(250, 350);
	}
	return 0;
}


static const u8 seg_table[10] = {
	0b00111111, // 0
	0b00000110, // 1
	0b01011011, // 2
	0b01001111, // 3
	0b01100110, // 4
	0b01101101, // 5
	0b01111101, // 6
	0b00000111, // 7
	0b01111111, // 8
	0b01101111  // 9
};

static u8 digit_to_7segments(u8 d) {
	return (d < 10) ? seg_table[d] : 0b01111001; // Default 'E' for error
}



MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chiheb");
MODULE_DESCRIPTION("Driver for 74HC595 shift register controlling 7-segment displays for Linux 6.12+");
