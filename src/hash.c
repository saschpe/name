#include "hash.h"
#include "name.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *
 */
void hash_table_insert(GHashTable *table, int id, const char *name)
{
    client_info_t *info = (client_info_t *)malloc(sizeof(client_info_t));

    if (info != NULL) {
        // This stupid bullshit is necessary because glib hash tables don't make
        // copies of provided keys and values, instead store pointers.
        int *key = (int *)malloc(sizeof(int));
        *key = id;
        strncpy(info->name, name, strlen(name));
        info->last_hello = get_time();
        g_hash_table_insert(table, key, info);
        printf("Inserted key '%d' with name value '%s' in hash table.\n", *key, info->name);
    } else {
        fprintf(stderr, "Error: Unable to allocate memory for client info!\n");
    }
}

/**
 *
 */
void hash_table_free(gpointer key, gpointer value, gpointer user_data)
{
    g_free(key);
    g_free(value);
}

