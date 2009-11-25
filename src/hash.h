#ifndef HASH_H
#define HASH_H

#include <glib.h>

void ns_hash_table_insert(GHashTable *table, int id, const char *name);
void ns_hash_table_free(gpointer key, gpointer value, gpointer user_data);
gboolean ns_hash_table_check_last_seen(gpointer key, gpointer value, gpointer user_data);

void ns_hash_table_print(GHashTable *table);

#endif
