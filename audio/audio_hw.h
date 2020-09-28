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

#ifndef _YUKAWA_AUDIO_HW_H_
#define _YUKAWA_AUDIO_HW_H_

#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>

#include "fir_filter.h"

#define CARD_OUT 0
#define PORT_INTERNAL_SPEAKER 0
#define CARD_IN 0
#define PORT_BUILTIN_MIC 3

#define MIXER_XML_PATH "/vendor/etc/mixer_paths.xml"
/* Minimum granularity - Arbitrary but small value */
#define CODEC_BASE_FRAME_COUNT 32

#define CHANNEL_STEREO 2

#ifdef AEC_HAL
#define NUM_AEC_REFERENCE_CHANNELS 1
#else
/* App AEC uses 2-channel reference */
#define NUM_AEC_REFERENCE_CHANNELS 2
#endif /* #ifdef AEC_HAL */

#define DEBUG_AEC 0

#define PCM_OPEN_RETRIES 100
#define PCM_OPEN_WAIT_TIME_MS 20

/* Capture codec parameters */
/* Set up a capture period of 32 ms:
 * CAPTURE_PERIOD = PERIOD_SIZE / SAMPLE_RATE, so (32e-3) = PERIOD_SIZE / (16e3)
 * => PERIOD_SIZE = 512 frames, where each "frame" consists of 1 sample of every channel (here, 2ch) */
#define CAPTURE_PERIOD_MULTIPLIER 16
#define CAPTURE_PERIOD_SIZE (CODEC_BASE_FRAME_COUNT * CAPTURE_PERIOD_MULTIPLIER)
#define CAPTURE_PERIOD_COUNT 4
#define CAPTURE_PERIOD_START_THRESHOLD 0
#define CAPTURE_CODEC_SAMPLING_RATE 16000

/* Playback codec parameters */
/* number of base blocks in a short period (low latency) */
#define PLAYBACK_PERIOD_MULTIPLIER 32  /* 21 ms */
/* number of frames per short period (low latency) */
#define PLAYBACK_PERIOD_SIZE (CODEC_BASE_FRAME_COUNT * PLAYBACK_PERIOD_MULTIPLIER)
/* number of pseudo periods for low latency playback */
#define PLAYBACK_PERIOD_COUNT 4
#define PLAYBACK_PERIOD_START_THRESHOLD 2
#define PLAYBACK_CODEC_SAMPLING_RATE 48000
#define MIN_WRITE_SLEEP_US      5000

#define SPEAKER_EQ_FILE "/vendor/etc/speaker_eq_sei610.fir"
#define SPEAKER_MAX_EQ_LENGTH 512

struct alsa_audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock;   /* see notes in in_read/out_write on mutex acquisition order */
    struct alsa_stream_in *active_input;
    struct alsa_stream_out *active_output;
    struct audio_route *audio_route;
    struct mixer *mixer;
    bool mic_mute;
    struct aec_t *aec;
};

struct alsa_stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock;   /* see note in in_read() on mutex acquisition order */
    audio_devices_t devices;
    struct pcm_config config;
    struct pcm *pcm;
    bool unavailable;
    bool standby;
    struct alsa_audio_device *dev;
    int read_threshold;
    unsigned int frames_read;
    uint64_t timestamp_nsec;
    audio_source_t source;
};

struct alsa_stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;   /* see note in out_write() on mutex acquisition order */
    audio_devices_t devices;
    struct pcm_config config;
    struct pcm *pcm;
    bool unavailable;
    int standby;
    struct alsa_audio_device *dev;
    int write_threshold;
    unsigned int frames_written;
    struct timespec timestamp;
    fir_filter_t* speaker_eq;
};

/* 'bytes' are the number of bytes written to audio FIFO, for which 'timestamp' is valid.
 * 'available' is the number of frames available to read (for input) or yet to be played
 * (for output) frames in the PCM buffer.
 * timestamp and available are updated by pcm_get_htimestamp(), so they use the same
 * datatypes as the corresponding arguments to that function. */
struct aec_info {
    struct timespec timestamp;
    uint64_t timestamp_usec;
    unsigned int available;
    size_t bytes;
};

#endif /* #ifndef _YUKAWA_AUDIO_HW_H_ */
