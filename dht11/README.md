# 🧪 DHT11 Linux Kernel Driver using GPIO Descriptor API (gpiod)

This kernel module implements a platform driver and a misc character device to interface with a **DHT11 temperature and humidity sensor** using the GPIO descriptor API (`gpiod`) available in Linux kernel 6.12 and above.

## 🚀 Features

- Supports DHT11 sensor connected via a single GPIO line
- Uses `platform_driver` with Device Tree integration (`compatible = "training,dht11"`)
- Reads 8-bit humidity and temperature values via bit-banging
- Verifies data with checksum (parity byte)
- Exposes data through a character device (`/dev/dht11`)
- No polling from userspace: reading from device triggers measurement
- Clean separation between kernel-space bit handling and user-space access

## 📦 Device Tree Node Example

Add the following to your Device Tree source (DTS) to declare the sensor:

```dts
dht11@0 {
    compatible = "training,dht11";
    data-gpios = <&gpio 17 GPIO_ACTIVE_HIGH>;
    status = "okay";
};
```

> Replace GPIO 17 with the actual GPIO number you're using.

## 📂 Character Device Usage

Once the module is loaded and the driver is probed successfully, it creates the device:

```
/dev/dht11
```

To read the temperature (in Celsius):

```bash
cat /dev/dht11
```

> Only the temperature value is returned for now. Humidity is available internally.

## ⚙️ Module Design

- **Bit-banging Protocol:** Follows the DHT11 protocol using precise microsecond timing with `ktime` and polling.
- **Parity Check:** Ensures data integrity using checksum validation.
- **GPIO Handling:** Uses `devm_gpiod_get()` to safely acquire and release the GPIO.

### Internal Workflow

1. Send 20ms low pulse (start signal)
2. Wait for response pulses from DHT11
3. Read 40 bits of data using timing-based logic
4. Parse and validate values
5. Return temperature to user

## 🛠️ Kernel Version

Tested on **Linux kernel 6.12+** using Raspberry Pi Zero and GPIO descriptor API. Make sure your kernel supports `gpiod_*` functions and that Device Tree is enabled.

## 🧪 Output Example

```bash
$ cat /dev/dht11
24
```

