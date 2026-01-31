/**
  ******************************************************************************
  * @file   sensor_maxim_max30102.h
  * @author Sifli software development team
  * @brief  RT-Thread sensor interface for MAX30102.
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

#ifndef SENSOR_MAXIM_MAX30102_H__
#define SENSOR_MAXIM_MAX30102_H__

#include "board.h"
#include "sensor.h"

/**
 * @brief MAX30102 device context.
 */
struct max30102_device
{
    rt_device_t bus;  /**< I2C bus device. */
    rt_uint8_t id;    /**< Part ID. */
    rt_uint8_t i2c_addr; /**< I2C address. */
};

/**
 * @brief Register MAX30102 sensor device.
 *
 * @param name Sensor device name.
 * @param cfg  Sensor configuration.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int rt_hw_max30102_register(const char *name, struct rt_sensor_config *cfg);

/**
 * @brief Initialize MAX30102 hardware resources.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int rt_hw_max30102_init(const char *name, struct rt_sensor_config *cfg);

/**
 * @brief Deinitialize MAX30102 hardware resources.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int rt_hw_max30102_deinit(void);

#endif  // SENSOR_MAXIM_MAX30102_H__
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
