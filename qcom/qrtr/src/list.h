#ifndef _LIST_H_
#define _LIST_H_

#ifndef offsetof
#define offsetof(type, md) ((unsigned long)&((type *)0)->md)
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
  ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

struct list_item {
	struct list_item *next;
	struct list_item *prev;
};

struct list {
	struct list_item *head;
	struct list_item *tail;
};

#define LIST_INIT(name) { 0, 0 }

#define LIST(name) \
	struct list name = LIST_INIT(name)

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

static inline void list_init(struct list *list)
{
	list->head = 0;
	list->tail = 0;
}

static inline void list_append(struct list *list, struct list_item *item)
{
	item->next = 0;
	item->prev = list->tail;
	if (list->tail != 0)
		list->tail->next = item;
	else
		list->head = item;
	list->tail = item;
}

static inline void list_prepend(struct list *list, struct list_item *item)
{
	item->prev = 0;
	item->next = list->head;
	if (list->head == 0)
		list->tail = item;
	list->head = item;
}

static inline void list_insert(struct list *list, struct list_item *after, struct list_item *item)
{
	if (after == 0) {
		list_prepend(list, item);
		return;
	}
	item->prev = after;
	item->next = after->next;
	after->next = item;
	if (item->next)
		item->next->prev = item;
	if (list->tail == after)
		list->tail = item;
}

static inline void list_remove(struct list *list, struct list_item *item)
{
	if (item->next)
		item->next->prev = item->prev;
	if (list->head == item) {
		list->head = item->next;
		if (list->head == 0)
			list->tail = 0;
	} else {
		item->prev->next = item->next;
		if (list->tail == item)
			list->tail = item->prev;
	}
	item->prev = item->next = 0;
}

static inline struct list_item *list_pop(struct list *list)
{
	struct list_item *item;
	item = list->head;
	if (item == 0)
		return 0;
	list_remove(list, item);
	return item;
}

static inline struct list_item *list_last(struct list *list)
{
	return list->tail;
}

static inline struct list_item *list_first(struct list *list)
{
	return list->head;
}


static inline struct list_item *list_next(struct list_item *item)
{
	return item->next;
}

#define list_push list_append

#define list_for_each(_list, _iter) \
  for (_iter = (_list)->head; (_iter) != 0; _iter = (_iter)->next)

#define list_for_each_after(_node, _iter) \
  for (_iter = (_node)->next; (_iter) != 0; _iter = (_iter)->next)

#define list_for_each_safe(_list, _iter, _bkup) \
  for (_iter = (_list)->head; (_iter) != 0 && ((_bkup = (_iter)->next) || 1); _iter = (_bkup))

#define list_for_each_safe_after(_node, _iter, _bkup) \
  for (_iter = (_node)->next; (_iter) != 0 && ((_bkup = (_iter)->next) || 1); _iter = (_bkup))

#endif
