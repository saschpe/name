#ifndef HASH_H
#define HASH_H

#include <glib.h>

void ns_hash_table_insert(GHashTable *table, int id, const char *name);
void ns_hash_table_free(gpointer key, gpointer value, gpointer user_data);

#endif
