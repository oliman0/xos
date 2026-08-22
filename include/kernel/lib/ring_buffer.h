#ifndef XOS_RING_BUFFER_H
#define XOS_RING_BUFFER_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdalign.h>

#define CACHE_LINE_SIZE 64

typedef struct
{
    uint8_t* buffer;
    size_t capacity;
    size_t element_size;
    size_t mask;

    alignas(CACHE_LINE_SIZE) _Atomic size_t head;
    alignas(CACHE_LINE_SIZE) _Atomic size_t tail;
} spsc_ring_buffer_t;

spsc_ring_buffer_t* spsc_ring_buffer_init(size_t capacity, size_t element_size);
void spsc_ring_buffer_free(spsc_ring_buffer_t* rb);

bool spsc_ring_buffer_push(spsc_ring_buffer_t* ring_buffer, const void* data);

bool spsc_ring_buffer_pop(spsc_ring_buffer_t* ring_buffer, void* out_data);

#endif
