/**
  ******************************************************************************
  * @file   max30102_algo.h
  * @author Sifli software development team
  * @brief  MAX30102 HR/SpO2 algorithm interface (platform independent).
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

#ifndef MAX30102_ALGO_H
#define MAX30102_ALGO_H

#include <stdint.h>

/**
 * @brief Sampling rate in Hz used by the algorithm.
 */
#define MAX30102_FS            100

/**
 * @brief Number of samples required per processing window.
 */
#define MAX30102_BUFFER_SIZE   (MAX30102_FS * 5)

/**
 * @brief Calculate heart rate and SpO2 from IR/Red buffers.
 *
 * @param[in]  ir_buf       Pointer to IR samples.
 * @param[in]  ir_len       Number of samples in ir_buf (must be >= MAX30102_BUFFER_SIZE).
 * @param[in]  red_buf      Pointer to Red samples.
 * @param[out] spo2         SpO2 value (percent).
 * @param[out] spo2_valid   1 if SpO2 result is valid, otherwise 0.
 * @param[out] hr           Heart rate value (BPM).
 * @param[out] hr_valid     1 if heart rate is valid, otherwise 0.
 *
 * @return 0 on success, -1 on parameter error.
 */
int max30102_calc_hr_spo2(const uint32_t *ir_buf,
                          int32_t ir_len,
                          const uint32_t *red_buf,
                          int32_t *spo2,
                          int8_t *spo2_valid,
                          int32_t *hr,
                          int8_t *hr_valid);

#endif  // MAX30102_ALGO_H
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
