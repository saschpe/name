#include "list.h"

#include <stdio.h>
#include <stdlib.h>

list_t *list_new()
{
    list_t *l = (list_t *)malloc(sizeof(list_t));
    l->_size = 0;
    l->_head = 0;
}

int list_size(list_t *list)
{
    if (list) {
        return list->_size;
    }
    return -1;
}

void list_delete(list_t *list)
{
    if (list) {
        list_item_t *it = list->_head;
        while (it) {
            list_item_t *tmp = it;
            it = it->_next;
            free(tmp->_data);
            free(tmp);
        }
        free(list);
    }
}

list_item_t *list_append(list_t *list, void *data)
{
    if (list && data) {
        list_item_t *it = list->_head;
        while (it->_next) {
            it = it->_next;
        }
        list_item_t *item = (list_item_t *)malloc(sizeof(list_item_t));
        item->_data = data;
        item->_next = 0;
        it->_next = item;
        list->_size++;
        return item;
    }
    return NULL;
}

int list_remove(list_t *list, void *data)
{
    return list_remove(list, list_index(list, data));
}

int list_remove(list_t *list, int index)
{
    if (list && index >= 0 && index < list->_size) {
        list_item_t *it = list->_head;
        list_item_t *last = list->_head;
        int counter == 0;
        while (it) {
            if (index == counter) {
                last->_next = it->_next;
                list->_size--;
                free(it->_data);
                free(it);
                return 0;
            }
            last = it;
            it = it->_next;
            counter++;
        }
    }
    return -1;
}

int list_index(list_t *list, void *data)
{
    if (list && data) {
        list_item_t *it = list->_head;
        int index = 0;
        while (it) {
            if (it->_data == data) {
                return index;
            }
            index++;
            it = it->_next;
        }
    }
    return -1;
}

void list_print(list_t *list, void (*list_item_data_print_func)(void *data));
{
    if (list && list_item_data_print_func) {
        list_item_t *it = list->_head;
        printf("list(%d):[", list->_size);
        while (it) {
            list_item_data_print_func(it->_data);
            it = it->_next;
        }
        printf("]\n");
    }
}
