#include <stdlib.h>

#include "include/utils.h"
#include "include/map.h"


typedef struct
{
    const void* key;
    void* value;
    map* storage;
}
map_item;

typedef struct
{
    void(*original_receiver)(const void*, void*);
    void* original_params;
}
map_key_receiver_params;

typedef struct
{
    void(*original_receiver)(void*, void*);
    void* original_params;
}
map_value_receiver_params;


static uint64_t modified_hash_function(map_item* item)
{
    return item->storage->hash_function(item->key);
}

static bool modified_comparison_function(map_item* item, map_item* other_item)
{
    if (item->storage != other_item->storage)
    {
        return false;
    }
    return item->storage->comparison_function(item->key, other_item->key);
}

void map_init(map* self, uint64_t(*hash_function)(const void*), bool(*comparison_function)(const void*, const void*))
{
    self->hash_function = hash_function;
    self->comparison_function = comparison_function;
    hash_table_init(
        &self->table, MAP_DEFAULT_ACCURACY,
        (uint64_t(*)(const void*))modified_hash_function,
        (bool(*)(const void*, const void*))modified_comparison_function
    );
}

void map_delete(map* self)
{
    self->hash_function = NULL;
    hash_table_delete(&self->table);
}

void map_set_item(map* self, const void* key, void* value)
{
    map_item* item = check_pointer_after_malloc(malloc(sizeof(map_item)));
    item->key = key;
    item->value = value;
    item->storage = self;
    map_item* existing_item = hash_table_get_item(&self->table, item);
    if (existing_item)
    {
        existing_item->value = value;
        free(item);
    }
    else
    {
        hash_table_insert_item(&self->table, item);
    }
}

bool map_is_present(map* self, const void* key)
{
    map_item item_to_search_for = (map_item){.key = key, .storage = self, .value = NULL};
    return (bool)hash_table_is_present(&self->table, &item_to_search_for);
}

void* map_get_item(map* self, const void* key)
{
    map_item item_to_search_for = (map_item){.key = key, .storage = self, .value = NULL};
    map_item* item_found = hash_table_get_item(&self->table, &item_to_search_for);
    if (item_found)
    {
        return item_found->value;
    }
    return NULL;
    
}

void* map_remove_item(map* self, const void* key)
{
    map_item item_to_remove = (map_item){.key = key, .storage = self, .value = NULL};
    void* result = NULL;
    if (hash_table_is_present(&self->table, &item_to_remove))
    {
        map_item* removed_item = hash_table_remove_item(&self->table, &item_to_remove);
        result = removed_item->value;
        free(removed_item);
    }
    return result;
}

static void get_items(map* self, map_item** dest)
{
    map_item** writer = dest;
    for (uint64_t i = 0; i < self->table.current_size; i++)
    {
        deque_node* current_map_item = self->table.data[i].first;
        while (current_map_item)
        {
            *writer = current_map_item->data;
            writer++;
        }
    }
}

void map_get_keys(map* self, const void** dest)
{
    map_item* items[self->table.items_count];
    get_items(self, items);
    for (uint64_t i = 0; i < self->table.items_count; i++)
    {
        dest[i] = items[i]->key;
    }
}

void map_get_values(map* self, void** dest)
{
    map_item* items[self->table.items_count];
    get_items(self, items);
    for (uint64_t i = 0; i < self->table.items_count; i++)
    {
        dest[i] = items[i]->value;
    }
}

static void map_key_receiver(const map_item* item, map_key_receiver_params* params)
{
    params->original_receiver(item->key, params->original_params);
}

static void map_value_receiver(map_item* item, map_value_receiver_params* params)
{
    params->original_receiver(item->value, params->original_params);
}

void map_iterate_keys(map* self, void(*item_receiver)(const void* item, void* params), void* params)
{
    map_key_receiver_params inner_params = (map_key_receiver_params){
        .original_receiver=item_receiver,
        .original_params=params
    };
    hash_table_const_iterate(&self->table, (void(*)(const void*, void*))map_key_receiver, &inner_params);
}

void map_iterate_values(map* self, void(*item_receiver)(void* item, void* params), void* params)
{
    map_value_receiver_params inner_params = (map_value_receiver_params){
        .original_receiver=item_receiver,
        .original_params=params
    };
    hash_table_iterate(&self->table, (void(*)(void*, void*))map_value_receiver, &inner_params);
}
