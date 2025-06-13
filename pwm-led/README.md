# PWM Linux Kernel Driver for LED Control

This Linux kernel module provides a platform driver and a `misc` character device interface for controlling PWM (Pulse Width Modulation) devices, typically used for LED brightness regulation. The driver allows dynamic adjustment of the duty cycle from user space.

## Features

- Platform driver bound via Device Tree using the compatible string `training,pwm_led`
- Exposes PWM control through a `misc` character device
- Accepts percentage values (0-100) via write operations
- Implements proper PWM state management
- Thread-safe operations with error checking
- Default 1ms PWM period (configurable in code)
- Initializes with 50% duty cycle

## Hardware Interface

The driver requires a PWM-capable device configured in the Device Tree with:

- A PWM controller reference
- Optional label for device naming

## Device Tree Example

```dts
servo@0 {
			compatible = "training,servo";
			pinctrl-names = "default";
			label = "pwm-servo0";
			pwms = <&pwm 0 1000000 0>;
			status = "okay";
};

&pwm {
	pinctrl-names = "default";
	pinctrl-0 = <&pwm_pins>;
	status = "okay";
};

pwm_pins: pwm_pins {
		brcm,pins = <18>;
		brcm,function = <BCM2835_FSEL_ALT5>;
};
```


