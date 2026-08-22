#include <stdbool.h>
#include <stdint.h>
#include <kernel/lib/ring_buffer.h>
#include <kernel/lib/string.h>
#include <kernel/mem/heap.h>

static inline bool spsc_ring_buffer_full(size_t tail, size_t head, size_t mask)
{
    return ((tail + 1) & mask) == (head & mask);
}

spsc_ring_buffer_t* spsc_ring_buffer_init(size_t capacity, size_t element_size)
{
    if ((capacity & (capacity - 1)) != 0 || capacity == 0 || element_size == 0) return NULL;

    spsc_ring_buffer_t* rb = (spsc_ring_buffer_t*)kmalloc(sizeof(spsc_ring_buffer_t));
    if (rb == NULL) return NULL;

    rb->buffer = kmalloc(element_size * capacity);
    if (rb->buffer == NULL)
    {
        kfree(rb);
        return NULL;
    }

    rb->capacity = capacity;
    rb->element_size = element_size;
    rb->mask = capacity - 1;

    atomic_init(&rb->head, 0);
    atomic_init(&rb->tail, 0);

    return rb;
}

void spsc_ring_buffer_free(spsc_ring_buffer_t* rb)
{
    if (rb == NULL) return;
    if (rb->buffer != NULL)
    {
        kfree(rb->buffer);
    }
    kfree(rb);
}

bool spsc_ring_buffer_push(spsc_ring_buffer_t* rb, const void* data)
{
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);

    if (spsc_ring_buffer_full(tail, head, rb->mask)) return false;

    size_t offset = (tail & rb->mask) * rb->element_size;
    memcpy(rb->buffer + offset, (uint8_t*)data, rb->element_size);

    atomic_store_explicit(&rb->tail, tail + 1, memory_order_release);
    return true;
}

bool spsc_ring_buffer_pop(spsc_ring_buffer_t* rb, void* out_data)
{
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&rb->tail, memory_order_acquire);

    if (head == tail) return false;

    size_t offset = (head & rb->mask) * rb->element_size;
    memcpy(out_data, rb->buffer + offset, rb->element_size);

    atomic_store_explicit(&rb->head, head + 1, memory_order_release);
    return true;
}