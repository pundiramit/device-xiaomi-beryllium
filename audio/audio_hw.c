/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *
 * Copied as it is from device/amlogic/generic/hal/audio/
 */

#define LOG_TAG "audio_hw_generic"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <inttypes.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include <log/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <audio_effects/effect_aec.h>
#include <audio_route/audio_route.h>
#include <audio_utils/clock.h>
#include <audio_utils/echo_reference.h>
#include <audio_utils/resampler.h>
#include <hardware/audio_alsaops.h>
#include <hardware/audio_effect.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>

#include <sys/ioctl.h>

#include "audio_aec.h"
#include "audio_hw.h"

static int adev_get_mic_mute(const struct audio_hw_device* dev, bool* state);
static int adev_get_microphones(const struct audio_hw_device* dev,
                                struct audio_microphone_characteristic_t* mic_array,
                                size_t* mic_count);
static size_t out_get_buffer_size(const struct audio_stream* stream);

static int get_audio_output_port(audio_devices_t devices) {
    /* Default to internal speaker */
    int port = PORT_INTERNAL_SPEAKER;
    return port;
}

static void timestamp_adjust(struct timespec* ts, ssize_t frames, uint32_t sampling_rate) {
    /* This function assumes the adjustment (in nsec) is less than the max value of long,
     * which for 32-bit long this is 2^31 * 1e-9 seconds, slightly over 2 seconds.
     * For 64-bit long it is  9e+9 seconds. */
    long adj_nsec = (frames / (float) sampling_rate) * 1E9L;
    ts->tv_nsec += adj_nsec;
    while (ts->tv_nsec > 1E9L) {
        ts->tv_sec++;
        ts->tv_nsec -= 1E9L;
    }
    if (ts->tv_nsec < 0) {
        ts->tv_sec--;
        ts->tv_nsec += 1E9L;
    }
}

/* Helper function to get PCM hardware timestamp.
 * Only the field 'timestamp' of argument 'ts' is updated. */
static int get_pcm_timestamp(struct pcm* pcm, uint32_t sample_rate, struct aec_info* info,
                             bool isOutput) {
    int ret = 0;
    if (pcm_get_htimestamp(pcm, &info->available, &info->timestamp) < 0) {
        ALOGE("Error getting PCM timestamp!");
        info->timestamp.tv_sec = 0;
        info->timestamp.tv_nsec = 0;
        return -EINVAL;
    }
    ssize_t frames;
    if (isOutput) {
        frames = pcm_get_buffer_size(pcm) - info->available;
    } else {
        frames = -info->available; /* rewind timestamp */
    }
    timestamp_adjust(&info->timestamp, frames, sample_rate);
    return ret;
}

static int read_filter_from_file(const char* filename, int16_t* filter, int max_length) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        ALOGI("%s: File %s not found.", __func__, filename);
        return 0;
    }
    int num_taps = 0;
    char* line = NULL;
    size_t len = 0;
    while (!feof(fp)) {
        size_t size = getline(&line, &len, fp);
        if ((line[0] == '#') || (size < 2)) {
            continue;
        }
        int n = sscanf(line, "%" SCNd16 "\n", &filter[num_taps++]);
        if (n < 1) {
            ALOGE("Could not find coefficient %d! Exiting...", num_taps - 1);
            return 0;
        }
        ALOGV("Coeff %d : %" PRId16, num_taps, filter[num_taps - 1]);
        if (num_taps == max_length) {
            ALOGI("%s: max tap length %d reached.", __func__, max_length);
            break;
        }
    }
    free(line);
    fclose(fp);
    return num_taps;
}

static void out_set_eq(struct alsa_stream_out* out) {
    out->speaker_eq = NULL;
    int16_t* speaker_eq_coeffs = (int16_t*)calloc(SPEAKER_MAX_EQ_LENGTH, sizeof(int16_t));
    if (speaker_eq_coeffs == NULL) {
        ALOGE("%s: Failed to allocate speaker EQ", __func__);
        return;
    }
    int num_taps = read_filter_from_file(SPEAKER_EQ_FILE, speaker_eq_coeffs, SPEAKER_MAX_EQ_LENGTH);
    if (num_taps == 0) {
        ALOGI("%s: Empty filter file or 0 taps set.", __func__);
        free(speaker_eq_coeffs);
        return;
    }
    out->speaker_eq = fir_init(
            out->config.channels, FIR_SINGLE_FILTER, num_taps,
            out_get_buffer_size(&out->stream.common) / out->config.channels / sizeof(int16_t),
            speaker_eq_coeffs);
    free(speaker_eq_coeffs);
}

/* must be called with hw device and output stream mutexes locked */
static int start_output_stream(struct alsa_stream_out *out)
{
    struct alsa_audio_device *adev = out->dev;

    /* default to low power: will be corrected in out_write if necessary before first write to
     * tinyalsa.
     */
    out->write_threshold = PLAYBACK_PERIOD_COUNT * PLAYBACK_PERIOD_SIZE;
    out->config.start_threshold = PLAYBACK_PERIOD_START_THRESHOLD * PLAYBACK_PERIOD_SIZE;
    out->config.avail_min = PLAYBACK_PERIOD_SIZE;
    out->unavailable = true;
    unsigned int pcm_retry_count = PCM_OPEN_RETRIES;
    int out_port = get_audio_output_port(out->devices);

    while (1) {
        out->pcm = pcm_open(CARD_OUT, out_port, PCM_OUT | PCM_MONOTONIC, &out->config);
        if ((out->pcm != NULL) && pcm_is_ready(out->pcm)) {
            break;
        } else {
            ALOGE("cannot open pcm_out driver: %s", pcm_get_error(out->pcm));
            if (out->pcm != NULL) {
                pcm_close(out->pcm);
                out->pcm = NULL;
            }
            if (--pcm_retry_count == 0) {
                ALOGE("Failed to open pcm_out after %d tries", PCM_OPEN_RETRIES);
                return -ENODEV;
            }
            usleep(PCM_OPEN_WAIT_TIME_MS * 1000);
        }
    }
    out->unavailable = false;
    adev->active_output = out;
    return 0;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct alsa_stream_out *out = (struct alsa_stream_out *)stream;
    return out->config.rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    ALOGV("out_set_sample_rate: %d", 0);
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    ALOGV("out_get_buffer_size: %d", 4096);

    /* return the closest majoring multiple of 16 frames, as
     * audioflinger expects audio buffers to be a multiple of 16 frames */
    size_t size = PLAYBACK_PERIOD_SIZE;
    size = ((size + 15) / 16) * 16;
    return size * audio_stream_out_frame_size((struct audio_stream_out *)stream);
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    ALOGV("out_get_channels");
    struct alsa_stream_out *out = (struct alsa_stream_out *)stream;
    return audio_channel_out_mask_from_count(out->config.channels);
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    ALOGV("out_get_format");
    struct alsa_stream_out *out = (struct alsa_stream_out *)stream;
    return audio_format_from_pcm_format(out->config.format);
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    ALOGV("out_set_format: %d",format);
    return -ENOSYS;
}

static int do_output_standby(struct alsa_stream_out *out)
{
    struct alsa_audio_device *adev = out->dev;

    fir_reset(out->speaker_eq);

    if (!out->standby) {
        pcm_close(out->pcm);
        out->pcm = NULL;
        adev->active_output = NULL;
        out->standby = 1;
    }
    aec_set_spk_running(adev->aec, false);
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    ALOGV("out_standby");
    struct alsa_stream_out *out = (struct alsa_stream_out *)stream;
    int status;

    pthread_mutex_lock(&out->dev->lock);
    pthread_mutex_lock(&out->lock);
    status = do_output_standby(out);
    pthread_mutex_unlock(&out->lock);
    pthread_mutex_unlock(&out->dev->lock);
    return status;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    ALOGV("out_dump");
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("out_set_parameters");
    struct alsa_stream_out *out = (struct alsa_stream_out *)stream;
    struct alsa_audio_device *adev = out->dev;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        pthread_mutex_lock(&adev->lock);
        pthread_mutex_lock(&out->lock);
        if (((out->devices & AUDIO_DEVICE_OUT_ALL) != val) && (val != 0)) {
            out->devices &= ~AUDIO_DEVICE_OUT_ALL;
            out->devices |= val;
        }
        pthread_mutex_unlock(&out->lock);
        pthread_mutex_unlock(&adev->lock);
    }

    str_parms_destroy(parms);
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ALOGV("out_get_parameters");
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    ALOGV("out_get_latency");
    struct alsa_stream_out *out = (struct alsa_stream_out *)stream;
    return (PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT * 1000) / out->config.rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
        float right)
{
    ALOGV("out_set_volume: Left:%f Right:%f", left, right);
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
        size_t bytes)
{
    int ret;
    struct alsa_stream_out *out = (struct alsa_stream_out *)stream;
    struct alsa_audio_device *adev = out->dev;
    size_t frame_size = audio_stream_out_frame_size(stream);
    size_t out_frames = bytes / frame_size;

    ALOGV("%s: devices: %d, bytes %zu", __func__, out->devices, bytes);

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the output stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&adev->lock);
    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream(out);
        if (ret != 0) {
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }
        out->standby = 0;
        aec_set_spk_running(adev->aec, true);
    }

    pthread_mutex_unlock(&adev->lock);

    if (out->speaker_eq != NULL) {
        fir_process_interleaved(out->speaker_eq, (int16_t*)buffer, (int16_t*)buffer, out_frames);
    }

    ret = pcm_write(out->pcm, buffer, out_frames * frame_size);
    if (ret == 0) {
        out->frames_written += out_frames;

        struct aec_info info;
        get_pcm_timestamp(out->pcm, out->config.rate, &info, true /*isOutput*/);
        out->timestamp = info.timestamp;
        info.bytes = out_frames * frame_size;
        int aec_ret = write_to_reference_fifo(adev->aec, (void *)buffer, &info);
        if (aec_ret) {
            ALOGE("AEC: Write to speaker loopback FIFO failed!");
        }
    }

exit:
    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        usleep((int64_t)bytes * 1000000 / audio_stream_out_frame_size(stream) /
                out_get_sample_rate(&stream->common));
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
        uint32_t *dsp_frames)
{
    ALOGV("out_get_render_position: dsp_frames: %p", dsp_frames);
    return -ENOSYS;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                   uint64_t *frames, struct timespec *timestamp)
{
    if (stream == NULL || frames == NULL || timestamp == NULL) {
        return -EINVAL;
    }
    struct alsa_stream_out* out = (struct alsa_stream_out*)stream;

    *frames = out->frames_written;
    *timestamp = out->timestamp;
    ALOGV("%s: frames: %" PRIu64 ", timestamp (nsec): %" PRIu64, __func__, *frames,
          audio_utils_ns_from_timespec(timestamp));

    return 0;
}


static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_add_audio_effect: %p", effect);
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_remove_audio_effect: %p", effect);
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
        int64_t *timestamp)
{
    *timestamp = 0;
    ALOGV("out_get_next_write_timestamp: %ld", (long int)(*timestamp));
    return -ENOSYS;
}

/** audio_stream_in implementation **/

/* must be called with hw device and input stream mutexes locked */
static int start_input_stream(struct alsa_stream_in *in)
{
    struct alsa_audio_device *adev = in->dev;
    in->unavailable = true;
    unsigned int pcm_retry_count = PCM_OPEN_RETRIES;

    while (1) {
        in->pcm = pcm_open(CARD_IN, PORT_BUILTIN_MIC, PCM_IN | PCM_MONOTONIC, &in->config);
        if ((in->pcm != NULL) && pcm_is_ready(in->pcm)) {
            break;
        } else {
            ALOGE("cannot open pcm_in driver: %s", pcm_get_error(in->pcm));
            if (in->pcm != NULL) {
                pcm_close(in->pcm);
                in->pcm = NULL;
            }
            if (--pcm_retry_count == 0) {
                ALOGE("Failed to open pcm_in after %d tries", PCM_OPEN_RETRIES);
                return -ENODEV;
            }
            usleep(PCM_OPEN_WAIT_TIME_MS * 1000);
        }
    }
    in->unavailable = false;
    adev->active_input = in;
    return 0;
}

static void get_mic_characteristics(struct audio_microphone_characteristic_t* mic_data,
                                    size_t* mic_count) {
    *mic_count = 1;
    memset(mic_data, 0, sizeof(struct audio_microphone_characteristic_t));
    strlcpy(mic_data->device_id, "builtin_mic", AUDIO_MICROPHONE_ID_MAX_LEN - 1);
    strlcpy(mic_data->address, "top", AUDIO_DEVICE_MAX_ADDRESS_LEN - 1);
    memset(mic_data->channel_mapping, AUDIO_MICROPHONE_CHANNEL_MAPPING_UNUSED,
           sizeof(mic_data->channel_mapping));
    mic_data->device = AUDIO_DEVICE_IN_BUILTIN_MIC;
    mic_data->sensitivity = -37.0;
    mic_data->max_spl = AUDIO_MICROPHONE_SPL_UNKNOWN;
    mic_data->min_spl = AUDIO_MICROPHONE_SPL_UNKNOWN;
    mic_data->orientation.x = 0.0f;
    mic_data->orientation.y = 0.0f;
    mic_data->orientation.z = 0.0f;
    mic_data->geometric_location.x = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic_data->geometric_location.y = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
    mic_data->geometric_location.z = AUDIO_MICROPHONE_COORDINATE_UNKNOWN;
}

static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct alsa_stream_in *in = (struct alsa_stream_in *)stream;
    return in->config.rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    ALOGV("in_set_sample_rate: %d", rate);
    return -ENOSYS;
}

static size_t get_input_buffer_size(size_t frames, audio_format_t format,
                                    audio_channel_mask_t channel_mask) {
    /* return the closest majoring multiple of 16 frames, as
     * audioflinger expects audio buffers to be a multiple of 16 frames */
    frames = ((frames + 15) / 16) * 16;
    size_t bytes_per_frame = audio_channel_count_from_in_mask(channel_mask) *
                            audio_bytes_per_sample(format);
    size_t buffer_size = frames * bytes_per_frame;
    return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    struct alsa_stream_in *in = (struct alsa_stream_in *)stream;
    ALOGV("in_get_channels: %d", in->config.channels);
    return audio_channel_in_mask_from_count(in->config.channels);
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    struct alsa_stream_in *in = (struct alsa_stream_in *)stream;
    ALOGV("in_get_format: %d", in->config.format);
    return audio_format_from_pcm_format(in->config.format);
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct alsa_stream_in* in = (struct alsa_stream_in*)stream;
    size_t frames = CAPTURE_PERIOD_SIZE;
    if (in->source == AUDIO_SOURCE_ECHO_REFERENCE) {
        frames = CAPTURE_PERIOD_SIZE * PLAYBACK_CODEC_SAMPLING_RATE / CAPTURE_CODEC_SAMPLING_RATE;
    }

    size_t buffer_size =
            get_input_buffer_size(frames, stream->get_format(stream), stream->get_channels(stream));
    ALOGV("in_get_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static int in_get_active_microphones(const struct audio_stream_in* stream,
                                     struct audio_microphone_characteristic_t* mic_array,
                                     size_t* mic_count) {
    ALOGV("in_get_active_microphones");
    if ((mic_array == NULL) || (mic_count == NULL)) {
        return -EINVAL;
    }
    struct alsa_stream_in* in = (struct alsa_stream_in*)stream;
    struct audio_hw_device* dev = (struct audio_hw_device*)in->dev;
    bool mic_muted = false;
    adev_get_mic_mute(dev, &mic_muted);
    if ((in->source == AUDIO_SOURCE_ECHO_REFERENCE) || mic_muted) {
        *mic_count = 0;
        return 0;
    }
    adev_get_microphones(dev, mic_array, mic_count);
    return 0;
}

static int do_input_standby(struct alsa_stream_in *in)
{
    struct alsa_audio_device *adev = in->dev;

    if (!in->standby) {
        pcm_close(in->pcm);
        in->pcm = NULL;
        adev->active_input = NULL;
        in->standby = true;
    }
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct alsa_stream_in *in = (struct alsa_stream_in *)stream;
    int status;

    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&in->dev->lock);
    status = do_input_standby(in);
    pthread_mutex_unlock(&in->dev->lock);
    pthread_mutex_unlock(&in->lock);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    struct alsa_stream_in* in = (struct alsa_stream_in*)stream;
    if (in->source == AUDIO_SOURCE_ECHO_REFERENCE) {
        return 0;
    }

    struct audio_microphone_characteristic_t mic_array[AUDIO_MICROPHONE_MAX_COUNT];
    size_t mic_count;

    get_mic_characteristics(mic_array, &mic_count);

    dprintf(fd, "  Microphone count: %zd\n", mic_count);
    size_t idx;
    for (idx = 0; idx < mic_count; idx++) {
        dprintf(fd, "  Microphone: %zd\n", idx);
        dprintf(fd, "    Address: %s\n", mic_array[idx].address);
        dprintf(fd, "    Device: %d\n", mic_array[idx].device);
        dprintf(fd, "    Sensitivity (dB): %.2f\n", mic_array[idx].sensitivity);
    }

    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
        const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
        size_t bytes)
{
    int ret;
    struct alsa_stream_in *in = (struct alsa_stream_in *)stream;
    struct alsa_audio_device *adev = in->dev;
    size_t frame_size = audio_stream_in_frame_size(stream);
    size_t in_frames = bytes / frame_size;

    ALOGV("in_read: stream: %d, bytes %zu", in->source, bytes);

    /* Special handling for Echo Reference: simply get the reference from FIFO.
     * The format and sample rate should be specified by arguments to adev_open_input_stream. */
    if (in->source == AUDIO_SOURCE_ECHO_REFERENCE) {
        struct aec_info info;
        info.bytes = bytes;

        const uint64_t time_increment_nsec = (uint64_t)bytes * NANOS_PER_SECOND /
                                             audio_stream_in_frame_size(stream) /
                                             in_get_sample_rate(&stream->common);
        if (!aec_get_spk_running(adev->aec)) {
            if (in->timestamp_nsec == 0) {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                const uint64_t timestamp_nsec = audio_utils_ns_from_timespec(&now);
                in->timestamp_nsec = timestamp_nsec;
            } else {
                in->timestamp_nsec += time_increment_nsec;
            }
            memset(buffer, 0, bytes);
            const uint64_t time_increment_usec = time_increment_nsec / 1000;
            usleep(time_increment_usec);
        } else {
            int ref_ret = get_reference_samples(adev->aec, buffer, &info);
            if ((ref_ret) || (info.timestamp_usec == 0)) {
                memset(buffer, 0, bytes);
                in->timestamp_nsec += time_increment_nsec;
            } else {
                in->timestamp_nsec = 1000 * info.timestamp_usec;
            }
        }
        in->frames_read += in_frames;

#if DEBUG_AEC
        FILE* fp_ref = fopen("/data/local/traces/aec_ref.pcm", "a+");
        if (fp_ref) {
            fwrite((char*)buffer, 1, bytes, fp_ref);
            fclose(fp_ref);
        } else {
            ALOGE("AEC debug: Could not open file aec_ref.pcm!");
        }
        FILE* fp_ref_ts = fopen("/data/local/traces/aec_ref_timestamps.txt", "a+");
        if (fp_ref_ts) {
            fprintf(fp_ref_ts, "%" PRIu64 "\n", in->timestamp_nsec);
            fclose(fp_ref_ts);
        } else {
            ALOGE("AEC debug: Could not open file aec_ref_timestamps.txt!");
        }
#endif
        return info.bytes;
    }

    /* Microphone input stream read */

    /* acquiring hw device mutex systematically is useful if a low priority thread is waiting
     * on the input stream mutex - e.g. executing select_mode() while holding the hw device
     * mutex
     */
    pthread_mutex_lock(&in->lock);
    pthread_mutex_lock(&adev->lock);
    if (in->standby) {
        ret = start_input_stream(in);
        if (ret != 0) {
            pthread_mutex_unlock(&adev->lock);
            ALOGE("start_input_stream failed with code %d", ret);
            goto exit;
        }
        in->standby = false;
    }

    pthread_mutex_unlock(&adev->lock);

    ret = pcm_read(in->pcm, buffer, in_frames * frame_size);
    struct aec_info info;
    get_pcm_timestamp(in->pcm, in->config.rate, &info, false /*isOutput*/);
    if (ret == 0) {
        in->frames_read += in_frames;
        in->timestamp_nsec = audio_utils_ns_from_timespec(&info.timestamp);
    }
    else {
        ALOGE("pcm_read failed with code %d", ret);
    }

exit:
    pthread_mutex_unlock(&in->lock);

    bool mic_muted = false;
    adev_get_mic_mute((struct audio_hw_device*)adev, &mic_muted);
    if (mic_muted) {
        memset(buffer, 0, bytes);
    }

    if (ret != 0) {
        usleep((int64_t)bytes * 1000000 / audio_stream_in_frame_size(stream) /
                in_get_sample_rate(&stream->common));
    } else {
        /* Process AEC if available */
        /* TODO move to a separate thread */
        if (!mic_muted) {
            info.bytes = bytes;
            int aec_ret = process_aec(adev->aec, buffer, &info);
            if (aec_ret) {
                ALOGE("process_aec returned error code %d", aec_ret);
            }
        }
    }

#if DEBUG_AEC && !defined(AEC_HAL)
    FILE* fp_in = fopen("/data/local/traces/aec_in.pcm", "a+");
    if (fp_in) {
        fwrite((char*)buffer, 1, bytes, fp_in);
        fclose(fp_in);
    } else {
        ALOGE("AEC debug: Could not open file aec_in.pcm!");
    }
    FILE* fp_mic_ts = fopen("/data/local/traces/aec_in_timestamps.txt", "a+");
    if (fp_mic_ts) {
        fprintf(fp_mic_ts, "%" PRIu64 "\n", in->timestamp_nsec);
        fclose(fp_mic_ts);
    } else {
        ALOGE("AEC debug: Could not open file aec_in_timestamps.txt!");
    }
#endif

    return bytes;
}

static int in_get_capture_position(const struct audio_stream_in* stream, int64_t* frames,
                                   int64_t* time) {
    if (stream == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }
    struct alsa_stream_in* in = (struct alsa_stream_in*)stream;

    *frames = in->frames_read;
    *time = in->timestamp_nsec;
    ALOGV("%s: source: %d, timestamp (nsec): %" PRIu64, __func__, in->source, *time);

    return 0;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
        audio_io_handle_t handle,
        audio_devices_t devices,
        audio_output_flags_t flags,
        struct audio_config *config,
        struct audio_stream_out **stream_out,
        const char *address __unused)
{
    ALOGV("adev_open_output_stream...");

    struct alsa_audio_device *ladev = (struct alsa_audio_device *)dev;
    struct alsa_stream_out *out;
    struct pcm_params *params;
    int ret = 0;

    int out_port = get_audio_output_port(devices);

    params = pcm_params_get(CARD_OUT, out_port, PCM_OUT);
    if (!params)
        return -ENOSYS;

    out = (struct alsa_stream_out *)calloc(1, sizeof(struct alsa_stream_out));
    if (!out)
        return -ENOMEM;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->config.channels = CHANNEL_STEREO;
    out->config.rate = PLAYBACK_CODEC_SAMPLING_RATE;
    out->config.format = PCM_FORMAT_S16_LE;
    out->config.period_size = PLAYBACK_PERIOD_SIZE;
    out->config.period_count = PLAYBACK_PERIOD_COUNT;

    if (out->config.rate != config->sample_rate ||
           audio_channel_count_from_out_mask(config->channel_mask) != CHANNEL_STEREO ||
               out->config.format !=  pcm_format_from_audio_format(config->format) ) {
        config->sample_rate = out->config.rate;
        config->format = audio_format_from_pcm_format(out->config.format);
        config->channel_mask = audio_channel_out_mask_from_count(CHANNEL_STEREO);
        ret = -EINVAL;
    }

    ALOGI("adev_open_output_stream selects channels=%d rate=%d format=%d, devices=%d",
          out->config.channels, out->config.rate, out->config.format, devices);

    out->dev = ladev;
    out->standby = 1;
    out->unavailable = false;
    out->devices = devices;

    config->format = out_get_format(&out->stream.common);
    config->channel_mask = out_get_channels(&out->stream.common);
    config->sample_rate = out_get_sample_rate(&out->stream.common);

    *stream_out = &out->stream;

    out->speaker_eq = NULL;
    if (out_port == PORT_INTERNAL_SPEAKER) {
        out_set_eq(out);
        if (out->speaker_eq == NULL) {
            ALOGE("%s: Failed to initialize speaker EQ", __func__);
        }
    }

    /* TODO The retry mechanism isn't implemented in AudioPolicyManager/AudioFlinger. */
    ret = 0;

    if (ret == 0) {
        int aec_ret = init_aec_reference_config(ladev->aec, out);
        if (aec_ret) {
            ALOGE("AEC: Speaker config init failed!");
            return -EINVAL;
        }
    }

    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
        struct audio_stream_out *stream)
{
    ALOGV("adev_close_output_stream...");
    struct alsa_audio_device *adev = (struct alsa_audio_device *)dev;
    destroy_aec_reference_config(adev->aec);
    struct alsa_stream_out* out = (struct alsa_stream_out*)stream;
    fir_release(out->speaker_eq);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    ALOGV("adev_set_parameters");
    return -ENOSYS;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
        const char *keys)
{
    ALOGV("adev_get_parameters");
    return strdup("");
}

static int adev_get_microphones(const struct audio_hw_device* dev,
                                struct audio_microphone_characteristic_t* mic_array,
                                size_t* mic_count) {
    ALOGV("adev_get_microphones");
    if ((mic_array == NULL) || (mic_count == NULL)) {
        return -EINVAL;
    }
    get_mic_characteristics(mic_array, mic_count);
    return 0;
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    ALOGV("adev_init_check");
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_voice_volume: %f", volume);
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_master_volume: %f", volume);
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume)
{
    ALOGV("adev_get_master_volume: %f", *volume);
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    ALOGV("adev_set_master_mute: %d", muted);
    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    ALOGV("adev_get_master_mute: %d", *muted);
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    ALOGV("adev_set_mode: %d", mode);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    ALOGV("adev_set_mic_mute: %d",state);
    struct alsa_audio_device *adev = (struct alsa_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    adev->mic_mute = state;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    ALOGV("adev_get_mic_mute");
    struct alsa_audio_device *adev = (struct alsa_audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    *state = adev->mic_mute;
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
        const struct audio_config *config)
{
    size_t buffer_size =
            get_input_buffer_size(CAPTURE_PERIOD_SIZE, config->format, config->channel_mask);
    ALOGV("adev_get_input_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static int adev_open_input_stream(struct audio_hw_device* dev, audio_io_handle_t handle,
                                  audio_devices_t devices, struct audio_config* config,
                                  struct audio_stream_in** stream_in,
                                  audio_input_flags_t flags __unused, const char* address __unused,
                                  audio_source_t source) {
    ALOGV("adev_open_input_stream...");

    struct alsa_audio_device *ladev = (struct alsa_audio_device *)dev;
    struct alsa_stream_in *in;
    struct pcm_params *params;
    int ret = 0;

    params = pcm_params_get(CARD_IN, PORT_BUILTIN_MIC, PCM_IN);
    if (!params)
        return -ENOSYS;

    in = (struct alsa_stream_in *)calloc(1, sizeof(struct alsa_stream_in));
    if (!in)
        return -ENOMEM;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->stream.get_capture_position = in_get_capture_position;
    in->stream.get_active_microphones = in_get_active_microphones;

    in->config.channels = CHANNEL_STEREO;
    if (source == AUDIO_SOURCE_ECHO_REFERENCE) {
        in->config.rate = PLAYBACK_CODEC_SAMPLING_RATE;
    } else {
        in->config.rate = CAPTURE_CODEC_SAMPLING_RATE;
    }
    in->config.format = PCM_FORMAT_S32_LE;
    in->config.period_size = CAPTURE_PERIOD_SIZE;
    in->config.period_count = CAPTURE_PERIOD_COUNT;

    if (in->config.rate != config->sample_rate ||
           audio_channel_count_from_in_mask(config->channel_mask) != CHANNEL_STEREO ||
               in->config.format !=  pcm_format_from_audio_format(config->format) ) {
        ret = -EINVAL;
    }

    ALOGI("adev_open_input_stream selects channels=%d rate=%d format=%d source=%d",
          in->config.channels, in->config.rate, in->config.format, source);

    in->dev = ladev;
    in->standby = true;
    in->unavailable = false;
    in->source = source;
    in->devices = devices;

    config->format = in_get_format(&in->stream.common);
    config->channel_mask = in_get_channels(&in->stream.common);
    config->sample_rate = in_get_sample_rate(&in->stream.common);

    /* If AEC is in the app, only configure based on ECHO_REFERENCE spec.
     * If AEC is in the HAL, configure using the given mic stream. */
    bool aecInput = true;
#if !defined(AEC_HAL)
    aecInput = (in->source == AUDIO_SOURCE_ECHO_REFERENCE);
#endif

    if ((ret == 0) && aecInput) {
        int aec_ret = init_aec_mic_config(ladev->aec, in);
        if (aec_ret) {
            ALOGE("AEC: Mic config init failed!");
            return -EINVAL;
        }
    }

    if (ret) {
        free(in);
    } else {
        *stream_in = &in->stream;
    }

#if DEBUG_AEC
    remove("/data/local/traces/aec_ref.pcm");
    remove("/data/local/traces/aec_in.pcm");
    remove("/data/local/traces/aec_ref_timestamps.txt");
    remove("/data/local/traces/aec_in_timestamps.txt");
#endif
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
        struct audio_stream_in *stream)
{
    ALOGV("adev_close_input_stream...");
    struct alsa_audio_device *adev = (struct alsa_audio_device *)dev;
    destroy_aec_mic_config(adev->aec);
    free(stream);
    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    ALOGV("adev_dump");
    return 0;
}

static int adev_close(hw_device_t *device)
{
    ALOGV("adev_close");

    struct alsa_audio_device *adev = (struct alsa_audio_device *)device;
    release_aec(adev->aec);
    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    struct alsa_audio_device *adev;

    ALOGV("adev_open: %s", name);

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct alsa_audio_device));
    if (!adev)
        return -ENOMEM;

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->hw_device.common.module = (struct hw_module_t *) module;
    adev->hw_device.common.close = adev_close;
    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.get_master_volume = adev_get_master_volume;
    adev->hw_device.set_master_mute = adev_set_master_mute;
    adev->hw_device.get_master_mute = adev_get_master_mute;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;
    adev->hw_device.get_microphones = adev_get_microphones;

    *device = &adev->hw_device.common;

    adev->mixer = mixer_open(CARD_OUT);

    if (!adev->mixer) {
        ALOGE("Unable to open the mixer, aborting.");
        return -EINVAL;
    }

    adev->audio_route = audio_route_init(CARD_OUT, MIXER_XML_PATH);
    if (!adev->audio_route) {
        ALOGE("%s: Failed to init audio route controls, aborting.", __func__);
        return -EINVAL;
    }

    pthread_mutex_lock(&adev->lock);
    if (init_aec(CAPTURE_CODEC_SAMPLING_RATE, NUM_AEC_REFERENCE_CHANNELS,
                    CHANNEL_STEREO, &adev->aec)) {
        pthread_mutex_unlock(&adev->lock);
        return -EINVAL;
    }
    pthread_mutex_unlock(&adev->lock);

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Generic audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
