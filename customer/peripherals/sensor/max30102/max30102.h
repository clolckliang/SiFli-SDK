/**
  ******************************************************************************
  * @file   max30102.h
  * @author Sifli software development team
  * @brief  MAX30102 driver public interface.
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

#ifndef _MAX30102_H_
#define _MAX30102_H_

#include <stdint.h>

/**
 * @brief MAX30102 I2C address.
 */
#define MAX30102_I2C_ADDR            (0x57)

/**
 * @brief MAX30102 register map.
 */
#define MAX30102_REG_INT_STATUS1     (0x00)
#define MAX30102_REG_INT_STATUS2     (0x01)
#define MAX30102_REG_INT_ENABLE1     (0x02)
#define MAX30102_REG_INT_ENABLE2     (0x03)
#define MAX30102_REG_FIFO_WR_PTR     (0x04)
#define MAX30102_REG_OVF_COUNTER     (0x05)
#define MAX30102_REG_FIFO_RD_PTR     (0x06)
#define MAX30102_REG_FIFO_DATA       (0x07)
#define MAX30102_REG_FIFO_CONFIG     (0x08)
#define MAX30102_REG_MODE_CONFIG     (0x09)
#define MAX30102_REG_SPO2_CONFIG     (0x0A)
#define MAX30102_REG_LED1_PA         (0x0C)
#define MAX30102_REG_LED2_PA         (0x0D)
#define MAX30102_REG_PILOT_PA        (0x10)
#define MAX30102_REG_PART_ID         (0xFF)

/**
 * @brief Mode configuration bits.
 */
#define MAX30102_MODE_RESET          (0x40)
#define MAX30102_MODE_SHUTDOWN       (0x80)
#define MAX30102_MODE_SPO2           (0x03)

/**
 * @brief FIFO sample output from MAX30102.
 */
typedef struct
{
    uint32_t red; /**< Red LED sample (18-bit). */
    uint32_t ir;  /**< IR LED sample (18-bit). */
} max30102_fifo_sample_t;

/**
 * @brief Initialize MAX30102 and configure default registers.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Init(void);

/**
 * @brief Reset the MAX30102 device.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Reset(void);

/**
 * @brief Put MAX30102 into shutdown mode.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Shutdown(void);

/**
 * @brief Wake up MAX30102 from shutdown mode.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Wakeup(void);

/**
 * @brief Read one FIFO sample from MAX30102.
 *
 * @param[out] sample Output sample container.
 *
 * @return RT_EOK on success, -RT_ERROR on failure.
 */
int MAX30102_Read_FIFO(max30102_fifo_sample_t *sample);

/**
 * @brief Get MAX30102 part ID.
 *
 * @return Part ID value.
 */
uint8_t MAX30102_Get_ID(void);

/**
 * @brief Get the bound I2C bus handle.
 *
 * @return Pointer to I2C bus device.
 */
void *MAX30102_Get_Bus(void);

/**
 * @brief Get the configured I2C address.
 *
 * @return I2C address.
 */
uint8_t MAX30102_Get_Addr(void);

#endif  // _MAX30102_H_
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
