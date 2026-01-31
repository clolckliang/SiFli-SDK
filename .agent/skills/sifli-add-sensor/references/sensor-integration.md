# Sensor Integration Reference

## Preflight checklist

- Confirm sensor class, bus type, and I2C/SPI address.
- Confirm the chosen bus is enabled and pins are routed on hardware.
- Verify voltage and frequency ranges match the board.
- Align with users on required features.

## Kconfig skeleton (6-axis example)

```
menuconfig SENSOR_USING_6D
    bool "6D Sensor for Accelerator and Gyro"
    default n
    if SENSOR_USING_6D
        menuconfig ACC_USING_LSM6DSL
            bool "LSM6DSL"
            select RT_USING_SENSOR
            default n
            if ACC_USING_LSM6DSL
                config LSM6DSL_USING_I2C
                    int "LSM6DSL bus type: 1 = I2C, 0 = SPI"
                    default 0
                config LSM6DSL_BUS_NAME
                    string "LSM6DSL bus name"
                    default "spi1"
                config LSM6DSL_INT_GPIO_BIT
                    int "LSM6DSL interrupt 1 pin"
                    default 97
                config LSM6DSL_INT2_GPIO_BIT
                    int "LSM6DSL interrupt 2 pin"
                    default 94
                config LSM_USING_AWT
                    bool "Enable AWT function"
                    default y
                config LSM_USING_PEDO
                    bool "Enable Pedometer function"
                    default y
                config LSM6DSL_USE_FIFO
                    bool "Enable FIFO"
                    default y
            endif
    endif
```

Notes:
- `SENSOR_USING_6D` is the top-level switch for the class.
- `ACC_USING_LSM6DSL` enables a model in that class.
- `LSM6DSL_USING_I2C` is only needed if the model supports multiple buses.
- `LSM6DSL_BUS_NAME` is used by RT-Thread to find the bus.
- `LSM6DSL_INT_GPIO_BIT` and `LSM6DSL_INT2_GPIO_BIT` define IRQ pins.

## Bus access (I2C) example

```
int lsm6dsl_i2c_init(void)
{
    lsm6dsl_content.bus_handle = rt_i2c_bus_device_find(LSM6DSL_BUS_NAME);
    if (lsm6dsl_content.bus_handle)
    {
        LOG_D("Find i2c bus device %s\n", LSM6DSL_BUS_NAME);
    }
    else
    {
        LOG_E("Can not found i2c bus %s, init fail\n", LSM6DSL_BUS_NAME);
        return -1;
    }

    return 0;
}

int32_t lsm_i2c_write(void *ctx, uint8_t reg, uint8_t *data, uint16_t len)
{
    rt_size_t res;
    struct LSM6DSL_CONT_T *handle = (struct LSM6DSL_CONT_T *)ctx;

    if (handle && handle->bus_handle && data)
    {
        uint16_t addr16 = (uint16_t)reg;
        res = rt_i2c_mem_write(handle->bus_handle, handle->dev_addr, addr16, 8, data, len);
        return (res > 0) ? 0 : -2;
    }

    return -3;
}

int32_t lsm_i2c_read(void *ctx, uint8_t reg, uint8_t *data, uint16_t len)
{
    rt_size_t res;
    struct LSM6DSL_CONT_T *handle = (struct LSM6DSL_CONT_T *)ctx;

    if (handle && handle->bus_handle && data)
    {
        uint16_t addr16 = (uint16_t)reg;
        res = rt_i2c_mem_read(handle->bus_handle, handle->dev_addr, addr16, 8, data, len);
        return (res > 0) ? 0 : -2;
    }

    return -3;
}
```

## PPG/HR sensor configuration notes

- Align register defaults with the vendor reference init (interrupts, FIFO config, LED currents).
- Clear interrupt status before reading FIFO to avoid stale flags.
- Keep a fixed sampling rate and buffer length for the HR/SpO2 algorithm.

## Reference code intake checklist

- Identify platform-specific layers (bit-banged I2C, GPIO macros, delay APIs) and map to RT-Thread APIs.
- Extract the init sequence and register defaults; mirror them in the new driver unless there is a hardware reason not to.
- Confirm FIFO read order and interrupt clear behavior; keep the same sequence as the reference.
- Note sampling rate and buffer length assumptions in any algorithm code and enforce them in the driver.
- Verify licensing headers and preserve required notices when reusing algorithms or tables.

## Reference-to-implementation diff template

Use this template to document deltas between the reference code and the RT-Thread driver.

```
Reference: <path or link>
Target: rtos/rthread/bsp/sifli/peripherals/<sensor>

1) Init sequence
- Reference: <register writes, order>
- Implementation: <register writes, order>
- Delta: <what changed and why>

2) IRQ and FIFO
- Reference: <IRQ enable, status clear, FIFO config>
- Implementation: <IRQ enable, status clear, FIFO config>
- Delta: <what changed and why>

3) Bus and timing
- Reference: <I2C/SPI/UART, speed, delays>
- Implementation: <I2C/SPI/UART, speed, delays>
- Delta: <what changed and why>

4) Data path
- Reference: <raw sample format, scaling>
- Implementation: <raw sample format, scaling>
- Delta: <what changed and why>

5) Algorithm (if any)
- Reference: <sampling rate, buffer size, HR/SpO2>
- Implementation: <sampling rate, buffer size, HR/SpO2>
- Delta: <what changed and why>

6) Build/Kconfig
- Reference: <config macros>
- Implementation: <config macros>
- Delta: <what changed and why>
```

## Interrupt setup example

```
int lsm6dsl_gpio_int_enable(void)
{
    struct rt_device_pin_mode m;
    rt_device_t device = rt_device_find("pin");
    if (!device)
    {
        LOG_E("GPIO pin device not found at LSM6DSL\n");
        return -1;
    }

    rt_device_open(device, RT_DEVICE_OFLAG_RDWR);

    m.pin = LSM6DSL_INT_GPIO_BIT;
    m.mode = PIN_MODE_INPUT;
    rt_device_control(device, 0, &m);

    rt_pin_mode(LSM6DSL_INT_GPIO_BIT, PIN_MODE_INPUT);
    rt_pin_attach_irq(m.pin, PIN_IRQ_MODE_RISING, lsm6dsl_int1_handle, (void *)(rt_uint32_t)m.pin);
    rt_pin_irq_enable(m.pin, 1);

    return 0;
}
```

## RT-Thread sensor registration template (BMP280 example)

```
int rt_hw_bmp280_init(const char *name, struct rt_sensor_config *cfg)
{
    rt_int8_t result;
    rt_sensor_t sensor_temp = RT_NULL, sensor_baro = RT_NULL;

    result = _bmp280_init();
    if (result != RT_EOK)
    {
        LOG_E("bmp280 init err code: %d", result);
        goto __exit;
    }

    sensor_temp = rt_calloc(1, sizeof(struct rt_sensor_device));
    if (sensor_temp == RT_NULL)
        return -1;

    sensor_temp->info.type       = RT_SENSOR_CLASS_TEMP;
    sensor_temp->info.vendor     = RT_SENSOR_VENDOR_BOSCH;
    sensor_temp->info.model      = "bmp280_temp";
    sensor_temp->info.unit       = RT_SENSOR_UNIT_DCELSIUS;
    sensor_temp->info.intf_type  = RT_SENSOR_INTF_I2C;
    sensor_temp->info.range_max  = 85;
    sensor_temp->info.range_min  = -40;
    sensor_temp->info.period_min = 5;

    rt_memcpy(&sensor_temp->config, cfg, sizeof(struct rt_sensor_config));
    sensor_temp->ops = &sensor_ops;

    result = rt_hw_sensor_register(sensor_temp, name, RT_DEVICE_FLAG_RDWR, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("device register err code: %d", result);
        goto __exit;
    }

    sensor_baro = rt_calloc(1, sizeof(struct rt_sensor_device));
    if (sensor_baro == RT_NULL)
        goto __exit;

    sensor_baro->info.type       = RT_SENSOR_CLASS_BARO;
    sensor_baro->info.vendor     = RT_SENSOR_VENDOR_BOSCH;
    sensor_baro->info.model      = "bmp280_baro";
    sensor_baro->info.unit       = RT_SENSOR_UNIT_PA;
    sensor_baro->info.intf_type  = RT_SENSOR_INTF_I2C;
    sensor_baro->info.range_max  = 110000;
    sensor_baro->info.range_min  = 30000;
    sensor_baro->info.period_min = 5;

    rt_memcpy(&sensor_baro->config, cfg, sizeof(struct rt_sensor_config));
    sensor_baro->ops = &sensor_ops;

    result = rt_hw_sensor_register(sensor_baro, name, RT_DEVICE_FLAG_RDWR, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("device register err code: %d", result);
        goto __exit;
    }

    LOG_I("sensor init success");
    return RT_EOK;

__exit:
    if (sensor_temp)
        rt_free(sensor_temp);
    if (sensor_baro)
        rt_free(sensor_baro);
    if (bmp_dev)
        rt_free(bmp_dev);
    return -RT_ERROR;
}
```

## Component registration hook

Use a component init hook so the sensor registers at startup.

```
int xxx_sensor_register(void)
{
    struct rt_sensor_config cfg;

    cfg.intf.dev_name = XXX_I2C_BUS;
    cfg.irq_pin.pin = XXX_INT_PIN;

    return rt_hw_xxx_register(XXX_MODEL_NAME, &cfg);
}

INIT_COMPONENT_EXPORT(xxx_sensor_register);
```

## Finsh debug command skeleton

```
#define DRV_BMP280_TEST

#ifdef DRV_BMP280_TEST
#include <string.h>

int cmd_bmpt(int argc, char *argv[])
{
    int32_t temp, pres, alti;
    if (argc < 2)
    {
        LOG_I("Invalid parameter!\n");
        return 1;
    }
    if (strcmp(argv[1], "-open") == 0)
    {
        uint8_t res = BMP280_Init();
        if (BMP280_RET_OK == res)
        {
            BMP280_open();
            LOG_I("Open bmp280 success\n");
        }
        else
            LOG_I("open bmp280 fail\n");
    }
    if (strcmp(argv[1], "-close") == 0)
    {
        BMP280_close();
        LOG_I("BMP280 closed\n");
    }
    if (strcmp(argv[1], "-r") == 0)
    {
        uint8_t rega = atoi(argv[2]) & 0xff;
        uint8_t value;
        BMP280_ReadReg(rega, 1, &value);
        LOG_I("Reg 0x%x value 0x%x\n", rega, value);
    }
    if (strcmp(argv[1], "-tpa") == 0)
    {
        temp = 0;
        pres = 0;
        alti = 0;
        BMP280_CalTemperatureAndPressureAndAltitude(&temp, &pres, &alti);
        LOG_I("Get temperature = %.1f\n", (float)temp / 10);
        LOG_I("Get pressure= %.2f\n", (float)pres / 100);
        LOG_I("Get altitude= %.2f\n", (float)alti / 100);
    }
    if (strcmp(argv[1], "-bps") == 0)
    {
        struct rt_i2c_configuration cfg;
        int bps = atoi(argv[2]);
        cfg.addr = 0;
        cfg.max_hz = bps;
        cfg.mode = 0;
        cfg.timeout = 5000;
        rt_i2c_configure(i2cbus, &cfg);
        LOG_I("Config BMP I2C speed to %d\n", bps);
    }

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_bmpt, __cmd_bmpt, Test driver bmp280);
#endif
```
