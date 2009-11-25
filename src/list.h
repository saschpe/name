#ifndef LIST_H
#define LIST_H

typedef struct list_item
{
    void *_data;
    struct list_item *_next;
} list_item_t;

typedef struct list
{
    int _size;
    struct list_item *_head;
} list_t;

list_t *list_new();
int list_size(list_t *list);
void list_delete(list_t *list);

list_item_t *list_append(list_t *list, void *data);
int list_remove(list_t *list, void *data);
int list_remove(list_t *list, int index);
int list_index(list_t *list, void *data);

void list_print(list_t *list, void (*list_item_data_print_func)(void *data));

#endif
