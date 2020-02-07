/*
 * Copyright (c) 2008-2009, Courtney Cavin
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
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

#include <stdlib.h>
#include "map.h"

struct map_entry {
	struct map_item *item;
};

/* Marker for deleted items */
static struct map_item deleted;

void map_destroy(struct map *map)
{
	free(map->data);
}

void map_clear(struct map *map, void (*release)(struct map_item *))
{
	int i;

	for (i = 0; i < map->size; ++i){
		if (!map->data[i].item)
			continue;
		if (map->data[i].item != &deleted)
			(* release)(map->data[i].item);
		map->data[i].item = NULL;
	}
	map->count = 0;
}

int map_create(struct map *map)
{
	map->size = 0;
	map->data = 0;
	map->count = 0;
	return 0;
}

static int map_hash(struct map *map, unsigned int key)
{
	struct map_entry *e;
	int idx, i;

	if (map->count == map->size)
		return -1;

	idx = key % map->size;

	for (i = 0; i < map->size; ++i) {
		e = &map->data[idx];
		if (!e->item || e->item == &deleted) {
			++map->count;
			return idx;
		}
		if (e->item->key == key)
			return idx;

		idx = (idx + 1) % map->size;
	}

	return -2;
}

static int map_rehash(struct map *map);

int map_reput(struct map *map, unsigned int key, struct map_item *value,
		struct map_item **old)
{
	int rc;

	while ((rc = map_hash(map, key)) < 0) {
		if ((rc = map_rehash(map)) < 0)
			return rc;
	}

	if (old) {
		if (map->data[rc].item == &deleted)
			*old = NULL;
		else
			*old = map->data[rc].item;
	}
	map->data[rc].item = value;
	if (value)
		map->data[rc].item->key = key;

	return 0;
}

int map_put(struct map *map, unsigned int key, struct map_item *value)
{
	return map_reput(map, key, value, NULL);
}

static int map_rehash(struct map *map)
{
	struct map_entry *oldt, *newt;
	int o_size, i;
	int rc;

	newt = calloc(sizeof(struct map_entry), map->size + 256);
	if (!newt)
		return -1;

	oldt = map->data;
	map->data = newt;

	o_size = map->size;
	map->size += 256;
	map->count = 0;

	for (i = 0; i < o_size; ++i){
		if (!oldt[i].item || oldt[i].item == &deleted)
			continue;
		rc = map_put(map, oldt[i].item->key, oldt[i].item);
		if (rc < 0)
			return rc;
	}

	free(oldt);

	return 0;
}

static struct map_entry *map_find(const struct map *map, unsigned int key)
{
	struct map_entry *e;
	int idx, i;

	if (map->size == 0)
		return NULL;

	idx = key % map->size;

	for (i = 0; i < map->size; ++i) {
		e = &map->data[idx];
		idx = (idx + 1) % map->size;

		if (!e->item)
			break;
		if (e->item == &deleted)
			continue;
		if (e->item->key == key)
			return e;
	}
	return NULL;
}

int map_contains(const struct map *map, unsigned int key)
{
	return (map_find(map, key) == NULL) ? 0 : 1;
}

struct map_item *map_get(const struct map *map, unsigned int key)
{
	struct map_entry *e;

	e = map_find(map, key);
	if (e == NULL)
		return NULL;
	return e->item;
}

int map_remove(struct map *map, unsigned int key)
{
	struct map_entry *e;

	e = map_find(map, key);
	if (e) {
		e->item = &deleted;
		--map->count;
	}
	return !e;
}

unsigned int map_length(struct map *map)
{
	return map ? map->count : 0;
}

static struct map_entry *map_iter_from(const struct map *map, unsigned int start)
{
	unsigned int i = start;

	for (; i < map->size; ++i) {
		if (map->data[i].item && map->data[i].item != &deleted)
			return &map->data[i];
	}
	return NULL;
}

struct map_entry *map_iter_next(const struct map *map, struct map_entry *iter)
{
	if (iter == NULL)
		return NULL;

	return map_iter_from(map, (iter - map->data) + 1);
}

struct map_entry *map_iter_first(const struct map *map)
{
	return map_iter_from(map, 0);
}


struct map_item *map_iter_item(struct map_entry *iter)
{
	return iter->item;
}
