#ifndef _MAP_H_
#define _MAP_H_

struct map_item {
	unsigned int key;
};

struct map_entry;

struct map {
	unsigned int size;
	unsigned int count;
	struct map_entry *data;
};

int map_create(struct map *map);
void map_destroy(struct map *map);
void map_clear(struct map *map, void (*release)(struct map_item *));

int map_put(struct map *map, unsigned int key, struct map_item *v);
int map_reput(struct map *map, unsigned int key, struct map_item *v,
		struct map_item **old);
int map_contains(const struct map *map, unsigned int key);
struct map_item *map_get(const struct map *map, unsigned int key);
int map_remove(struct map *map, unsigned int key);
unsigned int map_length(struct map *map);

struct map_entry *map_iter_first(const struct map *map);
struct map_entry *map_iter_next(const struct map *map, struct map_entry *iter);
struct map_item *map_iter_item(struct map_entry *iter);

#define map_for_each(map, iter) \
  for (iter = map_iter_first(map); iter; iter = map_iter_next(map, iter))

#define map_iter_data(iter, type, member) \
  container_of(map_iter_item(iter), type, member)

#endif
