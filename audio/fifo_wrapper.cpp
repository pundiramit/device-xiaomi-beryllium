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

#define LOG_TAG "audio_utils_fifo_wrapper"
// #define LOG_NDEBUG 0

#include <stdint.h>
#include <errno.h>
#include <log/log.h>
#include <audio_utils/fifo.h>
#include "fifo_wrapper.h"

struct audio_fifo_itfe {
    audio_utils_fifo *p_fifo;
    audio_utils_fifo_reader *p_fifo_reader;
    audio_utils_fifo_writer *p_fifo_writer;
    int8_t *p_buffer;
};

void *fifo_init(uint32_t bytes, bool reader_throttles_writer) {
    struct audio_fifo_itfe *interface = new struct audio_fifo_itfe;
    interface->p_buffer = new int8_t[bytes];
    if (interface->p_buffer == NULL) {
        ALOGE("Failed to allocate fifo buffer!");
        return NULL;
    }
    interface->p_fifo = new audio_utils_fifo(bytes, 1, interface->p_buffer, reader_throttles_writer);
    interface->p_fifo_writer = new audio_utils_fifo_writer(*interface->p_fifo);
    interface->p_fifo_reader = new audio_utils_fifo_reader(*interface->p_fifo);

    return (void *)interface;
}

void fifo_release(void *fifo_itfe) {
    struct audio_fifo_itfe *interface = static_cast<struct audio_fifo_itfe *>(fifo_itfe);
    delete interface->p_fifo_writer;
    delete interface->p_fifo_reader;
    delete interface->p_fifo;
    delete[] interface->p_buffer;
    delete interface;
}

ssize_t fifo_read(void *fifo_itfe, void *buffer, size_t bytes) {
    struct audio_fifo_itfe *interface = static_cast<struct audio_fifo_itfe *>(fifo_itfe);
    return interface->p_fifo_reader->read(buffer, bytes);
}

ssize_t fifo_write(void *fifo_itfe, void *buffer, size_t bytes) {
    struct audio_fifo_itfe *interface = static_cast<struct audio_fifo_itfe *>(fifo_itfe);
    return interface->p_fifo_writer->write(buffer, bytes);
}

ssize_t fifo_available_to_read(void *fifo_itfe) {
    struct audio_fifo_itfe *interface = static_cast<struct audio_fifo_itfe *>(fifo_itfe);
    return interface->p_fifo_reader->available();
}

ssize_t fifo_available_to_write(void *fifo_itfe) {
    struct audio_fifo_itfe *interface = static_cast<struct audio_fifo_itfe *>(fifo_itfe);
    return interface->p_fifo_writer->available();
}

ssize_t fifo_flush(void *fifo_itfe) {
    struct audio_fifo_itfe *interface = static_cast<struct audio_fifo_itfe *>(fifo_itfe);
    return interface->p_fifo_reader->flush();
}
