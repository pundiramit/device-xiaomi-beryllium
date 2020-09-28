/*
 * Copyright (C) 2019 The Android Open Source Project
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

/*
 * Definitions and interface related to HAL implementations of Acoustic Echo Canceller (AEC).
 *
 * AEC cleans the microphone signal by removing from it audio data corresponding to loudspeaker
 * playback. Note that this process can be nonlinear.
 *
 */

#ifndef _AUDIO_AEC_H_
#define _AUDIO_AEC_H_

#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <hardware/audio.h>
#include <audio_utils/resampler.h>
#include "audio_hw.h"
#include "fifo_wrapper.h"

struct aec_t {
    pthread_mutex_t lock;
    size_t num_reference_channels;
    bool mic_initialized;
    int32_t *mic_buf;
    size_t mic_num_channels;
    size_t mic_buf_size_bytes;
    size_t mic_frame_size_bytes;
    uint32_t mic_sampling_rate;
    struct aec_info last_mic_info;
    bool spk_initialized;
    int32_t *spk_buf;
    size_t spk_num_channels;
    size_t spk_buf_size_bytes;
    size_t spk_frame_size_bytes;
    uint32_t spk_sampling_rate;
    struct aec_info last_spk_info;
    int16_t *spk_buf_playback_format;
    int16_t *spk_buf_resampler_out;
    void *spk_fifo;
    void *ts_fifo;
    ssize_t read_write_diff_bytes;
    struct resampler_itfe *spk_resampler;
    bool spk_running;
    bool prev_spk_running;
};

/* Initialize AEC object.
 * This must be called when the audio device is opened.
 * ALSA device mutex must be held before calling this API.
 * Returns -EINVAL if AEC object fails to initialize, else returns 0. */
int init_aec (int sampling_rate, int num_reference_channels,
                int num_microphone_channels, struct aec_t **);

/* Release AEC object.
 * This must be called when the audio device is closed. */
void release_aec(struct aec_t* aec);

/* Initialize reference configuration for AEC.
 * Must be called when a new output stream is opened.
 * Returns -EINVAL if any processing block fails to initialize,
 * else returns 0. */
int init_aec_reference_config (struct aec_t *aec, struct alsa_stream_out *out);

/* Clear reference configuration for AEC.
 * Must be called when the output stream is closed. */
void destroy_aec_reference_config (struct aec_t *aec);

/* Initialize microphone configuration for AEC.
 * Must be called when a new input stream is opened.
 * Returns -EINVAL if any processing block fails to initialize,
 * else returns 0. */
int init_aec_mic_config(struct aec_t* aec, struct alsa_stream_in* in);

/* Clear microphone configuration for AEC.
 * Must be called when the input stream is closed. */
void destroy_aec_mic_config (struct aec_t *aec);

/* Used to communicate playback state (running or not) to AEC interface.
 * This is used by process_aec() to determine if AEC processing is to be run. */
void aec_set_spk_running (struct aec_t *aec, bool state);

/* Used to communicate playback state (running or not) to the caller. */
bool aec_get_spk_running(struct aec_t* aec);

/* Write audio samples to AEC reference FIFO for use in AEC.
 * Both audio samples and timestamps are added in FIFO fashion.
 * Must be called after every write to PCM.
 * Returns -ENOMEM if the write fails, else returns 0. */
int write_to_reference_fifo(struct aec_t* aec, void* buffer, struct aec_info* info);

/* Get reference audio samples + timestamp, in the format expected by AEC,
 * i.e. same sample rate and bit rate as microphone audio.
 * Timestamp is updated in field 'timestamp_usec', and not in 'timestamp'.
 * Returns:
 *  -EINVAL    if the AEC object is invalid.
 *  -ENOMEM    if the reference FIFO overflows or is corrupted.
 *  -ETIMEDOUT if we timed out waiting for the requested number of bytes
 *  0          otherwise */
int get_reference_samples(struct aec_t* aec, void* buffer, struct aec_info* info);

#ifdef AEC_HAL

/* Processing function call for AEC.
 * AEC output is updated at location pointed to by 'buffer'.
 * This function does not run AEC when there is no playback -
 * as communicated to this AEC interface using aec_set_spk_running().
 * Returns -EINVAL if processing fails, else returns 0. */
int process_aec(struct aec_t* aec, void* buffer, struct aec_info* info);

#else /* #ifdef AEC_HAL */

#define process_aec(...) ((int)0)

#endif /* #ifdef AEC_HAL */

#endif /* _AUDIO_AEC_H_ */
