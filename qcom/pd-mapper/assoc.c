/*
 * Copyright (c) 2013, Bjorn Andersson <bjorn@kryo.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "assoc.h"

static unsigned long assoc_hash(const char *value)
{
	unsigned long hash = 0;
	unsigned long g;
	const char *v = value;

	while (*v) {
		hash = (hash << 4) + *(v++);
		g = hash & 0xF0000000L;
		if (g)
			hash ^= g >> 24;
		hash &= ~g;
	}

	return hash;
}

void assoc_init(struct assoc *assoc, unsigned long size)
{
	assert(size > 0);

	assoc->size = size;
	assoc->fill = 0;
	assoc->keys = calloc(size, sizeof(const char *));
	assoc->values = malloc(size * sizeof(void *));
}

void *assoc_get(struct assoc *assoc, const char *key)
{
	unsigned long hash;

	hash = assoc_hash(key) % assoc->size;
	while (assoc->keys[hash]) {
		if (!strcmp(assoc->keys[hash], key))
			return assoc->values[hash];

		hash = (hash + 1) % assoc->size;
	}

	return NULL;
}

static void _assoc_set(struct assoc *assoc, const char *key, void *value)
{
	struct assoc new_set;
	unsigned long hash;
	unsigned long i;

	assert(assoc->fill < assoc->size);

	/* Grow set at 80% utilization */
	if (5 * assoc->fill > 4 * assoc->size) {
		assoc_init(&new_set, assoc->size * 5 / 4);

		for (i = 0; i < assoc->size; i++)
			if (assoc->keys[i])
				assoc_set(&new_set, assoc->keys[i],
					  assoc->values[i]);

		free(assoc->keys);
		free(assoc->values);

		assoc->keys = new_set.keys;
		assoc->values = new_set.values;
		assoc->fill = new_set.fill;
		assoc->size = new_set.size;
	}

	hash = assoc_hash(key) % assoc->size;
	while (assoc->keys[hash]) {
		if (!strcmp(assoc->keys[hash], key)) {
			assoc->values[hash] = value;
			return;
		}

		hash = (hash + 1) % assoc->size;
	}

	assoc->keys[hash] = key;
	assoc->values[hash] = value;
	assoc->fill++;
}

void assoc_set(struct assoc *assoc, const char *key, void *value)
{
	_assoc_set(assoc, strdup(key), value);
}

const char *assoc_next(struct assoc *assoc, void **value, unsigned long *iter)
{
	unsigned long it = *iter;

	while (it < assoc->size && !assoc->keys[it])
		it++;

	if (it == assoc->size)
		return NULL;

	*iter = it + 1;

	if (it < assoc->size) {
		if (value)
			*value = assoc->values[it];
		return assoc->keys[it];
	} else {
		return NULL;
	}
}

void assoc_destroy(struct assoc *assoc)
{
	unsigned long i;

	for (i = 0; i < assoc->size; i++)
		free((void*)assoc->keys[i]);

	free(assoc->keys);
	free(assoc->values);
	assoc->size = 0;
}
