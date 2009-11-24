#include "hash.h"
#include "name.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *
 */
void ns_hash_table_insert(GHashTable *table, int id, const char *name)
{
    ns_peer_t *info = (ns_peer_t *)malloc(sizeof(ns_peer_t));

    if (info != NULL) {
        // This stupid bullshit is necessary because glib ns_hash tables don't make
        // copies of provided keys and values, instead store pointers.
        int *key = (int *)malloc(sizeof(int));
        *key = id;
        strncpy(info->name, name, strlen(name));
        info->last_hello = get_time();
        g_hash_table_insert(table, key, info);
        printf("Inserted key '%d' with name value '%s' in ns_hash table.\n", *key, info->name);
        ns_hash_table_print(table);
    } else {
        fprintf(stderr, "Error: Unable to allocate memory for client info!\n");
    }
}

/**
 *
 */
void ns_hash_table_free(gpointer key, gpointer value, gpointer user_data)
{
    g_free(key);
    g_free(value);
}

static void print_key_value(gpointer key, gpointer value, gpointer user_data)
{
    printf("%d:%s, ", *(int *)key, value);
}

void ns_hash_table_print(GHashTable *table)
{
    printf("[");
    g_hash_table_foreach(table, print_key_value, NULL);
    printf("]\n");
}
