#include <stdlib.h>

#include "include/encoder.h"


void encoder_init(encoder* self, void(*writer)(const uint8_t*, uint32_t, void*), void* writer_params)
{
    adaptive_tree_init(&self->base.tree);
    bit_buffer_init(&self->output_buffer);
    self->base.writer_params = writer_params;
    self->base.writer = writer;

    self->base.vtbl.deinit = encoder_delete;
    self->base.vtbl.flush = encoder_flush;
    self->base.vtbl.write = encoder_write;
}

void encoder_delete(coder* self, bool free_params)
{
    if (free_params)
    {
        free(self->writer_params);
    }
    self->writer_params = NULL;
    self->writer = NULL;
    adaptive_tree_delete(&self->tree);
    bit_buffer_delete(&((encoder*)self)->output_buffer);
}

static void flush_full_items(encoder* self)
{
    dynamic_array* full_data = bit_buffer_get_full_part(&self->output_buffer, false);
    self->base.writer((uint8_t*)&full_data->first_item, full_data->length * sizeof(uint64_t), self->base.writer_params);
    array_clear(full_data);
}

void encoder_write(coder* self, const uint8_t* data, uint32_t length)
{
    bit_buffer path_holder;
    bit_buffer_init(&path_holder);
    for (uint32_t i = 0; i < length; i++)
    {
        adaptive_node* current_node = map_get_item(&self->tree.leaves_map, (void*)(uint64_t)data[i]);
        if (current_node)
        {
            adaptive_node_get_path(current_node, &path_holder);
            bit_buffer_extend(&((encoder*)self)->output_buffer, &path_holder);
            bit_buffer_clear(&path_holder);
        }
        else
        {
            adaptive_node_get_path(self->tree.nyt_node, &path_holder);
            bit_buffer_extend(&((encoder*)self)->output_buffer, &path_holder);
            bit_buffer_clear(&path_holder);
            bit_buffer_push_byte(&((encoder*)self)->output_buffer, data[i]);
        }
        adaptive_tree_update(&self->tree, data[i]);
        if (bit_buffer_get_size(&((encoder*)self)->output_buffer) >= ENCODER_FLUSH_ON_BITS)
        {
            flush_full_items((encoder*)self);
        }
    }
    bit_buffer_delete(&path_holder);
}

void encoder_flush(coder* self)
{
    flush_full_items((encoder*)self);
    uint64_t buffer_size = bit_buffer_get_size(&((encoder*)self)->output_buffer);
    uint8_t full_bytes = buffer_size / 8;
    uint8_t bits_left = buffer_size % 8;
    uint8_t bytes_to_write_count = bits_left? full_bytes + 1: full_bytes;
    uint8_t bytes_to_write[bytes_to_write_count];
    for (uint8_t i = 0; i < bytes_to_write_count; i++)
    {
        bytes_to_write[i] = ((uint8_t*)(&((encoder*)self)->output_buffer.last_item))[i];
    }
    if (bits_left)
    {
        bit_buffer tmp_buffer;
        bit_buffer_init(&tmp_buffer);
        for (uint8_t i = 0; i < bits_left; i++)
        {
            bit_buffer_push_bit(&tmp_buffer, (bytes_to_write[bytes_to_write_count - 1] >> i) & 1);
        }
        bit_buffer path_holder;
        bit_buffer_init(&path_holder);
        adaptive_node_get_path(self->tree.nyt_node, &path_holder);
        bit_buffer_extend(&tmp_buffer, &path_holder);
        bytes_to_write[bytes_to_write_count - 1] = (uint8_t)tmp_buffer.last_item;
        bit_buffer_delete(&path_holder);
        bit_buffer_delete(&tmp_buffer);
    }
    self->writer(bytes_to_write, bytes_to_write_count, self->writer_params);
    bit_buffer_clear(&((encoder*)self)->output_buffer);
}
