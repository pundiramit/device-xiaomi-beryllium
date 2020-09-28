/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIR_FILTER_H
#define FIR_FILTER_H

#include <stdint.h>

typedef enum fir_filter_mode { FIR_SINGLE_FILTER = 0, FIR_PER_CHANNEL_FILTER } fir_filter_mode_t;

typedef struct fir_filter {
    fir_filter_mode_t mode;
    uint32_t channels;
    uint32_t filter_length;
    uint32_t buffer_size;
    int16_t* coeffs;
    int16_t* state;
} fir_filter_t;

fir_filter_t* fir_init(uint32_t channels, fir_filter_mode_t mode, uint32_t filter_length,
                       uint32_t input_length, int16_t* coeffs);
void fir_release(fir_filter_t* fir);
void fir_reset(fir_filter_t* fir);
void fir_process_interleaved(fir_filter_t* fir, int16_t* input, int16_t* output, uint32_t samples);

#endif /* #ifndef FIR_FILTER_H */
