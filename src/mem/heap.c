#include <kernel/panic.h>
#include <kernel/mem/heap.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/vmm.h>

static heap_block_t* heap_start = NULL;
static heap_block_t* heap_end = NULL;

static heap_block_t* expand_heap(size_t size)
{
    size_t current_heap_end = (uintptr_t)heap_end + heap_end->size + sizeof(heap_block_t);

    size_t expand_size = size > KERNEL_HEAP_EXPAND_SIZE ? ALIGN_UP(size + sizeof(heap_block_t), 16) : KERNEL_HEAP_EXPAND_SIZE;

    expand_size = ALIGN_UP(expand_size, PAGE_SIZE);

    if (current_heap_end + expand_size > KERNEL_HEAP_BASE + KERNEL_HEAP_SIZE)
    {
        kernel_panic("Kernel Heap Overflow", NULL);
    }

    vmm_alloc_map_range(current_heap_end, expand_size, PAGE_WRITABLE);

    if (heap_end->is_free)
    {
        heap_end->size += expand_size;
    }
    else
    {
        heap_block_t* new_block = (heap_block_t*)current_heap_end;
        new_block->size = expand_size - sizeof(heap_block_t);
        new_block->next = NULL;
        new_block->prev = heap_end;
        new_block->is_free = true;

        heap_end->next = new_block;
        heap_end = new_block;
    }

    return heap_end;
}

void heap_init()
{
    vmm_alloc_map_range(KERNEL_HEAP_BASE, KERNEL_HEAP_EXPAND_SIZE, PAGE_WRITABLE);

    heap_start = (heap_block_t*)KERNEL_HEAP_BASE;
    heap_end = heap_start;

    heap_start->size = KERNEL_HEAP_EXPAND_SIZE - sizeof(heap_block_t);
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->is_free = true;
}

void* kmalloc(size_t size)
{
    if (size == 0) return NULL;

    size = ALIGN_UP(size, 16);

    heap_block_t* current = heap_start;
    while (current != NULL)
    {
        if (current->is_free && current->size >= size)
        {
            // Can we split the block?
            if (current->size >= size + sizeof(heap_block_t) + 16)
            {
                heap_block_t* next_block = (heap_block_t*)((uintptr_t)current + sizeof(heap_block_t) + size);
                next_block->size = current->size - size - sizeof(heap_block_t);
                next_block->is_free = true;
                next_block->next = current->next;
                next_block->prev = current;

                if (current->next)
                {
                    current->next->prev = next_block;
                }
                else
                {
                    heap_end = next_block;
                }

                current->next = next_block;
                current->size = size;
            }

            current->is_free = false;
            return (void*)((uintptr_t)current + sizeof(heap_block_t));
        }

        if (current->next == NULL)
        {
            current = expand_heap(size);
        }
        else
        {
            current = current->next;
        }
    }

    return NULL;
}

void kfree(void* ptr)
{
    if (ptr == NULL) return;

    heap_block_t* block = (heap_block_t*)((uintptr_t)ptr - sizeof(heap_block_t));
    block->is_free = true;

    if (block->next && block->next->is_free)
    {
        block->size += block->next->size + sizeof(heap_block_t);
        block->next = block->next->next;

        if (block->next)
        {
            block->next->prev = block;
        }
    }

    if (block->prev && block->prev->is_free)
    {
        block->prev->size += block->size + sizeof(heap_block_t);
        block->prev->next = block->next;

        if (block->next)
        {
            block->next->prev = block->prev;
        }

        block = block->prev;
    }

    if (block->next == NULL)
    {
        heap_end = block;
    }
}