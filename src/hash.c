#include "hash.h"
#include "name.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Convenience wrapper that makes dynamically allocated copies of an
 * id/name pair, constructs a ns_peer_t object and inserts the result
 * into a GHashTable.
 */
void ns_hash_table_insert(GHashTable *table, unsigned short id, const char *name)
{
    ns_peer_t *info = (ns_peer_t *)malloc(sizeof(ns_peer_t));
    memset(info, 0, sizeof(ns_peer_t));

    if (info != NULL) {
        unsigned short *key = (unsigned short *)malloc(sizeof(unsigned short));
        *key = id;
        strncpy(info->name, name, strlen(name));
        info->last_hello = get_time();
        g_hash_table_insert(table, key, info);
        printf("   Inserted key '%d' with name '%s'.\n", *key, info->name);
        ns_hash_table_print(table);
    } else {
        fprintf(stderr, "Error: Unable to allocate memory for client info!\n");
    }
}

/**
 * Frees the dynamically allocated memory of a key/value pair in a GHashTable
 */
void ns_hash_table_free(gpointer key, gpointer value, gpointer user_data)
{
    g_free(key);
    g_free(value);
}

/**
 * Checks the the time when the last HELLO message was received from another
 * peer. Returns TRUE if that time was longer than a default time difference
 * so that this peer can be removed from a GHashTable of peers.
 */
gboolean ns_hash_table_check_last_seen(gpointer key, gpointer value, gpointer user_data)
{
    ns_peer_t *info = (ns_peer_t *)value;

    // Difference is current time minus last time seen
    time_val diff = *(time_val *)user_data - info->last_hello;
    if (diff > NS_HELLO_LAST_TIME_DIFFERENCE) {
        printf("   Missing HELLO from '%d', remove from list\n", *(unsigned short *)key);
        return TRUE;
    }
    return FALSE;
}

static void print_key_value(gpointer key, gpointer value, gpointer user_data)
{
    ns_peer_t *info = (ns_peer_t *)value;
    printf("%d:(%s,%lld), ", *(unsigned short *)key, info->name, info->last_hello);
}

/**
 * Prints a GHashTable with ns_peer_t entries.
 */
void ns_hash_table_print(GHashTable *table)
{
    printf("   hash_table: [");
    g_hash_table_foreach(table, print_key_value, NULL);
    printf("]\n");
}
