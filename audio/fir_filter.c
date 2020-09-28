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

#define LOG_TAG "audio_hw_fir_filter"
//#define LOG_NDEBUG 0

#include <assert.h>
#include <audio_utils/primitives.h>
#include <errno.h>
#include <inttypes.h>
#include <log/log.h>
#include <malloc.h>
#include <string.h>

#include "fir_filter.h"

#ifdef __ARM_NEON
#include "arm_neon.h"
#endif /* #ifdef __ARM_NEON */

fir_filter_t* fir_init(uint32_t channels, fir_filter_mode_t mode, uint32_t filter_length,
                       uint32_t input_length, int16_t* coeffs) {
    if ((channels == 0) || (filter_length == 0) || (coeffs == NULL)) {
        ALOGE("%s: Invalid channel count, filter length or coefficient array.", __func__);
        return NULL;
    }

    fir_filter_t* fir = (fir_filter_t*)calloc(1, sizeof(fir_filter_t));
    if (fir == NULL) {
        ALOGE("%s: Unable to allocate memory for fir_filter.", __func__);
        return NULL;
    }

    fir->channels = channels;
    fir->filter_length = filter_length;
    /* Default: same filter coeffs for all channels */
    fir->mode = FIR_SINGLE_FILTER;
    uint32_t coeff_bytes = fir->filter_length * sizeof(int16_t);
    if (mode == FIR_PER_CHANNEL_FILTER) {
        fir->mode = FIR_PER_CHANNEL_FILTER;
        coeff_bytes = fir->filter_length * fir->channels * sizeof(int16_t);
    }

    fir->coeffs = (int16_t*)malloc(coeff_bytes);
    if (fir->coeffs == NULL) {
        ALOGE("%s: Unable to allocate memory for FIR coeffs", __func__);
        goto exit_1;
    }
    memcpy(fir->coeffs, coeffs, coeff_bytes);

    fir->buffer_size = (input_length + fir->filter_length) * fir->channels;
    fir->state = (int16_t*)malloc(fir->buffer_size * sizeof(int16_t));
    if (fir->state == NULL) {
        ALOGE("%s: Unable to allocate memory for FIR state", __func__);
        goto exit_2;
    }

#ifdef __ARM_NEON
    ALOGI("%s: Using ARM Neon", __func__);
#endif /* #ifdef __ARM_NEON */

    fir_reset(fir);
    return fir;

exit_2:
    free(fir->coeffs);
exit_1:
    free(fir);
    return NULL;
}

void fir_release(fir_filter_t* fir) {
    if (fir == NULL) {
        return;
    }
    free(fir->state);
    free(fir->coeffs);
    free(fir);
}

void fir_reset(fir_filter_t* fir) {
    if (fir == NULL) {
        return;
    }
    memset(fir->state, 0, fir->buffer_size * sizeof(int16_t));
}

void fir_process_interleaved(fir_filter_t* fir, int16_t* input, int16_t* output, uint32_t samples) {
    assert(fir != NULL);

    int start_offset = (fir->filter_length - 1) * fir->channels;
    memcpy(&fir->state[start_offset], input, samples * fir->channels * sizeof(int16_t));
    // int ch;
    bool use_2nd_set_coeffs = (fir->channels > 1) && (fir->mode == FIR_PER_CHANNEL_FILTER);
    int16_t* p_coeff_A = &fir->coeffs[0];
    int16_t* p_coeff_B = use_2nd_set_coeffs ? &fir->coeffs[fir->filter_length] : &fir->coeffs[0];
    int16_t* p_output;
    for (int ch = 0; ch < fir->channels; ch += 2) {
        p_output = &output[ch];
        int offset = start_offset + ch;
        for (int s = 0; s < samples; s++) {
            int32_t acc_A = 0;
            int32_t acc_B = 0;

#ifdef __ARM_NEON
            int32x4_t acc_vec = vdupq_n_s32(0);
            for (int k = 0; k < fir->filter_length; k++, offset -= fir->channels) {
                int16x4_t coeff_vec = vdup_n_s16(p_coeff_A[k]);
                coeff_vec = vset_lane_s16(p_coeff_B[k], coeff_vec, 1);
                int16x4_t input_vec = vld1_s16(&fir->state[offset]);
                acc_vec = vmlal_s16(acc_vec, coeff_vec, input_vec);
            }
            acc_A = vgetq_lane_s32(acc_vec, 0);
            acc_B = vgetq_lane_s32(acc_vec, 1);
#else
            for (int k = 0; k < fir->filter_length; k++, offset -= fir->channels) {
                int32_t input_A = (int32_t)(fir->state[offset]);
                int32_t coeff_A = (int32_t)(p_coeff_A[k]);
                int32_t input_B = (int32_t)(fir->state[offset + 1]);
                int32_t coeff_B = (int32_t)(p_coeff_B[k]);
                acc_A += (input_A * coeff_A);
                acc_B += (input_B * coeff_B);
            }
#endif /* #ifdef __ARM_NEON */

            *p_output = clamp16(acc_A >> 15);
            if (ch < fir->channels - 1) {
                *(p_output + 1) = clamp16(acc_B >> 15);
            }
            /* Move to next sample */
            p_output += fir->channels;
            offset += (fir->filter_length + 1) * fir->channels;
        }
        if (use_2nd_set_coeffs) {
            p_coeff_A += (fir->filter_length << 1);
            p_coeff_B += (fir->filter_length << 1);
        }
    }
    memmove(fir->state, &fir->state[samples * fir->channels],
            (fir->filter_length - 1) * fir->channels * sizeof(int16_t));
}
