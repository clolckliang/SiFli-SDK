/**
  ******************************************************************************
  * @file   max30102_algo.c
  * @author Sifli software development team
  * @brief  MAX30102 HR/SpO2 algorithm implementation.
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

#include "max30102_algo.h"

/* Algorithm constants. */
#define MAX30102_HR_FIFO_SIZE     7
#define MAX30102_MA4_SIZE         4
#define MAX30102_HAMMING_SIZE     5
#define MAX30102_HAMMING_SUM      1146
#define MAX30102_MAX_PEAKS        15
#define MAX30102_MAX_RATIOS       5
#define MAX30102_SPO2_TABLE_SIZE  184
#define MAX30102_INVALID_VALUE    (-999)
#define MAX30102_SIGNAL_MAX       16777216
#define MAX30102_SIGNAL_MIN       (-16777216)

/* Hamming window coefficients used by derivative smoothing. */
static const uint16_t max30102_hamm[MAX30102_HAMMING_SIZE] = { 41, 276, 512, 276, 41 };

/* SpO2 lookup table indexed by the ratio value. */
static const uint8_t max30102_spo2_table[MAX30102_SPO2_TABLE_SIZE] = {
    95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99,
    99, 99, 99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 99, 99, 99, 99, 99, 99,
    99, 99, 98, 98, 98, 98, 98, 98, 97, 97, 97, 97, 96, 96, 96, 96, 95, 95,
    95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91, 90, 90, 89, 89, 89, 88,
    88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81, 80, 80, 79, 78,
    78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67, 66, 66,
    65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
    49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31,
    30, 29, 28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10,
    9, 7, 6, 5, 3, 2, 1
};

/* Internal working buffers (not re-entrant). */
static int32_t max30102_dx[MAX30102_BUFFER_SIZE - MAX30102_MA4_SIZE];
static int32_t max30102_ir[MAX30102_BUFFER_SIZE];
static int32_t max30102_red[MAX30102_BUFFER_SIZE];

/**
 * @brief Return the smaller of two values.
 *
 * @param a First value.
 * @param b Second value.
 *
 * @return Minimum of a and b.
 */
static int32_t max30102_min(int32_t a, int32_t b)
{
    return (a < b) ? a : b;
}

/**
 * @brief Sort an array in ascending order (in-place).
 *
 * @param arr  Array to sort.
 * @param size Number of elements in arr.
 */
static void max30102_sort_ascend(int32_t *arr, int32_t size)
{
    int32_t i;

    for (i = 1; i < size; i++)
    {
        int32_t temp = arr[i];
        int32_t j;

        for (j = i; j > 0 && temp < arr[j - 1]; j--)
        {
            arr[j] = arr[j - 1];
        }
        arr[j] = temp;
    }
}

/**
 * @brief Sort index array by values in arr (descending).
 *
 * @param arr  Source values.
 * @param idx  Indices to reorder.
 * @param size Number of indices.
 */
static void max30102_sort_indices_descend(int32_t *arr, int32_t *idx, int32_t size)
{
    int32_t i;

    for (i = 1; i < size; i++)
    {
        int32_t temp = idx[i];
        int32_t j;

        for (j = i; j > 0 && arr[temp] > arr[idx[j - 1]]; j--)
        {
            idx[j] = idx[j - 1];
        }
        idx[j] = temp;
    }
}

/**
 * @brief Find peak locations above a minimum height.
 *
 * @param[out] locs       Peak indices.
 * @param[out] npks       Number of peaks found.
 * @param[in]  x          Input signal.
 * @param[in]  size       Number of samples in x.
 * @param[in]  min_height Minimum peak height.
 */
static void max30102_peaks_above_min_height(int32_t *locs,
                                            int32_t *npks,
                                            int32_t *x,
                                            int32_t size,
                                            int32_t min_height)
{
    int32_t i = 1;
    *npks = 0;

    while (i < size - 1)
    {
        if (x[i] > min_height && x[i] > x[i - 1])
        {
            int32_t width = 1;

            while (i + width < size && x[i] == x[i + width])
            {
                width++;
            }

            if (x[i] > x[i + width] && (*npks) < MAX30102_MAX_PEAKS)
            {
                locs[(*npks)++] = i;
                i += width + 1;
            }
            else
            {
                i += width;
            }
        }
        else
        {
            i++;
        }
    }
}

/**
 * @brief Remove peaks that are closer than min_distance.
 *
 * @param[in,out] locs         Peak locations (sorted).
 * @param[in,out] npks         Number of peaks (updated after pruning).
 * @param[in]     x            Input signal.
 * @param[in]     min_distance Minimum distance between peaks.
 */
static void max30102_remove_close_peaks(int32_t *locs, int32_t *npks, int32_t *x, int32_t min_distance)
{
    int32_t i;

    max30102_sort_indices_descend(x, locs, *npks);

    for (i = -1; i < *npks; i++)
    {
        int32_t old_npks = *npks;
        int32_t j;

        *npks = i + 1;
        for (j = i + 1; j < old_npks; j++)
        {
            int32_t dist = locs[j] - (i == -1 ? -1 : locs[i]);

            if (dist > min_distance || dist < -min_distance)
            {
                locs[(*npks)++] = locs[j];
            }
        }
    }

    max30102_sort_ascend(locs, *npks);
}

/**
 * @brief Find peaks in a signal using threshold and distance filtering.
 *
 * @param[out] locs        Peak indices.
 * @param[out] npks        Number of peaks found.
 * @param[in]  x           Input signal.
 * @param[in]  size        Number of samples in x.
 * @param[in]  min_height  Minimum peak height.
 * @param[in]  min_distance Minimum peak separation.
 * @param[in]  max_num     Maximum number of peaks to return.
 */
static void max30102_find_peaks(int32_t *locs,
                                int32_t *npks,
                                int32_t *x,
                                int32_t size,
                                int32_t min_height,
                                int32_t min_distance,
                                int32_t max_num)
{
    max30102_peaks_above_min_height(locs, npks, x, size, min_height);
    max30102_remove_close_peaks(locs, npks, x, min_distance);
    *npks = max30102_min(*npks, max_num);
}

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
                          int8_t *hr_valid)
{
    int32_t buffer_len = MAX30102_BUFFER_SIZE;
    int32_t i;
    int32_t k;
    int32_t n_th1;
    int32_t n_npks;
    int32_t n_peak_interval_sum;
    int32_t an_dx_peak_locs[MAX30102_MAX_PEAKS];
    int32_t an_ir_valley_locs[MAX30102_MAX_PEAKS];
    int32_t an_exact_ir_valley_locs[MAX30102_MAX_PEAKS];
    int32_t an_ratio[MAX30102_MAX_RATIOS];
    int32_t n_i_ratio_count;
    int32_t n_exact_ir_valley_locs_count;
    int32_t n_ratio_average;
    int32_t n_middle_idx;
    int32_t n_spo2_calc;
    int32_t n_y_dc_max;
    int32_t n_x_dc_max;
    int32_t n_y_dc_max_idx;
    int32_t n_x_dc_max_idx;
    int32_t n_y_ac;
    int32_t n_x_ac;
    int32_t n_nume;
    int32_t n_denom;
    int32_t n_c_min;
    int32_t m;
    int32_t s;
    uint32_t ir_mean;

    if (!ir_buf || !red_buf || !spo2 || !spo2_valid || !hr || !hr_valid)
    {
        return -1;
    }

    if (ir_len < MAX30102_BUFFER_SIZE)
    {
        *spo2 = MAX30102_INVALID_VALUE;
        *spo2_valid = 0;
        *hr = MAX30102_INVALID_VALUE;
        *hr_valid = 0;
        return -1;
    }

    /* Remove DC component from IR signal. */
    ir_mean = 0;
    for (k = 0; k < buffer_len; k++)
    {
        ir_mean += ir_buf[k];
    }
    ir_mean = ir_mean / (uint32_t)buffer_len;
    for (k = 0; k < buffer_len; k++)
    {
        max30102_ir[k] = (int32_t)ir_buf[k] - (int32_t)ir_mean;
    }

    /* Apply moving average filter. */
    for (k = 0; k < buffer_len - MAX30102_MA4_SIZE; k++)
    {
        int32_t sum = max30102_ir[k] + max30102_ir[k + 1] + max30102_ir[k + 2] + max30102_ir[k + 3];

        max30102_ir[k] = sum / MAX30102_MA4_SIZE;
    }

    /* First and second derivative smoothing. */
    for (k = 0; k < buffer_len - MAX30102_MA4_SIZE - 1; k++)
    {
        max30102_dx[k] = max30102_ir[k + 1] - max30102_ir[k];
    }
    for (k = 0; k < buffer_len - MAX30102_MA4_SIZE - 2; k++)
    {
        max30102_dx[k] = (max30102_dx[k] + max30102_dx[k + 1]) / 2;
    }

    for (i = 0; i < buffer_len - MAX30102_HAMMING_SIZE - MAX30102_MA4_SIZE - 2; i++)
    {
        s = 0;
        for (k = i; k < i + MAX30102_HAMMING_SIZE; k++)
        {
            s -= max30102_dx[k] * (int32_t)max30102_hamm[k - i];
        }
        max30102_dx[i] = s / (int32_t)MAX30102_HAMMING_SUM;
    }

    /* Threshold estimation. */
    n_th1 = 0;
    for (k = 0; k < buffer_len - MAX30102_HAMMING_SIZE; k++)
    {
        n_th1 += (max30102_dx[k] > 0) ? max30102_dx[k] : -max30102_dx[k];
    }
    n_th1 = n_th1 / (buffer_len - MAX30102_HAMMING_SIZE);

    max30102_find_peaks(an_dx_peak_locs,
                        &n_npks,
                        max30102_dx,
                        buffer_len - MAX30102_HAMMING_SIZE,
                        n_th1,
                        8,
                        5);

    /* Heart rate calculation. */
    n_peak_interval_sum = 0;
    if (n_npks >= 2)
    {
        for (k = 1; k < n_npks; k++)
        {
            n_peak_interval_sum += (an_dx_peak_locs[k] - an_dx_peak_locs[k - 1]);
        }
        n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
        *hr = (int32_t)(6000 / n_peak_interval_sum);
        *hr_valid = 1;
    }
    else
    {
        *hr = MAX30102_INVALID_VALUE;
        *hr_valid = 0;
    }

    /* Valley location search. */
    for (k = 0; k < n_npks; k++)
    {
        an_ir_valley_locs[k] = an_dx_peak_locs[k] + MAX30102_HAMMING_SIZE / 2;
    }

    for (k = 0; k < buffer_len; k++)
    {
        max30102_ir[k] = (int32_t)ir_buf[k];
        max30102_red[k] = (int32_t)red_buf[k];
    }

    n_exact_ir_valley_locs_count = 0;
    for (k = 0; k < n_npks; k++)
    {
        m = an_ir_valley_locs[k];
        n_c_min = MAX30102_SIGNAL_MAX;

        if (m + 5 < buffer_len - MAX30102_HAMMING_SIZE && m - 5 > 0)
        {
            int32_t only_once = 1;

            for (i = m - 5; i < m + 5; i++)
            {
                if (max30102_ir[i] < n_c_min)
                {
                    if (only_once > 0)
                    {
                        only_once = 0;
                    }
                    n_c_min = max30102_ir[i];
                    an_exact_ir_valley_locs[k] = i;
                }
            }
            if (only_once == 0)
            {
                n_exact_ir_valley_locs_count++;
            }
        }
    }

    if (n_exact_ir_valley_locs_count < 2)
    {
        *spo2 = MAX30102_INVALID_VALUE;
        *spo2_valid = 0;
        return 0;
    }

    /* Apply moving average filter for ratio calculation. */
    for (k = 0; k < buffer_len - MAX30102_MA4_SIZE; k++)
    {
        max30102_ir[k] = (max30102_ir[k] + max30102_ir[k + 1] + max30102_ir[k + 2] + max30102_ir[k + 3]) /
                         MAX30102_MA4_SIZE;
        max30102_red[k] = (max30102_red[k] + max30102_red[k + 1] + max30102_red[k + 2] + max30102_red[k + 3]) /
                          MAX30102_MA4_SIZE;
    }

    n_ratio_average = 0;
    n_i_ratio_count = 0;
    for (k = 0; k < MAX30102_MAX_RATIOS; k++)
    {
        an_ratio[k] = 0;
    }

    for (k = 0; k < n_exact_ir_valley_locs_count; k++)
    {
        if (an_exact_ir_valley_locs[k] > buffer_len)
        {
            *spo2 = MAX30102_INVALID_VALUE;
            *spo2_valid = 0;
            return 0;
        }
    }

    for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++)
    {
        n_y_dc_max = MAX30102_SIGNAL_MIN;
        n_x_dc_max = MAX30102_SIGNAL_MIN;

        if (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k] > 10)
        {
            for (i = an_exact_ir_valley_locs[k]; i < an_exact_ir_valley_locs[k + 1]; i++)
            {
                if (max30102_ir[i] > n_x_dc_max)
                {
                    n_x_dc_max = max30102_ir[i];
                    n_x_dc_max_idx = i;
                }
                if (max30102_red[i] > n_y_dc_max)
                {
                    n_y_dc_max = max30102_red[i];
                    n_y_dc_max_idx = i;
                }
            }

            n_y_ac = (max30102_red[an_exact_ir_valley_locs[k + 1]] - max30102_red[an_exact_ir_valley_locs[k]]) *
                     (n_y_dc_max_idx - an_exact_ir_valley_locs[k]);
            n_y_ac = max30102_red[an_exact_ir_valley_locs[k]] +
                     n_y_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
            n_y_ac = max30102_red[n_y_dc_max_idx] - n_y_ac;

            n_x_ac = (max30102_ir[an_exact_ir_valley_locs[k + 1]] - max30102_ir[an_exact_ir_valley_locs[k]]) *
                     (n_x_dc_max_idx - an_exact_ir_valley_locs[k]);
            n_x_ac = max30102_ir[an_exact_ir_valley_locs[k]] +
                     n_x_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
            n_x_ac = max30102_ir[n_y_dc_max_idx] - n_x_ac;

            n_nume = (n_y_ac * n_x_dc_max) >> 7;
            n_denom = (n_x_ac * n_y_dc_max) >> 7;
            if (n_denom > 0 && n_i_ratio_count < MAX30102_MAX_RATIOS && n_nume != 0)
            {
                an_ratio[n_i_ratio_count] = (n_nume * 20) / n_denom;
                n_i_ratio_count++;
            }
        }
    }

    max30102_sort_ascend(an_ratio, n_i_ratio_count);
    n_middle_idx = n_i_ratio_count / 2;

    if (n_middle_idx > 1)
    {
        n_ratio_average = (an_ratio[n_middle_idx - 1] + an_ratio[n_middle_idx]) / 2;
    }
    else
    {
        n_ratio_average = an_ratio[n_middle_idx];
    }

    if (n_ratio_average > 2 && n_ratio_average < MAX30102_SPO2_TABLE_SIZE)
    {
        n_spo2_calc = max30102_spo2_table[n_ratio_average];
        *spo2 = n_spo2_calc;
        *spo2_valid = 1;
    }
    else
    {
        *spo2 = MAX30102_INVALID_VALUE;
        *spo2_valid = 0;
    }

    return 0;
}
/************************ (C) COPYRIGHT Sifli Technology *******END OF FILE****/
