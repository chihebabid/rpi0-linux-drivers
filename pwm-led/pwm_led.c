#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/module.h>
#include <linux/kernel.h>


struct mypwm_misc_t {
	struct miscdevice mymisc_device;
	const char *device_name;
	struct pwm_device *pwm;
};

static int pwm_platform_probe(struct platform_device *pdev);
static void pwm_platform_remove(struct platform_device *pdev);
static ssize_t mypwm_read(struct file *flip, char __user *buf, size_t len, loff_t *off);
static ssize_t mypwm_write(struct file *flip,const char *buf,size_t len,loff_t *off);


static const struct file_operations mydev_fops = {
	.owner = THIS_MODULE,
	.read  = mypwm_read,
	.write = mypwm_write,
};


static ssize_t mypwm_read(struct file *flip, char __user *buf, size_t len, loff_t *off) {
	return 0;
}
static ssize_t mypwm_write(struct file *flip,const char *buf,size_t len,loff_t *off) {
	pr_info("mydev_write() is called.\n");
		struct mypwm_misc_t  *mydev = container_of(flip->private_data, struct mypwm_misc_t, mymisc_device);
		char kbuf[10];
		size_t to_copy = min(len, sizeof(kbuf) - 1);
		int duty_percent;
		u64 period_ns = 1000000; // 1 ms in nanoseconds
		struct pwm_state state;

		if (copy_from_user(kbuf, buf, to_copy)) {
		   return -EFAULT;
		}

		kbuf[to_copy] = '\0';

		if (kstrtoint(kbuf, 10, &duty_percent)) {
			pr_err("Failed to parse input\n");
			return -EINVAL;
		}

		if (duty_percent < 0 || duty_percent > 100) {
			pr_err("Duty cycle must be between 0 and 100\n");
			return -EINVAL;
		}

		pwm_get_state(mydev->pwm, &state);
		state.period = period_ns;
		state.duty_cycle = (u64)(period_ns * duty_percent / 100);
		state.enabled = true;

		if (pwm_apply_might_sleep(mydev->pwm, &state)) {
			pr_err("Failed to apply new PWM configuration\n");
			return -EIO;
		}

		pr_info("Duty cycle set to %d%%\n", duty_percent);
		return len;
}


static int pwm_platform_probe(struct platform_device *pdev)
{
	int ret_val;
	struct mypwm_misc_t *mydev;
	dev_info(&pdev->dev,"pf_probe() function is called.\n");
	mydev = devm_kzalloc(&pdev->dev, sizeof(struct mypwm_misc_t), GFP_KERNEL);
	of_property_read_string(pdev->dev.of_node, "label", &mydev->device_name);

	struct pwm_state state;

    mydev->pwm = devm_pwm_get(&pdev->dev, NULL);
    if (IS_ERR(mydev->pwm)) {
        dev_err(&pdev->dev, "Failed to get PWM\n");
        return PTR_ERR(mydev->pwm);
    }

    pwm_get_state(mydev->pwm, &state);
    state.period = 1000000;      // 1 ms
    state.duty_cycle = 500000;   // 50%
    state.enabled = true;

    ret_val=pwm_apply_might_sleep(mydev->pwm, &state);
    if (ret_val) {
    	dev_err(&pdev->dev, "Failed to apply pwm config\n");
    	return ret_val;
    }

    mydev->mymisc_device.minor = MISC_DYNAMIC_MINOR;
    mydev->mymisc_device.name = mydev->device_name;
    mydev->mymisc_device.fops = &mydev_fops;
    mydev->mymisc_device.mode=0666;
    ret_val = misc_register(&mydev->mymisc_device);
	if (ret_val) {
		dev_err(&pdev->dev,"Failed to register misc device.\n");
		return ret_val; /* misc_register returns 0 if success */
	}

	platform_set_drvdata(pdev, mydev);
    return 0;
}

static void pwm_platform_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev,"pwm_platform_remove() called.\n");
	struct mypwm_misc_t  *mydev = platform_get_drvdata(pdev);
	misc_deregister(&mydev->mymisc_device);
	pwm_disable(mydev->pwm);
}

static const struct of_device_id pwm_of_ids[] = {
    { .compatible = "training,pwm_led", },
    {}
};
MODULE_DEVICE_TABLE(of, pwm_of_ids);

static struct platform_driver pwm_platform_driver = {
    .probe  = pwm_platform_probe,
    .remove = pwm_platform_remove,
    .driver = {
        .name = "pwm-led",
        .of_match_table = pwm_of_ids,
    },
};

static int __init pf_init(void) {
    return platform_driver_register(&pwm_platform_driver);
}

static void __exit pf_exit(void) {
    platform_driver_unregister(&pwm_platform_driver);
}

module_init(pf_init);
module_exit(pf_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chiheb");
MODULE_DESCRIPTION("Simple platform driver using PWM");
