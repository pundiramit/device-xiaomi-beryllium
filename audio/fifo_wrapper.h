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

#ifndef _AUDIO_FIFO_WRAPPER_H_
#define _AUDIO_FIFO_WRAPPER_H_

#ifdef __cplusplus
extern "C" {
#endif

void *fifo_init(uint32_t bytes, bool reader_throttles_writer);
void fifo_release(void *fifo_itfe);
ssize_t fifo_read(void *fifo_itfe, void *buffer, size_t bytes);
ssize_t fifo_write(void *fifo_itfe, void *buffer, size_t bytes);
ssize_t fifo_available_to_read(void *fifo_itfe);
ssize_t fifo_available_to_write(void *fifo_itfe);
ssize_t fifo_flush(void *fifo_itfe);

#ifdef __cplusplus
}
#endif
#endif /* #ifndef _AUDIO_FIFO_WRAPPER_H_ */
