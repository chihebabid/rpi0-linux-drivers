# PWM Linux Kernel Driver for Servo Motor Control

This Linux kernel module implements a simple platform driver to control a servo motor using the PWM (Pulse Width Modulation) framework. The driver exposes a `misc` character device that allows user-space programs to control the servo angle by writing values from 0 to 180 degrees.

## Features

- Platform driver bound via Device Tree using the compatible string `training,servo`
- Controls servo position by writing degrees (0–180) to a character device
- Uses the Linux PWM subsystem to set duty cycles
- Exposes configurable parameters: `MIN_DUTY_NS`, `MAX_DUTY_NS`, and `PERIOD_NS`
- Registers a `misc` device for easy user-space access (e.g. `/dev/servo0`)
- Thread-safe and robust PWM state handling

## Module Parameters

| Parameter       | Type      | Default    | Description                          |
|----------------|-----------|------------|--------------------------------------|
| `MIN_DUTY_NS`  | `uint`    | 500000     | Minimum duty cycle in nanoseconds    |
| `MAX_DUTY_NS`  | `uint`    | 2500000    | Maximum duty cycle in nanoseconds    |
| `PERIOD_NS`    | `ullong`  | 20000000   | PWM signal period in nanoseconds     |

You can override these parameters when loading the module:
```bash
sudo insmod pwm-servo.ko MIN_DUTY_NS=1000000 MAX_DUTY_NS=2000000 PERIOD_NS=2000000
```

## Hardware Interface

The driver uses the following GPIOs defined in the Device Tree:

- `data`: Serial data input to the 74HC595
- `clk`: Clock input to the 74HC595
- `latch`: Latch pin to transfer data to output
- `digit1`: Enable pin for first 7-segment digit
- `digit2`: Enable pin for second 7-segment digit

## Device Tree Example

```dts
servo@0 {
    compatible = "training,servo";
    label = "servo0";
    pwms = <&pwm 0 20000000 0>;
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

## Usage

To set the servo angle, write a value between 0 and 180 into the character device:

```bash
echo 90 > /dev/servo0
```
