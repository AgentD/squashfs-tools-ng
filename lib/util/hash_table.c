/*
 * Copyright © 2009,2012 Intel Corporation
 * Copyright © 1988-2004 Keith Packard and Bart Massey.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors
 * or their institutions shall not be used in advertising or
 * otherwise to promote the sale, use or other dealings in this
 * Software without prior written authorization from the
 * authors.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *    Keith Packard <keithp@keithp.com>
 */

/**
 * Implements an open-addressing, linear-reprobing hash table.
 *
 * For more information, see:
 *
 * http://cgit.freedesktop.org/~anholt/hash_table/tree/README
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "fast_urem_by_const.h"
#include "hash_table.h"

#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const sqfs_u32 deleted_key_value;

/**
 * From Knuth -- a good choice for hash/rehash values is p, p-2 where
 * p and p-2 are both prime.  These tables are sized to have an extra 10%
 * free to avoid exponential performance degradation as the hash table fills
 */
static const struct {
   sqfs_u32 max_entries, size, rehash;
   sqfs_u64 size_magic, rehash_magic;
} hash_sizes[] = {
#define ENTRY(max_entries, size, rehash) \
   { max_entries, size, rehash, \
      REMAINDER_MAGIC(size), REMAINDER_MAGIC(rehash) }

   ENTRY(2,            5,            3            ),
   ENTRY(4,            7,            5            ),
   ENTRY(8,            13,           11           ),
   ENTRY(16,           19,           17           ),
   ENTRY(32,           43,           41           ),
   ENTRY(64,           73,           71           ),
   ENTRY(128,          151,          149          ),
   ENTRY(256,          283,          281          ),
   ENTRY(512,          571,          569          ),
   ENTRY(1024,         1153,         1151         ),
   ENTRY(2048,         2269,         2267         ),
   ENTRY(4096,         4519,         4517         ),
   ENTRY(8192,         9013,         9011         ),
   ENTRY(16384,        18043,        18041        ),
   ENTRY(32768,        36109,        36107        ),
   ENTRY(65536,        72091,        72089        ),
   ENTRY(131072,       144409,       144407       ),
   ENTRY(262144,       288361,       288359       ),
   ENTRY(524288,       576883,       576881       ),
   ENTRY(1048576,      1153459,      1153457      ),
   ENTRY(2097152,      2307163,      2307161      ),
   ENTRY(4194304,      4613893,      4613891      ),
   ENTRY(8388608,      9227641,      9227639      ),
   ENTRY(16777216,     18455029,     18455027     ),
   ENTRY(33554432,     36911011,     36911009     ),
   ENTRY(67108864,     73819861,     73819859     ),
   ENTRY(134217728,    147639589,    147639587    ),
   ENTRY(268435456,    295279081,    295279079    ),
   ENTRY(536870912,    590559793,    590559791    ),
   ENTRY(1073741824,   1181116273,   1181116271   ),
   ENTRY(2147483648ul, 2362232233ul, 2362232231ul )
};

static inline bool
key_pointer_is_reserved(const struct hash_table *ht, const void *key)
{
   return key == NULL || key == ht->deleted_key;
}

static int
entry_is_free(const struct hash_entry *entry)
{
   return entry->key == NULL;
}

static int
entry_is_deleted(const struct hash_table *ht, struct hash_entry *entry)
{
   return entry->key == ht->deleted_key;
}

static int
entry_is_present(const struct hash_table *ht, struct hash_entry *entry)
{
   return entry->key != NULL && entry->key != ht->deleted_key;
}

static bool
hash_table_init(struct hash_table *ht,
                sqfs_u32 (*key_hash_function)(void *user, const void *key),
                bool (*key_equals_function)(void *user, const void *a,
                                            const void *b))
{
   ht->size_index = 0;
   ht->size = hash_sizes[ht->size_index].size;
   ht->rehash = hash_sizes[ht->size_index].rehash;
   ht->size_magic = hash_sizes[ht->size_index].size_magic;
   ht->rehash_magic = hash_sizes[ht->size_index].rehash_magic;
   ht->max_entries = hash_sizes[ht->size_index].max_entries;
   ht->key_hash_function = key_hash_function;
   ht->key_equals_function = key_equals_function;
   ht->table = calloc(sizeof(struct hash_entry), ht->size);
   ht->entries = 0;
   ht->deleted_entries = 0;
   ht->deleted_key = &deleted_key_value;

   return ht->table != NULL;
}

struct hash_table *
hash_table_create(sqfs_u32 (*key_hash_function)(void *user, const void *key),
                  bool (*key_equals_function)(void *user, const void *a,
                                              const void *b))
{
   struct hash_table *ht;

   ht = malloc(sizeof(struct hash_table));
   if (ht == NULL)
      return NULL;

   if (!hash_table_init(ht, key_hash_function, key_equals_function)) {
      free(ht);
      return NULL;
   }

   return ht;
}

struct hash_table *
hash_table_clone(struct hash_table *src)
{
   struct hash_table *ht;

   ht = malloc(sizeof(struct hash_table));
   if (ht == NULL)
      return NULL;

   memcpy(ht, src, sizeof(struct hash_table));

   ht->table = calloc(sizeof(struct hash_entry), ht->size);
   if (ht->table == NULL) {
      free(ht);
      return NULL;
   }

   memcpy(ht->table, src->table, ht->size * sizeof(struct hash_entry));

   return ht;
}

/**
 * Frees the given hash table.
 */
void
hash_table_destroy(struct hash_table *ht,
		   void (*delete_function)(struct hash_entry *entry))
{
   if (!ht)
      return;

   if (delete_function) {
      hash_table_foreach(ht, entry) {
         delete_function(entry);
      }
   }
   free(ht->table);
   free(ht);
}

static struct hash_entry *
hash_table_search(struct hash_table *ht, sqfs_u32 hash, const void *key)
{
   assert(!key_pointer_is_reserved(ht, key));

   sqfs_u32 size = ht->size;
   sqfs_u32 start_hash_address = util_fast_urem32(hash, size, ht->size_magic);
   sqfs_u32 double_hash = 1 + util_fast_urem32(hash, ht->rehash,
                                               ht->rehash_magic);
   sqfs_u32 hash_address = start_hash_address;

   do {
      struct hash_entry *entry = ht->table + hash_address;

      if (entry_is_free(entry)) {
         return NULL;
      } else if (entry_is_present(ht, entry) && entry->hash == hash) {
         if (ht->key_equals_function(ht->user, key, entry->key)) {
            return entry;
         }
      }

      hash_address += double_hash;
      if (hash_address >= size)
         hash_address -= size;
   } while (hash_address != start_hash_address);

   return NULL;
}

/**
 * Finds a hash table entry with the given key and hash of that key.
 *
 * Returns NULL if no entry is found.  Note that the data pointer may be
 * modified by the user.
 */
struct hash_entry *
hash_table_search_pre_hashed(struct hash_table *ht, sqfs_u32 hash,
                             const void *key)
{
   assert(ht->key_hash_function == NULL ||
          hash == ht->key_hash_function(ht->user, key));
   return hash_table_search(ht, hash, key);
}

static void
hash_table_insert_rehash(struct hash_table *ht, sqfs_u32 hash,
                         const void *key, void *data)
{
   sqfs_u32 size = ht->size;
   sqfs_u32 start_hash_address = util_fast_urem32(hash, size, ht->size_magic);
   sqfs_u32 double_hash = 1 + util_fast_urem32(hash, ht->rehash,
                                               ht->rehash_magic);
   sqfs_u32 hash_address = start_hash_address;
   do {
      struct hash_entry *entry = ht->table + hash_address;

      if (entry->key == NULL) {
         entry->hash = hash;
         entry->key = key;
         entry->data = data;
         return;
      }

      hash_address += double_hash;
      if (hash_address >= size)
         hash_address -= size;
   } while (true);
}

static void
hash_table_rehash(struct hash_table *ht, unsigned new_size_index)
{
   struct hash_table old_ht;
   struct hash_entry *table;

   if (new_size_index >= ARRAY_SIZE(hash_sizes))
      return;

   table = calloc(sizeof(struct hash_entry), hash_sizes[new_size_index].size);
   if (table == NULL)
      return;

   old_ht = *ht;

   ht->table = table;
   ht->size_index = new_size_index;
   ht->size = hash_sizes[ht->size_index].size;
   ht->rehash = hash_sizes[ht->size_index].rehash;
   ht->size_magic = hash_sizes[ht->size_index].size_magic;
   ht->rehash_magic = hash_sizes[ht->size_index].rehash_magic;
   ht->max_entries = hash_sizes[ht->size_index].max_entries;
   ht->entries = 0;
   ht->deleted_entries = 0;

   hash_table_foreach(&old_ht, entry) {
      hash_table_insert_rehash(ht, entry->hash, entry->key, entry->data);
   }

   ht->entries = old_ht.entries;

   free(old_ht.table);
}

static struct hash_entry *
hash_table_insert(struct hash_table *ht, sqfs_u32 hash,
                  const void *key, void *data)
{
   struct hash_entry *available_entry = NULL;

   assert(!key_pointer_is_reserved(ht, key));

   if (ht->entries >= ht->max_entries) {
      hash_table_rehash(ht, ht->size_index + 1);
   } else if (ht->deleted_entries + ht->entries >= ht->max_entries) {
      hash_table_rehash(ht, ht->size_index);
   }

   sqfs_u32 size = ht->size;
   sqfs_u32 start_hash_address = util_fast_urem32(hash, size, ht->size_magic);
   sqfs_u32 double_hash = 1 + util_fast_urem32(hash, ht->rehash,
                                               ht->rehash_magic);
   sqfs_u32 hash_address = start_hash_address;
   do {
      struct hash_entry *entry = ht->table + hash_address;

      if (!entry_is_present(ht, entry)) {
         /* Stash the first available entry we find */
         if (available_entry == NULL)
            available_entry = entry;
         if (entry_is_free(entry))
            break;
      }

      /* Implement replacement when another insert happens
       * with a matching key.  This is a relatively common
       * feature of hash tables, with the alternative
       * generally being "insert the new value as well, and
       * return it first when the key is searched for".
       *
       * Note that the hash table doesn't have a delete
       * callback.  If freeing of old data pointers is
       * required to avoid memory leaks, perform a search
       * before inserting.
       */
      if (!entry_is_deleted(ht, entry) &&
          entry->hash == hash &&
          ht->key_equals_function(ht->user, key, entry->key)) {
         entry->key = key;
         entry->data = data;
         return entry;
      }

      hash_address += double_hash;
      if (hash_address >= size)
         hash_address -= size;
   } while (hash_address != start_hash_address);

   if (available_entry) {
      if (entry_is_deleted(ht, available_entry))
         ht->deleted_entries--;
      available_entry->hash = hash;
      available_entry->key = key;
      available_entry->data = data;
      ht->entries++;
      return available_entry;
   }

   /* We could hit here if a required resize failed. An unchecked-malloc
    * application could ignore this result.
    */
   return NULL;
}

/**
 * Inserts the key with the given hash into the table.
 *
 * Note that insertion may rearrange the table on a resize or rehash,
 * so previously found hash_entries are no longer valid after this function.
 */
struct hash_entry *
hash_table_insert_pre_hashed(struct hash_table *ht, sqfs_u32 hash,
                             const void *key, void *data)
{
   assert(ht->key_hash_function == NULL ||
          hash == ht->key_hash_function(ht->user, key));
   return hash_table_insert(ht, hash, key, data);
}

/**
 * This function is an iterator over the hash table.
 *
 * Pass in NULL for the first entry, as in the start of a for loop.  Note that
 * an iteration over the table is O(table_size) not O(entries).
 */
struct hash_entry *
hash_table_next_entry(struct hash_table *ht,
                      struct hash_entry *entry)
{
   if (entry == NULL)
      entry = ht->table;
   else
      entry = entry + 1;

   for (; entry != ht->table + ht->size; entry++) {
      if (entry_is_present(ht, entry)) {
         return entry;
      }
   }

   return NULL;
}
