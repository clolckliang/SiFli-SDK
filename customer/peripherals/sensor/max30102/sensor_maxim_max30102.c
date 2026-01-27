/**
  ******************************************************************************
  * @file   sensor_maxim_max30102.c
  * @author Sifli software development team
  * @brief  RT-Thread sensor layer for MAX30102.
  ******************************************************************************
*/
/**
 * @attention
 * Copyright (c) 2019 - 2022,  Sifli Technology
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Sifli integrated circuit
 *    in a product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of Sifli nor the names of its contributors may be used to endorse
 *    or promote products derived from this software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Sifli integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY SIFLI TECHNOLOGY "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SIFLI TECHNOLOGY OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "sensor_maxim_max30102.h"
#include "max30102.h"
#include "max30102_algo.h"

#ifdef RT_USING_SENSOR

#define DBG_TAG "sensor.max30102"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

/**
 * @brief Default I2C bus name used by the sensor layer.
 */
#ifndef MAX30102_I2C_BUS
#define MAX30102_I2C_BUS "i2c4"
#endif

/**
 * @brief Interrupt pin number. Use -1 to disable interrupt pin.
 */
#ifndef MAX30102_INT_PIN
#define MAX30102_INT_PIN (-1)
#endif

static struct max30102_device *max30102_dev = RT_NULL;
static rt_sensor_t sensor_hr = RT_NULL;
static uint8_t max30102_inited = 0;
static rt_size_t max30102_buf_head = 0;
static rt_size_t max30102_buf_count = 0;
static rt_size_t max30102_algo_tick = 0;
static int32_t max30102_last_hr = 0;
static int32_t max30102_last_spo2 = 0;
static int8_t max30102_hr_valid = 0;
static int8_t max30102_spo2_valid = 0;
static uint32_t max30102_ir_buf[MAX30102_BUFFER_SIZE];
static uint32_t max30102_red_buf[MAX30102_BUFFER_SIZE];
static uint32_t max30102_ir_work[MAX30102_BUFFER_SIZE];
static uint32_t max30102_red_work[MAX30102_BUFFER_SIZE];

/**
 * @brief Number of samples between algorithm runs.
 */
#define MAX30102_ALGO_STEP 25

/**
 * @brief Initialize MAX30102 sensor resources once.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
static rt_err_t _max30102_init(void)
{
    if (max30102_inited)
    {
        return RT_EOK;
    }

    if (MAX30102_Init() != RT_EOK)
    {
        return -RT_ERROR;
    }

    max30102_dev = rt_calloc(1, sizeof(struct max30102_device));
    if (max30102_dev == RT_NULL)
    {
        return RT_ENOMEM;
    }

    max30102_dev->bus = (rt_device_t)MAX30102_Get_Bus();
    max30102_dev->i2c_addr = MAX30102_Get_Addr();
    max30102_dev->id = MAX30102_Get_ID();

    max30102_buf_head = 0;
    max30102_buf_count = 0;
    max30102_algo_tick = 0;
    max30102_last_hr = 0;
    max30102_last_spo2 = 0;
    max30102_hr_valid = 0;
    max30102_spo2_valid = 0;

    max30102_inited = 1;
    return RT_EOK;
}

/**
 * @brief Set sensor range (not supported for MAX30102).
 *
 * @param sensor Sensor device.
 * @param range  Requested range.
 *
 * @return RT_EOK always.
 */
static rt_err_t _max30102_set_range(rt_sensor_t sensor, rt_int32_t range)
{
    RT_UNUSED(sensor);
    RT_UNUSED(range);
    return RT_EOK;
}

/**
 * @brief Perform a simple self-test by reading the part ID.
 *
 * @param sensor Sensor device.
 * @param mode   Self-test mode (unused).
 *
 * @return RT_EOK on success, -RT_EIO or -RT_ERROR on failure.
 */
static rt_err_t _max30102_self_test(rt_sensor_t sensor, rt_uint8_t mode)
{
    RT_UNUSED(sensor);
    RT_UNUSED(mode);

    if (_max30102_init() != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (MAX30102_Get_ID() == 0 || MAX30102_Get_ID() == 0xFF)
    {
        return -RT_EIO;
    }

    return RT_EOK;
}

/**
 * @brief Configure sensor mode (only polling supported).
 *
 * @param sensor Sensor device.
 * @param mode   Sensor mode.
 *
 * @return RT_EOK on success, -RT_ERROR on unsupported mode.
 */
static rt_err_t _max30102_hr_set_mode(rt_sensor_t sensor, rt_uint8_t mode)
{
    RT_UNUSED(sensor);

    if (mode == RT_SENSOR_MODE_POLLING)
    {
        LOG_D("set mode to POLLING");
    }
    else
    {
        LOG_D("Unsupported mode, code is %d", mode);
        return -RT_ERROR;
    }
    return RT_EOK;
}

/**
 * @brief Set sensor power state.
 *
 * @param sensor Sensor device.
 * @param power  Power mode.
 *
 * @return RT_EOK always.
 */
static rt_err_t _max30102_set_power(rt_sensor_t sensor, rt_uint8_t power)
{
    RT_UNUSED(sensor);

    switch (power)
    {
    case RT_SENSOR_POWER_DOWN:
        MAX30102_Shutdown();
        break;
    case RT_SENSOR_POWER_NORMAL:
        MAX30102_Wakeup();
        break;
    case RT_SENSOR_POWER_LOW:
    case RT_SENSOR_POWER_HIGH:
    default:
        break;
    }
    return RT_EOK;
}

/**
 * @brief Polling read for heart rate data.
 *
 * @param sensor Sensor device.
 * @param data   Output data container.
 *
 * @return Number of samples stored in data.
 */
static rt_size_t _max30102_polling_get_data(rt_sensor_t sensor, struct rt_sensor_data *data)
{
    max30102_fifo_sample_t sample;
    rt_size_t i;
    rt_size_t idx;

    if (sensor->info.type != RT_SENSOR_CLASS_HR)
    {
        return 0;
    }

    if (_max30102_init() != RT_EOK)
    {
        return 0;
    }

    if (MAX30102_Read_FIFO(&sample) != RT_EOK)
    {
        return 0;
    }

    max30102_ir_buf[max30102_buf_head] = sample.ir;
    max30102_red_buf[max30102_buf_head] = sample.red;
    max30102_buf_head++;
    if (max30102_buf_head >= MAX30102_BUFFER_SIZE)
    {
        max30102_buf_head = 0;
    }
    if (max30102_buf_count < MAX30102_BUFFER_SIZE)
    {
        max30102_buf_count++;
    }

    if (max30102_buf_count >= MAX30102_BUFFER_SIZE)
    {
        max30102_algo_tick++;
        if (max30102_algo_tick >= MAX30102_ALGO_STEP)
        {
            max30102_algo_tick = 0;
            for (i = 0; i < MAX30102_BUFFER_SIZE; i++)
            {
                idx = max30102_buf_head + i;
                if (idx >= MAX30102_BUFFER_SIZE)
                {
                    idx -= MAX30102_BUFFER_SIZE;
                }
                max30102_ir_work[i] = max30102_ir_buf[idx];
                max30102_red_work[i] = max30102_red_buf[idx];
            }

            max30102_calc_hr_spo2(max30102_ir_work,
                                  MAX30102_BUFFER_SIZE,
                                  max30102_red_work,
                                  &max30102_last_spo2,
                                  &max30102_spo2_valid,
                                  &max30102_last_hr,
                                  &max30102_hr_valid);
        }
    }

    data->type = RT_SENSOR_CLASS_HR;
    if (max30102_hr_valid)
    {
        data->data.hr = max30102_last_hr;
    }
    else
    {
        data->data.hr = 0;
    }
    data->timestamp = rt_sensor_get_ts();
    return 1;
}

/**
 * @brief Sensor ops: fetch data.
 *
 * @param sensor Sensor device.
 * @param buf    Output data buffer.
 * @param len    Buffer length.
 *
 * @return Number of samples read.
 */
static rt_size_t max30102_fetch_data(struct rt_sensor_device *sensor, void *buf, rt_size_t len)
{
    RT_ASSERT(buf);

    if (sensor->config.mode == RT_SENSOR_MODE_POLLING)
    {
        return _max30102_polling_get_data(sensor, buf);
    }

    return 0;
}

/**
 * @brief Sensor ops: control handler.
 *
 * @param sensor Sensor device.
 * @param cmd    Control command.
 * @param args   Command arguments.
 *
 * @return RT_EOK on success, negative error on failure.
 */
static rt_err_t max30102_control(struct rt_sensor_device *sensor, int cmd, void *args)
{
    rt_err_t result = RT_EOK;

    RT_UNUSED(sensor);

    switch (cmd)
    {
    case RT_SENSOR_CTRL_GET_ID:
        if (args)
        {
            if (_max30102_init() != RT_EOK)
            {
                *(uint8_t *)args = 0;
            }
            else
            {
                *(uint8_t *)args = max30102_dev ? max30102_dev->id : 0;
            }
        }
        break;
    case RT_SENSOR_CTRL_SET_RANGE:
        result = _max30102_set_range(sensor, (rt_int32_t)args);
        break;
    case RT_SENSOR_CTRL_SET_ODR:
        result = -RT_EINVAL;
        break;
    case RT_SENSOR_CTRL_SET_MODE:
        result = _max30102_hr_set_mode(sensor, (rt_uint32_t)args & 0xff);
        break;
    case RT_SENSOR_CTRL_SET_POWER:
        result = _max30102_set_power(sensor, (rt_uint32_t)args & 0xff);
        break;
    case RT_SENSOR_CTRL_SELF_TEST:
        result = _max30102_self_test(sensor, *((rt_uint8_t *)args));
        break;
    default:
        result = -RT_ERROR;
        break;
    }

    return result;
}

/**
 * @brief Sensor operation table.
 */
static struct rt_sensor_ops sensor_ops =
{
    max30102_fetch_data,
    max30102_control
};

/**
 * @brief Register MAX30102 sensor with RT-Thread.
 *
 * @param name Sensor device name.
 * @param cfg  Sensor configuration.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int rt_hw_max30102_register(const char *name, struct rt_sensor_config *cfg)
{
    int result;

    sensor_hr = rt_calloc(1, sizeof(struct rt_sensor_device));
    if (sensor_hr == RT_NULL)
    {
        return -RT_ERROR;
    }

    sensor_hr->info.type       = RT_SENSOR_CLASS_HR;
    sensor_hr->info.vendor     = RT_SENSOR_VENDOR_UNKNOWN;
    sensor_hr->info.model      = "max30102_hr";
    sensor_hr->info.unit       = RT_SENSOR_UNIT_BPM;
    sensor_hr->info.intf_type  = RT_SENSOR_INTF_I2C;
    sensor_hr->info.range_max  = 220;
    sensor_hr->info.range_min  = 30;
    sensor_hr->info.period_min = 1;
    sensor_hr->data_len = 0;
    sensor_hr->data_buf = RT_NULL;

    rt_memcpy(&sensor_hr->config, cfg, sizeof(struct rt_sensor_config));
    sensor_hr->ops = &sensor_ops;

    result = rt_hw_sensor_register(sensor_hr, name, RT_DEVICE_FLAG_RDWR, RT_NULL);
    if (result != RT_EOK)
    {
        LOG_E("device register err code: %d", result);
        rt_free(sensor_hr);
        sensor_hr = RT_NULL;
        return -RT_ERROR;
    }

    LOG_I("sensor init success");
    return RT_EOK;
}

/**
 * @brief Register MAX30102 sensor at component initialization.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int max30102_sensor_register(void)
{
    struct rt_sensor_config cfg = {0};

    cfg.intf.dev_name = MAX30102_I2C_BUS;
    cfg.irq_pin.pin = MAX30102_INT_PIN;

    return rt_hw_max30102_register(HR_MODEL_NAME, &cfg);
}

INIT_COMPONENT_EXPORT(max30102_sensor_register);

/**
 * @brief Initialize MAX30102 hardware and cached state.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int rt_hw_max30102_init(void)
{
    rt_int8_t result = RT_EOK;

    if (!max30102_inited)
    {
        result = _max30102_init();
        if (result != RT_EOK)
        {
            LOG_E("max30102 init err code: %d", result);
            if (max30102_dev)
            {
                rt_free(max30102_dev);
                max30102_dev = RT_NULL;
            }
        }
    }

    return result;
}

/**
 * @brief Deinitialize MAX30102 and free resources.
 *
 * @return RT_EOK always.
 */
int rt_hw_max30102_deinit(void)
{
    if (max30102_dev)
    {
        rt_free(max30102_dev);
        max30102_dev = RT_NULL;
    }
    max30102_inited = 0;
    return RT_EOK;
}

#endif // RT_USING_SENSOR
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
