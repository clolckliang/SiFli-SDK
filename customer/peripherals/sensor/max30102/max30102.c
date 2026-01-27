/**
  ******************************************************************************
  * @file   max30102.c
  * @author Sifli software development team
  * @brief  MAX30102 driver implementation for RT-Thread.
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

#include "max30102.h"
#include <rtdevice.h>
#include <rtthread.h>
#include "board.h"

/**
 * @brief Reset pin number. Use -1 to disable pin reset.
 */
#ifndef MAX30102_RST_PIN
#define MAX30102_RST_PIN (-1)
#endif

static struct rt_i2c_bus_device *max30102_i2c_bus = RT_NULL;
static uint8_t max30102_part_id = 0;
static uint8_t max30102_i2c_addr = MAX30102_I2C_ADDR;

/**
 * @brief Write a register over I2C.
 *
 * @param reg   Register address.
 * @param value Value to write.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */

static int max30102_write_reg(uint8_t reg, uint8_t value)
{
    rt_size_t res;

    if (!max30102_i2c_bus)
    {
        return -RT_ERROR;
    }

    res = rt_i2c_mem_write(max30102_i2c_bus,
                           max30102_i2c_addr,
                           reg,
                           8,
                           &value,
                           1);
    return (res > 0) ? RT_EOK : -RT_ERROR;
}

/**
 * @brief Read a register over I2C.
 *
 * @param reg   Register address.
 * @param value Output value.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
static int max30102_read_reg(uint8_t reg, uint8_t *value)
{
    rt_size_t res;

    if (!max30102_i2c_bus || !value)
    {
        return -RT_ERROR;
    }

    res = rt_i2c_mem_read(max30102_i2c_bus,
                          max30102_i2c_addr,
                          reg,
                          8,
                          value,
                          1);
    return (res > 0) ? RT_EOK : -RT_ERROR;
}

/**
 * @brief Read multiple bytes starting from a register.
 *
 * @param reg Starting register address.
 * @param buf Output buffer.
 * @param len Number of bytes to read.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
static int max30102_read_bytes(uint8_t reg, uint8_t *buf, rt_size_t len)
{
    rt_size_t res;

    if (!max30102_i2c_bus || !buf || len == 0)
    {
        return -RT_ERROR;
    }

    res = rt_i2c_mem_read(max30102_i2c_bus,
                          max30102_i2c_addr,
                          reg,
                          8,
                          buf,
                          len);
    return (res > 0) ? RT_EOK : -RT_ERROR;
}

/**
 * @brief Toggle the hardware reset pin if configured.
 */
static void max30102_reset_pin_pulse(void)
{
#if (MAX30102_RST_PIN >= 0)
    rt_pin_mode(MAX30102_RST_PIN, PIN_MODE_OUTPUT);
    rt_pin_write(MAX30102_RST_PIN, PIN_LOW);
    rt_thread_mdelay(5);
    rt_pin_write(MAX30102_RST_PIN, PIN_HIGH);
    rt_thread_mdelay(5);
#endif
}

/**
 * @brief Issue a software reset.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Reset(void)
{
    if (max30102_write_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET) != RT_EOK)
    {
        return -RT_ERROR;
    }
    rt_thread_mdelay(10);
    return RT_EOK;
}

/**
 * @brief Enter shutdown mode.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Shutdown(void)
{
    uint8_t value = 0;

    if (max30102_read_reg(MAX30102_REG_MODE_CONFIG, &value) != RT_EOK)
    {
        return -RT_ERROR;
    }
    value |= MAX30102_MODE_SHUTDOWN;
    return max30102_write_reg(MAX30102_REG_MODE_CONFIG, value);
}

/**
 * @brief Exit shutdown mode.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Wakeup(void)
{
    uint8_t value = 0;

    if (max30102_read_reg(MAX30102_REG_MODE_CONFIG, &value) != RT_EOK)
    {
        return -RT_ERROR;
    }
    value &= (uint8_t)(~MAX30102_MODE_SHUTDOWN);
    return max30102_write_reg(MAX30102_REG_MODE_CONFIG, value);
}

/**
 * @brief Initialize MAX30102 and configure registers.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Init(void)
{
    if (!max30102_i2c_bus)
    {
        max30102_i2c_bus = rt_i2c_bus_device_find(MAX30102_I2C_BUS);
        if (!max30102_i2c_bus)
        {
            return -RT_ERROR;
        }
    }

    max30102_reset_pin_pulse();

    if (MAX30102_Reset() != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (max30102_read_reg(MAX30102_REG_PART_ID, &max30102_part_id) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (max30102_write_reg(MAX30102_REG_INT_ENABLE1, 0xC0) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_INT_ENABLE2, 0x00) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_FIFO_WR_PTR, 0x00) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_OVF_COUNTER, 0x00) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_FIFO_RD_PTR, 0x00) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (max30102_write_reg(MAX30102_REG_FIFO_CONFIG, 0x0F) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_SPO2_CONFIG, 0x27) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_LED1_PA, 0x24) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_LED2_PA, 0x24) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_write_reg(MAX30102_REG_PILOT_PA, 0x7F) != RT_EOK)
    {
        return -RT_ERROR;
    }

    return RT_EOK;
}

/**
 * @brief Read one FIFO sample.
 *
 * @param[out] sample Output sample container.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Read_FIFO(max30102_fifo_sample_t *sample)
{
    uint8_t status;
    uint8_t buf[6];
    uint32_t red;
    uint32_t ir;

    if (!sample)
    {
        return -RT_ERROR;
    }

    if (max30102_read_reg(MAX30102_REG_INT_STATUS1, &status) != RT_EOK)
    {
        return -RT_ERROR;
    }
    if (max30102_read_reg(MAX30102_REG_INT_STATUS2, &status) != RT_EOK)
    {
        return -RT_ERROR;
    }

    if (max30102_read_bytes(MAX30102_REG_FIFO_DATA, buf, sizeof(buf)) != RT_EOK)
    {
        return -RT_ERROR;
    }

    red = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    ir = ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) | buf[5];

    sample->red = red & 0x03FFFF;
    sample->ir = ir & 0x03FFFF;

    return RT_EOK;
}

/**
 * @brief Get MAX30102 part ID.
 *
 * @return Part ID.
 */
uint8_t MAX30102_Get_ID(void)
{
    return max30102_part_id;
}

/**
 * @brief Get the I2C bus handle.
 *
 * @return Pointer to I2C bus device.
 */
void *MAX30102_Get_Bus(void)
{
    return max30102_i2c_bus;
}

/**
 * @brief Get the I2C address.
 *
 * @return I2C address.
 */
uint8_t MAX30102_Get_Addr(void)
{
    return max30102_i2c_addr;
}
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
